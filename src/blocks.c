/**
 * Block parsing implementation.
 *
 * For a high-level overview of the block parsing process,
 * see http://spec.commonmark.org/0.24/#phase-1-block-structure
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmark_ctype.h"
#include "syntax_extension.h"
#include "cmark-gfm_config.h"
#include "parser.h"
#include "cmark-gfm.h"
#include "node.h"
#include "references.h"
#include "utf8.h"
#include "scanners.h"
#include "inlines.h"
#include "houdini.h"
#include "string_token.h"
#include "buffer.h"
#include "footnotes.h"

#define CODE_INDENT 4
#define TAB_STOP 4

/**
 * Very deeply nested lists can cause quadratic performance issues.
 * This constant is used in open_new_blocks() to limit the nesting
 * depth. It is unlikely that a non-contrived markdown document will
 * be nested this deeply.
 */
#define MAX_LIST_DEPTH 100

#ifndef MIN
#define MIN(x, y) ((x < y) ? x : y)
#endif

#define peek_at(i, n) (i)->data[n]

static bool S_last_line_blank(const cmark_node *node) {
  return (node->flags & CMARK_NODE__LAST_LINE_BLANK) != 0;
}

static bool S_last_line_checked(const cmark_node *node) {
  return (node->flags & CMARK_NODE__LAST_LINE_CHECKED) != 0;
}

static inline cmark_node_type S_type(const cmark_node *node) {
  return (cmark_node_type)node->type;
}

static void S_set_last_line_blank(cmark_node *node, bool is_blank) {
  if (is_blank)
    node->flags |= CMARK_NODE__LAST_LINE_BLANK;
  else
    node->flags &= ~CMARK_NODE__LAST_LINE_BLANK;
}

static void S_set_last_line_checked(cmark_node *node) {
  node->flags |= CMARK_NODE__LAST_LINE_CHECKED;
}

static inline bool S_is_line_end_char(char c) {
  return (c == '\n' || c == '\r');
}

static inline bool S_is_space_or_tab(char c) {
  return (c == ' ' || c == '\t');
}

static void S_parser_feed(cmark_parser *parser, const unsigned char *buffer,
                          size_t len, bool eof);

static void S_process_line(cmark_parser *parser, const unsigned char *buffer,
                           bufsize_t bytes, bool ensureEndsInNewline);

static cmark_node *make_block(cmark_mem *mem, cmark_node_type tag,
                              int start_line, int start_column) {
  cmark_node *e;

  e = (cmark_node *)mem->calloc(1, sizeof(*e));
  cmark_strbuf_init(mem, &e->content, 32);
  e->type = (uint16_t)tag;
  e->flags = CMARK_NODE__OPEN;
  e->start_line = start_line;
  e->start_column = start_column;
  e->end_line = start_line;

  return e;
}

// Create a root document node.
static cmark_node *make_document(cmark_mem *mem) {
  cmark_node *e = make_block(mem, CMARK_NODE_DOCUMENT, 1, 1);
  return e;
}

int cmark_parser_attach_syntax_extension(cmark_parser *parser,
                                         cmark_syntax_extension *extension) {
  parser->syntax_extensions = cmark_llist_append(parser->mem, parser->syntax_extensions, extension);
  if (extension->match_inline || extension->insert_inline_from_delim) {
    if (!parser->inline_syntax_extensions) {
      // if we're loading an inline extension into this parser for the first time,
      // allocate new buffers for the inline parser character arrays
      parser->skip_chars = (int8_t *)parser->mem->calloc(sizeof(int8_t), 256);
      cmark_set_default_skip_chars(&parser->skip_chars, true);

      parser->special_chars = (int8_t *)parser->mem->calloc(sizeof(int8_t), 256);
      cmark_set_default_special_chars(&parser->special_chars, true);
    }

    parser->inline_syntax_extensions = cmark_llist_append(
      parser->mem, parser->inline_syntax_extensions, extension);
  }

  return 1;
}

static void cmark_parser_dispose(cmark_parser *parser) {
  cmark_list_spread_free(parser);
  if (parser->root)
    cmark_node_free(parser->root);

  if (parser->refmap)
    cmark_map_free(parser->refmap);
}

static void cmark_parser_reset(cmark_parser *parser) {
  cmark_llist *saved_exts = parser->syntax_extensions;
  cmark_llist *saved_inline_exts = parser->inline_syntax_extensions;
  int saved_options = parser->options;
  cmark_mem *saved_mem = parser->mem;
  int8_t *saved_specials = parser->special_chars;
  int8_t *saved_skips = parser->skip_chars;

  cmark_parser_dispose(parser);

  memset(parser, 0, sizeof(cmark_parser));
  parser->mem = saved_mem;

  cmark_strbuf_init(parser->mem, &parser->curline, 256);
  cmark_strbuf_init(parser->mem, &parser->linebuf, 0);

  cmark_node *document = make_document(parser->mem);

  if (saved_options & CMARK_OPT_REFERENCE_USAGES) saved_options |= CMARK_OPT_REFERENCE_DEFINITIONS;
  parser->refmap = cmark_reference_map_new(parser->mem);
  parser->refmap->web_reference_labels = (saved_options & CMARK_OPT_REFERENCE_USAGES) != 0;
  parser->refmap->web_character_references = (saved_options & CMARK_OPT_WEB_CHARACTER_REFERENCES) != 0;
  parser->root = document;
  parser->current = document;

  parser->syntax_extensions = saved_exts;
  parser->inline_syntax_extensions = saved_inline_exts;
  parser->options = saved_options;

  parser->special_chars = saved_specials;
  parser->skip_chars = saved_skips;
}

cmark_parser *cmark_parser_new_with_mem(int options, cmark_mem *mem) {
  cmark_parser *parser = (cmark_parser *)mem->calloc(1, sizeof(cmark_parser));
  parser->mem = mem;
  parser->options = options;
  cmark_set_default_skip_chars(&parser->skip_chars, false);
  cmark_set_default_special_chars(&parser->special_chars, false);
  cmark_parser_reset(parser);
  return parser;
}

cmark_parser *cmark_parser_new(int options) {
  extern cmark_mem CMARK_DEFAULT_MEM_ALLOCATOR;
  return cmark_parser_new_with_mem(options, &CMARK_DEFAULT_MEM_ALLOCATOR);
}

void cmark_parser_free(cmark_parser *parser) {
  cmark_mem *mem = parser->mem;

  // If any inline syntax extensions were added, free the memory allocated for the special-chars arrays
  if (parser->inline_syntax_extensions) {
    mem->free(parser->special_chars);
    mem->free(parser->skip_chars);
  }

  cmark_parser_dispose(parser);
  cmark_strbuf_free(&parser->curline);
  cmark_strbuf_free(&parser->linebuf);
  cmark_llist_free(parser->mem, parser->syntax_extensions);
  cmark_llist_free(parser->mem, parser->inline_syntax_extensions);
  mem->free(parser);
}

static cmark_node *finalize(cmark_parser *parser, cmark_node *b);

// Returns true if line has only space characters, else false.
static bool is_blank(cmark_strbuf *s, bufsize_t offset) {
  while (offset < s->size) {
    switch (s->ptr[offset]) {
    case '\r':
    case '\n':
      return true;
    case ' ':
      offset++;
      break;
    case '\t':
      offset++;
      break;
    default:
      return false;
    }
  }

  return true;
}

static inline bool accepts_lines(cmark_node_type block_type) {
  return (block_type == CMARK_NODE_PARAGRAPH ||
          block_type == CMARK_NODE_HEADING ||
          block_type == CMARK_NODE_CODE_BLOCK || block_type == CMARK_NODE_MATH_BLOCK);
}

static inline bool contains_inlines(cmark_node *node) {
  if (node->extension && node->extension->contains_inlines_func) {
    return node->extension->contains_inlines_func(node->extension, node) != 0;
  }

  return (node->type == CMARK_NODE_DEFINITION_TERM ||
          node->type == CMARK_NODE_PARAGRAPH ||
          node->type == CMARK_NODE_HEADING);
}

static void S_advance_offset(cmark_parser *parser, cmark_chunk *input, bufsize_t count, bool columns);

static void add_line(cmark_node *node, cmark_chunk *ch, cmark_parser *parser) {
  int chars_to_tab;
  int i;
  bufsize_t code_source_start = node->content.size;
  assert(node->flags & CMARK_NODE__OPEN);
  if ((parser->options & CMARK_OPT_DEFINITION_LISTS) && node->type == CMARK_NODE_PARAGRAPH &&
      node->content.size > 0 && parser->column > 0 && node->parent &&
      (node->parent->type == CMARK_NODE_BLOCK_QUOTE || node->parent->type == CMARK_NODE_DEFINITION_DESCRIPTION)) {
    node->definition.flow_offset = node->content.size;
    node->definition.flow_line = parser->line_number;
    node->definition.flow_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
  }
  if (parser->partially_consumed_tab) {
    parser->offset += 1; // skip over tab
    // add space characters:
    chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);
    for (i = 0; i < chars_to_tab; i++) {
      cmark_strbuf_putc(&node->content, ' ');
    }
  }
  if ((parser->options & (CMARK_OPT_MATH | CMARK_OPT_REFERENCE_DEFINITIONS | CMARK_OPT_WEB_CHARACTER_REFERENCES | CMARK_OPT_CODE_METADATA | CMARK_OPT_SOFTBREAK_METADATA)) &&
      (node->type == CMARK_NODE_PARAGRAPH || node->type == CMARK_NODE_HEADING)) {
    if (node->math_line_count == node->math_line_capacity) {
      node->math_line_capacity = node->math_line_capacity ? node->math_line_capacity * 2 : 8;
      node->math_lines = parser->mem->realloc(node->math_lines, node->math_line_capacity * sizeof(cmark_math_line));
    }
    cmark_math_line *line = &node->math_lines[node->math_line_count++];
    line->offset = node->content.size;
    line->line = parser->line_number;
    line->column = parser->offset + 1;
    line->ending = parser->math_current_ending;
    int prefix = parser->math_inline_prefix;
    line->visual_column = cmark_reference_column(1, ch->data, parser->offset);
    line->prefix_visual_column = cmark_reference_column(1, ch->data, parser->offset - prefix);
    line->prefix = cmark_chunk_dup(ch, parser->offset - prefix, prefix);
    // Own the prefix: the parser's current-line storage is reused next line.
    cmark_chunk_to_cstr(parser->mem, &line->prefix);
    parser->math_eol_target = node;
  }
  cmark_strbuf_put(&node->content, ch->data + parser->offset,
                   ch->len - parser->offset);
  if (node->type == CMARK_NODE_MATH_BLOCK && node->content.size && parser->math_current_ending != 0) {
    cmark_strbuf_truncate(&node->content, node->content.size - 1);
    if (parser->math_current_ending != 10) cmark_strbuf_putc(&node->content, '\r');
    if (parser->math_current_ending != 13) cmark_strbuf_putc(&node->content, '\n');
    parser->math_eol_target = node;
  }

  if (node->type == CMARK_NODE_CODE_BLOCK && node->code_metadata) {
    cmark_code_metadata *data = node->code_metadata;
    cmark_strbuf line = CMARK_BUF_INIT(parser->mem);
    if (parser->blank && parser->code_footnote_blank_offset >= 0) {
      /* GFM footnote blankLine is a check, not an indent token. Use the real
       * blank-line bytes before the legacy footnote prefix consumed them. */
      cmark_parser cursor = *parser;
      cursor.offset = parser->code_footnote_blank_offset;
      cursor.column = parser->code_footnote_blank_column;
      cursor.partially_consumed_tab = parser->code_footnote_blank_partial;
      /* Replay only the real open-container whitespace prefixes. GFM
       * footnote blankLine is non-consuming; list/definition/code prefixes
       * still consume their token widths, including virtual tab spaces. */
      size_t count = 0;
      cmark_node *owner = node;
      while (owner && owner != parser->code_footnote_blank_container) { count++; owner = owner->parent; }
      if (owner) {
        cmark_node **path = parser->mem->calloc(count, sizeof(cmark_node *));
        owner = node;
        for (size_t j = count; j > 0; j--) { path[j - 1] = owner; owner = owner->parent; }
        for (size_t j = 0; j < count; j++) {
          int width = 0;
          if (path[j]->type == CMARK_NODE_ITEM) width = path[j]->as.list.marker_offset + path[j]->as.list.padding;
          else if (path[j]->type == CMARK_NODE_DEFINITION_DESCRIPTION) width = path[j]->definition.padding;
          else if (path[j] == node) width = data->kind == 2 ? CODE_INDENT : node->as.code.fence_offset;
          int consumed = 0;
          while (consumed < width && S_is_space_or_tab(peek_at(ch, cursor.offset))) { S_advance_offset(&cursor, ch, 1, true); consumed++; }
          if (path[j] == node && data->kind == 2) data->indented_qualified = consumed == CODE_INDENT;
        }
        parser->mem->free(path);
      }
      if (cursor.partially_consumed_tab) {
        cursor.offset++;
        for (int j = TAB_STOP - (cursor.column % TAB_STOP); j > 0; j--) cmark_strbuf_putc(&line, ' ');
      }
      cmark_strbuf_put(&line, ch->data + cursor.offset, ch->len - cursor.offset);
    } else cmark_strbuf_put(&line, node->content.ptr + code_source_start, node->content.size - code_source_start);
    if (line.size && line.ptr[line.size - 1] == '\n') cmark_strbuf_truncate(&line, line.size - 1);
    cmark_strbuf_put(&data->source, line.ptr, line.size);
    if (data->kind == 2 && data->indented_qualified) {
      cmark_strbuf_put(&data->compiled, data->pending.ptr, data->pending.size);
      cmark_strbuf_clear(&data->pending);
      cmark_strbuf_put(&data->compiled, line.ptr, line.size);
    }
    if (parser->math_current_ending == 13 || parser->math_current_ending == 0x0d0a) {
      cmark_strbuf_putc(&data->source, '\r');
      if (data->kind == 2) cmark_strbuf_putc(&data->pending, '\r');
    }
    if (parser->math_current_ending == 10 || parser->math_current_ending == 0x0d0a) {
      cmark_strbuf_putc(&data->source, '\n');
      if (data->kind == 2) cmark_strbuf_putc(&data->pending, '\n');
    }
    cmark_strbuf_free(&line);
    parser->math_eol_target = node;
  }
}

static void remove_trailing_blank_lines(cmark_strbuf *ln) {
  bufsize_t i;
  unsigned char c;

  for (i = ln->size - 1; i >= 0; --i) {
    c = ln->ptr[i];

    if (c != ' ' && c != '\t' && !S_is_line_end_char(c))
      break;
  }

  if (i < 0) {
    cmark_strbuf_clear(ln);
    return;
  }

  for (; i < ln->size; ++i) {
    c = ln->ptr[i];

    if (!S_is_line_end_char(c))
      continue;

    cmark_strbuf_truncate(ln, i);
    break;
  }
}

// Check to see if a node ends with a blank line, descending
// if needed into lists and sublists.
static bool S_ends_with_blank_line(cmark_node *node) {
  if (S_last_line_checked(node)) {
    return(S_last_line_blank(node));
  } else if ((S_type(node) == CMARK_NODE_LIST ||
              S_type(node) == CMARK_NODE_ITEM) && node->last_child) {
    S_set_last_line_checked(node);
    return(S_ends_with_blank_line(node->last_child));
  } else {
    S_set_last_line_checked(node);
    return (S_last_line_blank(node));
  }
}

// returns true if content remains after link defs are resolved.
static bool resolve_reference_link_definitions(
		cmark_parser *parser,
                cmark_node *b) {
  bufsize_t pos;
  cmark_strbuf *node_content = &b->content;
  cmark_chunk chunk = {node_content->ptr, node_content->size, 0};
  for (;;) {
    cmark_node *definition = NULL;
    bufsize_t indent = 0;
    if (parser->options & CMARK_OPT_REFERENCE_DEFINITIONS) {
      while (indent < chunk.len && (chunk.data[indent] == ' ' || chunk.data[indent] == '\t')) indent++;
    }
    if (chunk.len > indent && chunk.data[indent] == '[') {
      cmark_chunk candidate = {chunk.data + indent, chunk.len - indent, 0};
      pos = (parser->options & CMARK_OPT_REFERENCE_DEFINITIONS) ?
          cmark_parse_reference_definition(parser->mem, &candidate, parser->refmap, b,
                                          node_content->size - chunk.len + indent, &definition) :
          cmark_parse_reference_inline(parser->mem, &chunk, parser->refmap);
      if (pos) pos += indent;
    } else if (!(parser->options & CMARK_OPT_REFERENCE_USAGES) && chunk.len > 1 && chunk.data[0] == '^' && chunk.data[1] == '[') {
      pos = cmark_parse_reference_attributes_inline(parser->mem, &chunk, parser->refmap);
    } else pos = 0;
    if (!pos) break;
    if (definition && !cmark_node_insert_before(b, definition)) cmark_node_free(definition);
    chunk.data += pos;
    chunk.len -= pos;
  }
  if ((parser->options & CMARK_OPT_REFERENCE_DEFINITIONS) && node_content->size > chunk.len) {
    // Remaining flow content starts a new logical paragraph after the definitions.
    while (chunk.len && (chunk.data[0] == ' ' || chunk.data[0] == '\t')) { chunk.data++; chunk.len--; }
  }
  if ((parser->options & (CMARK_OPT_DEFINITION_LISTS | CMARK_OPT_REFERENCE_DEFINITIONS)) && node_content->size > chunk.len) {
    for (bufsize_t i = 0; i < node_content->size - chunk.len; i++) {
      if (node_content->ptr[i] == '\n' || (node_content->ptr[i] == '\r' && (i + 1 == node_content->size - chunk.len || node_content->ptr[i + 1] != '\n'))) b->start_line++;
    }
    b->start_column = 1;
  }
  bufsize_t dropped = node_content->size - chunk.len;
  if (dropped && b->math_line_count) {
    size_t out = 0;
    for (size_t i = 0; i < b->math_line_count; i++) {
      cmark_math_line row = b->math_lines[i];
      bufsize_t next = i + 1 < b->math_line_count ? b->math_lines[i + 1].offset : node_content->size;
      if (next <= dropped) { cmark_chunk_free(parser->mem, &row.prefix); continue; }
      if (row.offset < dropped) {
        row.visual_column = cmark_reference_column(row.visual_column, node_content->ptr + row.offset, dropped - row.offset);
        row.prefix_visual_column = row.visual_column;
        row.column += dropped - row.offset;
        row.offset = 0;
        cmark_chunk_free(parser->mem, &row.prefix);
        row.prefix = cmark_chunk_literal("");
      } else row.offset -= dropped;
      b->math_lines[out++] = row;
    }
    b->math_line_count = out;
    if ((parser->options & CMARK_OPT_REFERENCE_DEFINITIONS) && out) {
      b->start_line = b->math_lines[0].line; b->start_column = b->math_lines[0].column;
    }
  }
  if (dropped) b->reference_cached_valid = false;
  cmark_strbuf_drop(node_content, dropped);
  return !is_blank(&b->content, 0);
}

// Table conversion must preserve completed reference definitions before changing paragraph type.
bool cmark_resolve_reference_definitions(cmark_parser *parser, cmark_node *node) {
  return resolve_reference_link_definitions(parser, node);
}

static cmark_node *finalize(cmark_parser *parser, cmark_node *b) {
  bufsize_t pos;
  cmark_node *item;
  cmark_node *subitem;
  cmark_node *parent;
  bool has_content;

  parent = b->parent;
  assert(b->flags &
         CMARK_NODE__OPEN); // shouldn't call finalize on closed blocks
  b->flags &= ~CMARK_NODE__OPEN;
  cmark_list_spread_close(parser, b);

  if (S_type(b) == CMARK_NODE_FOOTNOTE_DEFINITION &&
      (parser->options & CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    // S_process_line has recorded the actual boundary of this definition's last nonblank content line.
  } else if (parser->curline.size == 0) {
    // end of input - line number has not been incremented
    b->end_line = parser->line_number;
    b->end_column = parser->last_line_length;
  } else if (S_type(b) == CMARK_NODE_DOCUMENT ||
             (S_type(b) == CMARK_NODE_CODE_BLOCK && b->as.code.fenced) ||
             (S_type(b) == CMARK_NODE_MATH_BLOCK && b->math_closing_fence) ||
             (S_type(b) == CMARK_NODE_HEADING && b->as.heading.setext) ||
             (parser->line_number - 1 < b->end_line)) {
    b->end_line = parser->line_number;
    b->end_column = parser->curline.size;
    if (b->end_column && parser->curline.ptr[b->end_column - 1] == '\n')
      b->end_column -= 1;
    if (b->end_column && parser->curline.ptr[b->end_column - 1] == '\r')
      b->end_column -= 1;
  } else {
    b->end_line = parser->line_number - 1;
    b->end_column = parser->last_line_length;
  }

  if ((b->type == CMARK_NODE_DEFINITION_LIST || b->type == CMARK_NODE_DEFINITION_DESCRIPTION) && parser->curline.size > 0) {
    b->end_line = parser->line_number;
    b->end_column = parser->definition_line_offset;
  } else if ((b->type == CMARK_NODE_DEFINITION_LIST || b->type == CMARK_NODE_DEFINITION_DESCRIPTION) && !parser->definition_synthetic_ending) {
    b->end_line = parser->line_number + 1;
    b->end_column = 0;
  }
  cmark_strbuf *node_content = &b->content;

  if (S_type(b) == CMARK_NODE_MATH_BLOCK && parser->curline.size == 0 &&
      !b->math_closing_fence && parser->math_last_line_ending) {
    b->end_line = parser->line_number + 1;
    b->end_column = 0; // exclusive EOF is column 1 of the following line
  }

  if (S_type(b) == CMARK_NODE_MATH_BLOCK && !b->math_closing_fence) {
    // A following list item/list is a non-lazy container boundary. The newline
    // belongs to math, but none of that next line's quote/list prefixes do.
    for (cmark_node *owner = b->parent; owner; owner = owner->parent) {
      cmark_node *next = owner->next;
      if (next && next->start_line == parser->line_number &&
          ((owner->type == CMARK_NODE_ITEM && next->type == CMARK_NODE_ITEM) ||
           (owner->type == CMARK_NODE_LIST && next->type == CMARK_NODE_LIST))) {
        b->end_line = parser->line_number; b->end_column = 0; break;
      }
    }
  }

  switch (S_type(b)) {
  case CMARK_NODE_PARAGRAPH:
  {
    has_content = b->definition.no_reference || resolve_reference_link_definitions(parser, b);
    if (!has_content && (parser->options & CMARK_OPT_PRESERVE_WHITESPACE) == 0) {
      // remove blank node (former reference def)
      cmark_node_free(b);
    }
    break;
  }

  case CMARK_NODE_MATH_BLOCK: {
    // First content line is the meta string. Keep trailing spaces: unlike code info,
    // math meta is not trimmed by mdast-util-math.
    for (pos = 0; pos < node_content->size && !S_is_line_end_char(node_content->ptr[pos]); pos++) {}
    cmark_strbuf meta = CMARK_BUF_INIT(parser->mem);
    cmark_decode_string_token(&meta, node_content->ptr, pos);
    b->as.code.info = cmark_chunk_buf_detach(&meta);
    if (pos < node_content->size && node_content->ptr[pos] == '\r') pos++;
    if (pos < node_content->size && node_content->ptr[pos] == '\n') pos++;
    cmark_strbuf_drop(node_content, pos);
    // Math value excludes only its last source line ending, retaining internal raw EOLs.
    if (node_content->size && node_content->ptr[node_content->size - 1] == '\n') {
      cmark_strbuf_truncate(node_content, node_content->size - 1);
      if (node_content->size && node_content->ptr[node_content->size - 1] == '\r') cmark_strbuf_truncate(node_content, node_content->size - 1);
    } else if (node_content->size && node_content->ptr[node_content->size - 1] == '\r') cmark_strbuf_truncate(node_content, node_content->size - 1);
    b->as.code.literal = cmark_chunk_buf_detach(node_content);
    break;
  }

  case CMARK_NODE_CODE_BLOCK:
    cmark_code_metadata_finish(b);
    if (!b->as.code.fenced) { // indented code
      remove_trailing_blank_lines(node_content);
      cmark_strbuf_putc(node_content, '\n');
    } else {
      // first line of contents becomes info
      for (pos = 0; pos < node_content->size; ++pos) {
        if (S_is_line_end_char(node_content->ptr[pos]))
          break;
      }
      assert(pos < node_content->size);

      cmark_strbuf tmp = CMARK_BUF_INIT(parser->mem);
      if (parser->options & CMARK_OPT_WEB_CHARACTER_REFERENCES) {
        bufsize_t start = 0, end = pos;
        while (start < end && (node_content->ptr[start] == ' ' || node_content->ptr[start] == '\t')) start++;
        while (end > start && (node_content->ptr[end - 1] == ' ' || node_content->ptr[end - 1] == '\t')) end--;
        cmark_decode_string_token(&tmp, node_content->ptr + start, end - start);
      } else {
        houdini_unescape_html_f(&tmp, node_content->ptr, pos);
        cmark_strbuf_trim(&tmp);
        cmark_strbuf_unescape(&tmp);
      }
      b->as.code.info = cmark_chunk_buf_detach(&tmp);

      if (node_content->ptr[pos] == '\r')
        pos += 1;
      if (node_content->ptr[pos] == '\n')
        pos += 1;
      cmark_strbuf_drop(node_content, pos);
    }
    b->as.code.literal = cmark_chunk_buf_detach(node_content);
    break;

  case CMARK_NODE_HTML_BLOCK:
    b->as.literal = cmark_chunk_buf_detach(node_content);
    break;

  case CMARK_NODE_LIST:      // determine tight/loose status
    b->as.list.tight = true; // tight by default
    item = b->first_child;

    while (item) {
      // check for non-final non-empty list item ending with blank line:
      if (S_last_line_blank(item) && item->next) {
        b->as.list.tight = false;
        break;
      }
      // recurse into children of list item, to see if there are
      // spaces between them:
      subitem = item->first_child;
      while (subitem) {
        if ((item->next || subitem->next) &&
            S_ends_with_blank_line(subitem)) {
          b->as.list.tight = false;
          break;
        }
        subitem = subitem->next;
      }
      if (!(b->as.list.tight)) {
        break;
      }
      item = item->next;
    }

    break;

  default:
    break;
  }

  return parent;
}

// Add a node as child of another.  Return pointer to child.
static cmark_node *add_child(cmark_parser *parser, cmark_node *parent,
                             cmark_node_type block_type, int start_column) {
  assert(parent);

  // if 'parent' isn't the kind of node that can accept this child,
  // then back up til we hit a node that can.
  while (!cmark_node_can_contain_type(parent, block_type)) {
    parent = finalize(parser, parent);
  }

  cmark_node *child =
      make_block(parser->mem, block_type, parser->line_number, start_column);
  child->parent = parent;

  if (parent->last_child) {
    parent->last_child->next = child;
    child->prev = parent->last_child;
  } else {
    parent->first_child = child;
    child->prev = NULL;
  }
  parent->last_child = child;
  return child;
}

void cmark_manage_extensions_special_characters(cmark_parser *parser, int add) {
  cmark_llist *tmp_ext;

  for (tmp_ext = parser->inline_syntax_extensions; tmp_ext; tmp_ext=tmp_ext->next) {
    cmark_syntax_extension *ext = (cmark_syntax_extension *) tmp_ext->data;
    cmark_llist *tmp_char;
    for (tmp_char = ext->special_inline_chars; tmp_char; tmp_char=tmp_char->next) {
      unsigned char c = (unsigned char)(size_t)tmp_char->data;
      if (add)
        cmark_inlines_add_special_character(parser, c, ext->emphasis);
      else
        cmark_inlines_remove_special_character(parser, c, ext->emphasis);
    }
  }
}

// Walk through node and all children, recursively, parsing
// string content into inline content where appropriate.
static void process_inlines(cmark_parser *parser,
                            cmark_map *refmap, int options) {
  cmark_iter *iter = cmark_iter_new(parser->root);
  cmark_node *cur;
  cmark_event_type ev_type;

  cmark_manage_extensions_special_characters(parser, true);

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_ENTER) {
      if (contains_inlines(cur)) {
        cmark_parse_inlines(parser, cur, refmap, options);
        if (cur->definition.hardbreak) {
          cmark_node *linebreak = cmark_node_new_with_mem(CMARK_NODE_LINEBREAK, parser->mem);
          linebreak->start_line = linebreak->end_line = cur->end_line;
          linebreak->start_column = linebreak->end_column = cur->end_column;
          cmark_node_append_child(cur, linebreak);
        }
      }
    }
  }

  cmark_manage_extensions_special_characters(parser, false);

  cmark_iter_free(iter);
}

static int sort_footnote_by_ix(const void *_a, const void *_b) {
  cmark_footnote *a = *(cmark_footnote **)_a;
  cmark_footnote *b = *(cmark_footnote **)_b;
  return (int)a->ix - (int)b->ix;
}

static void process_footnotes(cmark_parser *parser) {
  // * Collect definitions in a map.
  // * Iterate the references in the document in order, assigning indices to
  //   definitions in the order they're seen.
  // * Write out the footnotes at the bottom of the document in index order.

  cmark_map *map = cmark_footnote_map_new(parser->mem);

  cmark_iter *iter = cmark_iter_new(parser->root);
  cmark_node *cur;
  cmark_event_type ev_type;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_EXIT && cur->type == CMARK_NODE_FOOTNOTE_DEFINITION) {
      cmark_footnote_create(map, cur);
    }
  }

  cmark_iter_free(iter);
  iter = cmark_iter_new(parser->root);
  unsigned int ix = 0;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_EXIT && cur->type == CMARK_NODE_FOOTNOTE_REFERENCE) {
      cmark_footnote *footnote = (cmark_footnote *)cmark_map_lookup(map, &cur->as.literal);
      if (footnote) {
        if (!footnote->ix)
          footnote->ix = ++ix;

        // store a reference to this footnote reference's footnote definition
        // this is used by renderers when generating label ids
        cur->parent_footnote_def = footnote->node;

        // keep track of a) count of how many times this footnote def has been
        // referenced, and b) which reference index this footnote ref is at.
        // this is used by renderers when generating links and backreferences.
        cur->footnote.ref_ix = ++footnote->node->footnote.def_count;

        char n[32];
        snprintf(n, sizeof(n), "%d", footnote->ix);
        cmark_chunk_free(parser->mem, &cur->as.literal);
        cmark_strbuf buf = CMARK_BUF_INIT(parser->mem);
        cmark_strbuf_puts(&buf, n);

        cur->as.literal = cmark_chunk_buf_detach(&buf);
      } else {
        cmark_node *text = (cmark_node *)parser->mem->calloc(1, sizeof(*text));
        cmark_strbuf_init(parser->mem, &text->content, 0);
        text->type = (uint16_t) CMARK_NODE_TEXT;

        cmark_strbuf buf = CMARK_BUF_INIT(parser->mem);
        cmark_strbuf_puts(&buf, "[^");
        cmark_strbuf_put(&buf, cur->as.literal.data, cur->as.literal.len);
        cmark_strbuf_putc(&buf, ']');

        text->as.literal = cmark_chunk_buf_detach(&buf);
        cmark_node_insert_after(cur, text);
        cmark_node_free(cur);
      }
    }
  }

  cmark_iter_free(iter);

  if (map->sorted) {
    qsort(map->sorted, map->size, sizeof(cmark_map_entry *), sort_footnote_by_ix);
    for (unsigned int i = 0; i < map->size; ++i) {
      cmark_footnote *footnote = (cmark_footnote *)map->sorted[i];
      if (!footnote->ix) {
        cmark_node_unlink(footnote->node);
        continue;
      }
      cmark_node_append_child(parser->root, footnote->node);
      footnote->node = NULL;
    }
  }

  cmark_unlink_footnotes_map(map);
  cmark_map_free(map);
}

// Attempts to parse a list item marker (bullet or enumerated).
// On success, returns length of the marker, and populates
// data with the details.  On failure, returns 0.
static bufsize_t parse_list_marker(cmark_mem *mem, cmark_chunk *input,
                                   bufsize_t pos, bool interrupts_paragraph,
                                   cmark_list **dataptr) {
  unsigned char c;
  bufsize_t startpos;
  cmark_list *data;
  bufsize_t i;

  startpos = pos;
  c = peek_at(input, pos);

  if (c == '*' || c == '-' || c == '+') {
    pos++;
    if (!cmark_isspace(peek_at(input, pos))) {
      return 0;
    }

    if (interrupts_paragraph) {
      i = pos;
      // require non-blank content after list marker:
      while (S_is_space_or_tab(peek_at(input, i))) {
        i++;
      }
      if (peek_at(input, i) == '\n') {
        return 0;
      }
    }

    data = (cmark_list *)mem->calloc(1, sizeof(*data));
    data->marker_offset = 0; // will be adjusted later
    data->list_type = CMARK_BULLET_LIST;
    data->bullet_char = c;
    data->start = 0;
    data->delimiter = CMARK_NO_DELIM;
    data->tight = false;
  } else if (cmark_isdigit(c)) {
    int start = 0;
    int digits = 0;

    do {
      start = (10 * start) + (peek_at(input, pos) - '0');
      pos++;
      digits++;
      // We limit to 9 digits to avoid overflow,
      // assuming max int is 2^31 - 1
      // This also seems to be the limit for 'start' in some browsers.
    } while (digits < 9 && cmark_isdigit(peek_at(input, pos)));

    if (interrupts_paragraph && start != 1) {
      return 0;
    }
    c = peek_at(input, pos);
    if (c == '.' || c == ')') {
      pos++;
      if (!cmark_isspace(peek_at(input, pos))) {
        return 0;
      }
      if (interrupts_paragraph) {
        // require non-blank content after list marker:
        i = pos;
        while (S_is_space_or_tab(peek_at(input, i))) {
          i++;
        }
        if (S_is_line_end_char(peek_at(input, i))) {
          return 0;
        }
      }

      data = (cmark_list *)mem->calloc(1, sizeof(*data));
      data->marker_offset = 0; // will be adjusted later
      data->list_type = CMARK_ORDERED_LIST;
      data->bullet_char = 0;
      data->start = start;
      data->delimiter = (c == '.' ? CMARK_PERIOD_DELIM : CMARK_PAREN_DELIM);
      data->tight = false;
    } else {
      return 0;
    }
  } else {
    return 0;
  }

  *dataptr = data;
  return (pos - startpos);
}

// Return 1 if list item belongs in list, else 0.
static int lists_match(cmark_list *list_data, cmark_list *item_data) {
  return (list_data->list_type == item_data->list_type &&
          list_data->delimiter == item_data->delimiter &&
          // list_data->marker_offset == item_data.marker_offset &&
          list_data->bullet_char == item_data->bullet_char);
}

static cmark_node *finalize_document(cmark_parser *parser) {
  while (parser->current != parser->root) {
    parser->current = finalize(parser, parser->current);
  }

  finalize(parser, parser->root);

  // Limit total size of extra content created from reference links to
  // document size to avoid superlinear growth. Always allow 100KB.
  if (parser->total_size > 100000)
    parser->refmap->max_ref_size = parser->total_size;
  else
    parser->refmap->max_ref_size = 100000;

  if ((parser->options & CMARK_OPT_FOOTNOTES) &&
      (parser->options & CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    // Collect all definitions after block parsing so references can precede definitions.
    cmark_map *map = cmark_footnote_map_new(parser->mem);
    cmark_iter *iter = cmark_iter_new(parser->root);
    cmark_event_type event;
    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
      cmark_node *node = cmark_iter_get_node(iter);
      if (event == CMARK_EVENT_ENTER &&
          node->type == CMARK_NODE_FOOTNOTE_DEFINITION)
        cmark_footnote_create(map, node);
    }
    cmark_iter_free(iter);
    parser->footnote_map = map;
    process_inlines(parser, parser->refmap, parser->options);
    parser->footnote_map = NULL;
    // sorted is deduplicated; every entry in the full refs chain only borrows tree nodes.
    for (cmark_map_entry *entry = map->refs; entry; entry = entry->next)
      ((cmark_footnote *)entry)->node = NULL;
    cmark_map_free(map);
  } else {
    process_inlines(parser, parser->refmap, parser->options);
    if (parser->options & CMARK_OPT_FOOTNOTES)
      process_footnotes(parser);
  }

  return parser->root;
}

cmark_node *cmark_parse_file(FILE *f, int options) {
  unsigned char buffer[4096];
  cmark_parser *parser = cmark_parser_new(options);
  size_t bytes;
  cmark_node *document;

  while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    bool eof = bytes < sizeof(buffer);
    S_parser_feed(parser, buffer, bytes, eof);
    if (eof) {
      break;
    }
  }

  document = cmark_parser_finish(parser);
  cmark_parser_free(parser);
  return document;
}

cmark_node *cmark_parse_document(const char *buffer, size_t len, int options) {
  cmark_parser *parser = cmark_parser_new(options);
  cmark_node *document;

  S_parser_feed(parser, (const unsigned char *)buffer, len, true);

  document = cmark_parser_finish(parser);
  cmark_parser_free(parser);
  return document;
}

void cmark_parser_feed(cmark_parser *parser, const char *buffer, size_t len) {
  S_parser_feed(parser, (const unsigned char *)buffer, len, false);
}

void cmark_parser_feed_reentrant(cmark_parser *parser, const char *buffer, size_t len) {
  cmark_strbuf saved_linebuf;

  cmark_strbuf_init(parser->mem, &saved_linebuf, 0);
  cmark_strbuf_puts(&saved_linebuf, cmark_strbuf_cstr(&parser->linebuf));
  cmark_strbuf_clear(&parser->linebuf);

  S_parser_feed(parser, (const unsigned char *)buffer, len, true);

  cmark_strbuf_sets(&parser->linebuf, cmark_strbuf_cstr(&saved_linebuf));
  cmark_strbuf_free(&saved_linebuf);
}

static void S_parser_feed(cmark_parser *parser, const unsigned char *buffer,
                          size_t len, bool eof) {
  const unsigned char *end = buffer + len;
  static const uint8_t repl[] = {239, 191, 189};
  bool preserveWhitespace = parser->options & CMARK_OPT_PRESERVE_WHITESPACE;

  if (len > UINT_MAX - parser->total_size)
    parser->total_size = UINT_MAX;
  else
    parser->total_size += len;

  if (parser->last_buffer_ended_with_cr && *buffer == '\n') {
    // skip NL if last buffer ended with CR ; see #117
    cmark_node *target = parser->math_eol_target;
    if (target && target->code_metadata && target->code_metadata->source.size &&
        target->code_metadata->source.ptr[target->code_metadata->source.size - 1] == '\r')
      { cmark_strbuf_putc(&target->code_metadata->source, '\n');
        if (target->code_metadata->kind == 2) cmark_strbuf_putc(&target->code_metadata->pending, '\n'); }
    if (target && target->type == CMARK_NODE_MATH_BLOCK) cmark_strbuf_putc(&target->content, '\n');
    else if (target && target->math_line_count) target->math_lines[target->math_line_count - 1].ending = 0x0d0a;
    buffer++;
  }
  parser->last_buffer_ended_with_cr = false;
  while (buffer < end) {
    const unsigned char *eol;
    bufsize_t chunk_len;
    bool process = false;
    for (eol = buffer; eol < end; ++eol) {
      if (S_is_line_end_char(*eol)) {
        process = true;
        break;
      }
      if (*eol == '\0' && eol < end) {
        break;
      }
    }
    if (eol >= end && eof) {
      process = true;
    }

    chunk_len = (bufsize_t)(eol - buffer);
    if (process) {
      parser->math_last_line_ending = eol < end && S_is_line_end_char(*eol);
      parser->math_current_ending = !parser->math_last_line_ending ? 0 : *eol == '\n' ? 10 : eol + 1 < end && eol[1] == '\n' ? 0x0d0a : 13;
      if (parser->linebuf.size > 0) {
        cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
        parser->definition_synthetic_ending = eol == end;
        S_process_line(parser, parser->linebuf.ptr, parser->linebuf.size, !preserveWhitespace || !eof || eol < end);
        cmark_strbuf_clear(&parser->linebuf);
      } else {
        parser->definition_synthetic_ending = eol == end;
        S_process_line(parser, buffer, chunk_len, !preserveWhitespace || !eof || eol < end);
      }
    } else {
      if (eol < end && *eol == '\0') {
        // omit NULL byte
        cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
        // add replacement character
        cmark_strbuf_put(&parser->linebuf, repl, 3);
      } else {
        cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
      }
    }

    buffer += chunk_len;
    if (buffer < end) {
      if (*buffer == '\0') {
        // skip over NULL
        buffer++;
      } else {
        // skip over line ending characters
        if (*buffer == '\r') {
          buffer++;
          if (buffer == end)
            parser->last_buffer_ended_with_cr = true;
        }
        if (buffer < end && *buffer == '\n')
          buffer++;
      }
    }
  }
}

static void chop_trailing_hashtags(cmark_chunk *ch) {
  bufsize_t n, orig_n;

  cmark_chunk_rtrim(ch);
  orig_n = n = ch->len - 1;

  // if string ends in space followed by #s, remove these:
  while (n >= 0 && peek_at(ch, n) == '#')
    n--;

  // Check for a space before the final #s:
  if (n != orig_n && n >= 0 && S_is_space_or_tab(peek_at(ch, n))) {
    ch->len = n;
    cmark_chunk_rtrim(ch);
  }
}

// Check for thematic break.  On failure, return 0 and update
// thematic_break_kill_pos with the index at which the
// parse fails.  On success, return length of match.
// "...three or more hyphens, asterisks,
// or underscores on a line by themselves. If you wish, you may use
// spaces between the hyphens or asterisks."
static int S_scan_thematic_break(cmark_parser *parser, cmark_chunk *input,
                                 bufsize_t offset) {
  bufsize_t i;
  char c;
  char nextc = '\0';
  int count;
  i = offset;
  c = peek_at(input, i);
  if (!(c == '*' || c == '_' || c == '-')) {
    parser->thematic_break_kill_pos = i;
    return 0;
  }
  count = 1;
  while ((nextc = peek_at(input, ++i))) {
    if (nextc == c) {
      count++;
    } else if (nextc != ' ' && nextc != '\t') {
      break;
    }
  }
  if (count >= 3 && (nextc == '\r' || nextc == '\n')) {
    return (i - offset) + 1;
  } else {
    parser->thematic_break_kill_pos = i;
    return 0;
  }
}

// Find first nonspace character from current offset, setting
// parser->first_nonspace, parser->first_nonspace_column,
// parser->indent, and parser->blank. Does not advance parser->offset.
static void S_find_first_nonspace(cmark_parser *parser, cmark_chunk *input) {
  char c;
  int chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);

  if (parser->first_nonspace <= parser->offset) {
    parser->first_nonspace = parser->offset;
    parser->first_nonspace_column = parser->column;
    while ((c = peek_at(input, parser->first_nonspace))) {
      if (c == ' ') {
        parser->first_nonspace += 1;
        parser->first_nonspace_column += 1;
        chars_to_tab = chars_to_tab - 1;
        if (chars_to_tab == 0) {
          chars_to_tab = TAB_STOP;
        }
      } else if (c == '\t') {
        parser->first_nonspace += 1;
        parser->first_nonspace_column += chars_to_tab;
        chars_to_tab = TAB_STOP;
      } else {
        break;
      }
    }
  }

  parser->indent = parser->first_nonspace_column - parser->column;
  parser->blank = S_is_line_end_char(peek_at(input, parser->first_nonspace));
}

// Advance parser->offset and parser->column.  parser->offset is the
// byte position in input; parser->column is a virtual column number
// that takes into account tabs. (Multibyte characters are not taken
// into account, because the Markdown line prefixes we are interested in
// analyzing are entirely ASCII.)  The count parameter indicates
// how far to advance the offset.  If columns is true, then count
// indicates a number of columns; otherwise, a number of bytes.
// If advancing a certain number of columns partially consumes
// a tab character, parser->partially_consumed_tab is set to true.
static void S_advance_offset(cmark_parser *parser, cmark_chunk *input,
                             bufsize_t count, bool columns) {
  char c;
  int chars_to_tab;
  int chars_to_advance;
  while (count > 0 && (c = peek_at(input, parser->offset))) {
    if (c == '\t') {
      chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);
      if (columns) {
        parser->partially_consumed_tab = chars_to_tab > count;
        chars_to_advance = MIN(count, chars_to_tab);
        parser->column += chars_to_advance;
        parser->offset += (parser->partially_consumed_tab ? 0 : 1);
        count -= chars_to_advance;
      } else {
        parser->partially_consumed_tab = false;
        parser->column += chars_to_tab;
        parser->offset += 1;
        count -= 1;
      }
    } else {
      parser->partially_consumed_tab = false;
      parser->offset += 1;
      parser->column += 1; // assume ascii; block starts are ascii
      count -= 1;
    }
  }
}

static bool S_last_child_is_open(cmark_node *container) {
  return container->last_child &&
         (container->last_child->flags & CMARK_NODE__OPEN);
}

static bool parse_block_quote_prefix(cmark_parser *parser, cmark_chunk *input) {
  bool res = false;
  bufsize_t matched = 0;

  matched =
      parser->indent <= 3 && peek_at(input, parser->first_nonspace) == '>';
  if (matched) {

    S_advance_offset(parser, input, parser->indent + 1, true);

    if (S_is_space_or_tab(peek_at(input, parser->offset))) {
      S_advance_offset(parser, input, 1, true);
    }

    res = true;
  }
  return res;
}

static bool parse_footnote_definition_block_prefix(cmark_parser *parser, cmark_chunk *input,
                                                   cmark_node *container) {
  if (parser->indent >= 4) {
    if ((parser->options & CMARK_OPT_CODE_METADATA) && parser->blank && parser->code_footnote_blank_offset < 0) {
      parser->code_footnote_blank_offset = parser->offset;
      parser->code_footnote_blank_container = container;
      parser->code_footnote_blank_column = parser->column;
      parser->code_footnote_blank_partial = parser->partially_consumed_tab;
    }
    S_advance_offset(parser, input, 4, true);
    return true;
  } else if (input->len > 0 && (input->data[0] == '\n' || (input->data[0] == '\r' && input->data[1] == '\n'))) {
    return true;
  }

  return false;
}

static bool parse_node_item_prefix(cmark_parser *parser, cmark_chunk *input,
                                   cmark_node *container) {
  bool res = false;

  if (parser->indent >=
      container->as.list.marker_offset + container->as.list.padding) {
    S_advance_offset(parser, input, container->as.list.marker_offset +
                                        container->as.list.padding,
                     true);
    res = true;
  } else if (parser->blank && container->first_child != NULL) {
    // if container->first_child is NULL, then the opening line
    // of the list item was blank after the list marker; in this
    // case, we are done with the list item.
    S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                     false);
    res = true;
  }
  return res;
}

static bool parse_code_block_prefix(cmark_parser *parser, cmark_chunk *input,
                                    cmark_node *container,
                                    bool *should_continue) {
  bool res = false;

  if (!container->as.code.fenced) { // indented
    if (parser->indent >= CODE_INDENT) {
      if (container->code_metadata) container->code_metadata->indented_qualified = true;
      S_advance_offset(parser, input, CODE_INDENT, true);
      res = true;
    } else if (parser->blank) {
      if (container->code_metadata) container->code_metadata->indented_qualified = false;
      S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                       false);
      res = true;
    }
  } else { // fenced
    bufsize_t matched = 0;

    if (parser->indent <= 3 && (peek_at(input, parser->first_nonspace) ==
                                container->as.code.fence_char)) {
      matched = scan_close_code_fence(input, parser->first_nonspace);
    }

    bufsize_t required = ((parser->options & CMARK_OPT_WEB_CODE_SYNTAX) &&
        container->as.code.web_fence_length > 0)
        ? container->as.code.web_fence_length : container->as.code.fence_length;
    if (matched >= required) {
      // closing fence - and since we're at
      // the end of a line, we can stop processing it:
      *should_continue = false;
      S_advance_offset(parser, input, matched, false);
      if (container->code_metadata) container->code_metadata->closed = true;
      cmark_list_spread_leaf(parser, container, true);
      parser->current = finalize(parser, container);
    } else {
      // skip opt. spaces of fence parser->offset
      int i = container->as.code.fence_offset;

      while (i > 0 && S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
        i--;
      }
      res = true;
    }
  }

  return res;
}

static bufsize_t math_fence(cmark_chunk *input, bufsize_t start, bool closing) {
  bufsize_t end = start;
  while (end < input->len && input->data[end] == '$') end++;
  bufsize_t count = end - start;
  if (count < 2) return 0;
  while (end < input->len && !S_is_line_end_char(input->data[end])) {
    if (closing ? !S_is_space_or_tab(input->data[end]) : input->data[end] == '$') return 0;
    end++;
  }
  return count;
}

static bool parse_math_prefix(cmark_parser *parser, cmark_chunk *input,
                              cmark_node *container, bool *should_continue) {
  bufsize_t matched = parser->indent <= 3 ? math_fence(input, parser->first_nonspace, true) : 0;
  if (matched >= container->math_fence_length && matched > 0) {
    container->math_closing_fence = true; // finalize this line, not the previous line
    *should_continue = false;
    cmark_list_spread_leaf(parser, container, true);
    S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
    parser->current = finalize(parser, container);
    return false;
  }
  int indent = container->as.code.fence_offset;
  while (indent > 0 && S_is_space_or_tab(peek_at(input, parser->offset))) {
    S_advance_offset(parser, input, 1, true);
    indent--;
  }
  return true;
}

static bool parse_html_block_prefix(cmark_parser *parser,
                                    cmark_node *container) {
  bool res = false;
  int html_block_type = container->as.html_block_type;

  assert(html_block_type >= 1 && html_block_type <= 7);
  switch (html_block_type) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    // these types of blocks can accept blanks
    res = true;
    break;
  case 6:
  case 7:
    res = !parser->blank;
    break;
  }

  return res;
}

static bool parse_extension_block(cmark_parser *parser,
                                  cmark_node *container,
                                  cmark_chunk *input)
{
  bool res = false;

  if (container->extension->last_block_matches) {
    if (container->extension->last_block_matches(
        container->extension, parser, input->data, input->len, container))
      res = true;
  }

  return res;
}

/**
 * For each containing node, try to parse the associated line start.
 *
 * Will not close unmatched blocks, as we may have a lazy continuation
 * line -> http://spec.commonmark.org/0.24/#lazy-continuation-line
 *
 * Returns: The last matching node, or NULL
 */
static cmark_node *check_open_blocks(cmark_parser *parser, cmark_chunk *input,
                                     bool *all_matched) {
  bool should_continue = true;
  *all_matched = false;
  cmark_node *container = parser->root;
  cmark_node_type cont_type;
    
    

  while (S_last_child_is_open(container)) {
    container = container->last_child;
    cont_type = S_type(container);

    S_find_first_nonspace(parser, input);

    if (container->extension) {
      if ((parser->options & CMARK_OPT_DEFINITION_LISTS) &&
          strcmp(cmark_node_get_type_string(container), "table") == 0 &&
          parser->indent < 4 && peek_at(input, parser->first_nonspace) == ':' &&
          (S_is_space_or_tab(peek_at(input, parser->first_nonspace + 1)) ||
           S_is_line_end_char(peek_at(input, parser->first_nonspace + 1)))) goto done;
      if (!parse_extension_block(parser, container, input))
        goto done;
      continue;
    }

    switch (cont_type) {
    case CMARK_NODE_BLOCK_QUOTE:
      if (!parse_block_quote_prefix(parser, input))
        goto done;
      cmark_list_spread_prefix(parser, container);
      break;
    case CMARK_NODE_DEFINITION_DESCRIPTION:
      cmark_list_spread_unsupported(parser);
      if (parser->blank) {
        container->definition.blank_lines++;
        S_advance_offset(parser, input, MIN(parser->indent, container->definition.padding), true);
      } else if (container->definition.initial_blank && container->definition.blank_lines > 0) {
        parser->definition_lazy_code = true;
        goto done;
      } else if (parser->indent >= container->definition.padding) {
        S_advance_offset(parser, input, container->definition.padding, true);
        container->definition.blank_lines = 0;
        container->definition.initial_blank = false;
      } else {
        goto done;
      }
      break;
    case CMARK_NODE_ITEM:
      if (!parse_node_item_prefix(parser, input, container))
        goto done;
      break;
    case CMARK_NODE_MATH_BLOCK:
      if (!parse_math_prefix(parser, input, container, &should_continue)) goto done;
      break;
    case CMARK_NODE_CODE_BLOCK:
      if (!parse_code_block_prefix(parser, input, container, &should_continue))
        goto done;
      break;
    case CMARK_NODE_HEADING:
      // a heading can never contain more than one line
      goto done;
    case CMARK_NODE_HTML_BLOCK:
      if (!parse_html_block_prefix(parser, container))
        goto done;
      break;
    case CMARK_NODE_PARAGRAPH:
      if (parser->blank) {
        if ((parser->options & CMARK_OPT_DEFINITION_LISTS) && parser->column > 0) {
          container->definition.flow_offset = container->content.size;
          container->definition.flow_line = parser->line_number;
          container->definition.flow_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
        }
        goto done;
      }
      break;
		case CMARK_NODE_FOOTNOTE_DEFINITION:
      /* Web emits a footnote indent token only on non-blank continuation lines. */
      if (!parser->blank && parser->indent >= 4)
        cmark_list_spread_prefix(parser, container);
      if (!parse_footnote_definition_block_prefix(parser, input, container)) {
        /* Web continues footnote definitions across every blank line; this
         * parser closes them on whitespace-only or CR blank lines. */
        if (parser->blank)
          cmark_list_spread_unsupported(parser);
        if (parser->options & CMARK_OPT_CODE_METADATA) {
          for (cmark_node *open = parser->current; open && open != container; open = open->parent)
            if (open->code_metadata && open->code_metadata->kind == 1) open->code_metadata->boundary_trim = true;
        }
        goto done;
      }
      break;
    default:
      break;
    }
  }

  *all_matched = true;

done:
  if (!*all_matched) {
    container = container->parent; // back up to last matching node
  }

  if (!should_continue) {
    container = NULL;
  }

  return container;
}

static void open_new_blocks(cmark_parser *parser, cmark_node **container,
                            cmark_chunk *input, bool all_matched) {
  bool indented;
  cmark_list *data = NULL;
  bool maybe_lazy = S_type(parser->current) == CMARK_NODE_PARAGRAPH;
  cmark_node_type cont_type = S_type(*container);
  bufsize_t matched = 0;
  int lev = 0;
  bool save_partially_consumed_tab;
  bool has_content;
  int save_offset;
  int save_column;
  size_t depth = 0;

  while (cont_type != CMARK_NODE_MATH_BLOCK &&
         cont_type != CMARK_NODE_CODE_BLOCK &&
         cont_type != CMARK_NODE_HTML_BLOCK) {
    depth++;
    S_find_first_nonspace(parser, input);
    indented = parser->indent >= CODE_INDENT;

    if (!indented && peek_at(input, parser->first_nonspace) == '>') {

      bufsize_t blockquote_startpos = parser->first_nonspace;

      S_advance_offset(parser, input,
                       parser->first_nonspace + 1 - parser->offset, false);
      // optional following character
      if (S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
      }
      *container = add_child(parser, *container, CMARK_NODE_BLOCK_QUOTE,
                             blockquote_startpos + 1);
      cmark_list_spread_open(parser, *container);

    } else if (!indented && (matched = scan_atx_heading_start(
                                 input, parser->first_nonspace))) {
      bufsize_t hashpos;
      int level = 0;
      bufsize_t heading_startpos = parser->first_nonspace;

      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);
      *container = add_child(parser, *container, CMARK_NODE_HEADING,
                             heading_startpos + 1);

      hashpos = cmark_chunk_strchr(input, '#', parser->first_nonspace);

      while (peek_at(input, hashpos) == '#') {
        level++;
        hashpos++;
      }

      (*container)->as.heading.level = level;
      (*container)->as.heading.setext = false;
      (*container)->internal_offset = matched;
      cmark_list_spread_leaf(parser, *container, false);

    } else if (!indented && (parser->options & CMARK_OPT_MATH) &&
               (matched = math_fence(input, parser->first_nonspace, false))) {
      *container = add_child(parser, *container, CMARK_NODE_MATH_BLOCK, parser->first_nonspace + 1);
      (*container)->math_fence_length = matched;
      (*container)->as.code.fenced = 1;
      (*container)->as.code.fence_offset = parser->first_nonspace - parser->offset;
      (*container)->as.code.info = cmark_chunk_literal("");
      S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
      // factorySpace before meta accepts spaces and tabs, not arbitrary Unicode whitespace.
      while (S_is_space_or_tab(peek_at(input, parser->offset))) S_advance_offset(parser, input, 1, false);
    } else if (!indented && (matched = scan_open_code_fence(
                                 input, parser->first_nonspace))) {
      *container = add_child(parser, *container, CMARK_NODE_CODE_BLOCK,
                             parser->first_nonspace + 1);
      (*container)->as.code.fenced = true;
      (*container)->as.code.fence_char = peek_at(input, parser->first_nonspace);
      (*container)->as.code.fence_length = (matched > 255) ? 255 : (uint8_t)matched;
      if (parser->options & CMARK_OPT_WEB_CODE_SYNTAX) (*container)->as.code.web_fence_length = matched;
      /* Metadata for a long opener requires the exact Web closing grammar.
       * Legacy mode still rejects metadata beyond its 255-character cap. */
      if ((parser->options & CMARK_OPT_CODE_METADATA) &&
          (matched <= 255 || (parser->options & CMARK_OPT_WEB_CODE_SYNTAX)))
        cmark_code_metadata_start(*container, 1);
      (*container)->as.code.fence_offset =
          (int8_t)((parser->options & CMARK_OPT_DEFINITION_LISTS) ? parser->indent : parser->first_nonspace - parser->offset);
      (*container)->as.code.info = cmark_chunk_literal("");
      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);

    } else if (!indented && ((matched = scan_html_block_start(
                                  input, parser->first_nonspace)) ||
                             (cont_type != CMARK_NODE_PARAGRAPH &&
                              (matched = scan_html_block_start_7(
                                   input, parser->first_nonspace))))) {
      *container = add_child(parser, *container, CMARK_NODE_HTML_BLOCK,
                             parser->first_nonspace + 1);
      (*container)->as.html_block_type = matched;
      // note, we don't adjust parser->offset because the tag is part of the
      // text
    } else if (!indented && cont_type == CMARK_NODE_PARAGRAPH &&
               (lev =
                    scan_setext_heading_line(input, parser->first_nonspace))) {
      // finalize paragraph, resolving reference links
      has_content = resolve_reference_link_definitions(parser, *container);

      if (has_content) {

        (*container)->type = (uint16_t)CMARK_NODE_HEADING;
        (*container)->as.heading.level = lev;
        (*container)->as.heading.setext = true;
        cmark_list_spread_leaf(parser, *container, false);
        S_advance_offset(parser, input, input->len - 1 - parser->offset, false);
      }
    } else if (!indented &&
               !(cont_type == CMARK_NODE_PARAGRAPH && !all_matched) &&
	       (parser->thematic_break_kill_pos <= parser->first_nonspace) &&
               (matched = S_scan_thematic_break(parser, input, parser->first_nonspace))) {
      // it's only now that we know the line is not part of a setext heading:
      *container = add_child(parser, *container, CMARK_NODE_THEMATIC_BREAK,
                             parser->first_nonspace + 1);
      cmark_list_spread_leaf(parser, *container, false);
      S_advance_offset(parser, input, input->len - 1 - parser->offset, false);
    } else if (!indented &&
               (parser->options & CMARK_OPT_FOOTNOTES) &&
               depth < MAX_LIST_DEPTH &&
               (matched = scan_footnote_definition(input, parser->first_nonspace))) {
      cmark_chunk c = cmark_chunk_dup(input, parser->first_nonspace + 2, matched - 2);

      while (c.data[c.len - 1] != ']')
        --c.len;
      --c.len;

      cmark_chunk_to_cstr(parser->mem, &c);

      S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
      *container = add_child(parser, *container, CMARK_NODE_FOOTNOTE_DEFINITION, parser->first_nonspace + matched + 1);
      if (parser->options & CMARK_OPT_FOOTNOTES_LOSSLESS)
        (*container)->start_column -= matched;
      (*container)->as.literal = c;

      (*container)->internal_offset = matched;
      cmark_list_spread_open(parser, *container);
    } else if ((!indented || cont_type == CMARK_NODE_LIST) &&
	       parser->indent < 4 &&
               depth < MAX_LIST_DEPTH &&
               (matched = parse_list_marker(
                    parser->mem, input, parser->first_nonspace,
                    ((*container)->type == CMARK_NODE_PARAGRAPH ||
                     ((*container)->type == CMARK_NODE_DEFINITION_DESCRIPTION &&
                      (*container)->start_line == parser->line_number && (*container)->definition.interrupt)), &data))) {

      // Note that we can have new list items starting with >= 4
      // spaces indent, as long as the list container is still open.
      int i = 0;

      // compute padding:
      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);

      save_partially_consumed_tab = parser->partially_consumed_tab;
      save_offset = parser->offset;
      save_column = parser->column;

      while (parser->column - save_column <= 5 &&
             S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
      }

      i = parser->column - save_column;
      if (i >= 5 || i < 1 ||
          // only spaces after list marker:
          S_is_line_end_char(peek_at(input, parser->offset))) {
        data->padding = matched + 1;
        parser->offset = save_offset;
        parser->column = save_column;
        parser->partially_consumed_tab = save_partially_consumed_tab;
        if (i > 0) {
          S_advance_offset(parser, input, 1, true);
        }
      } else {
        data->padding = matched + i;
      }

      // check container; if it's a list, see if this list item
      // can continue the list; otherwise, create a list container.

      data->marker_offset = parser->indent;

      if (cont_type != CMARK_NODE_LIST ||
          !lists_match(&((*container)->as.list), data)) {
        *container = add_child(parser, *container, CMARK_NODE_LIST,
                               parser->first_nonspace + 1);

        memcpy(&((*container)->as.list), data, sizeof(*data));
        cmark_list_spread_open(parser, *container);
      }

      // add the list item
      *container = add_child(parser, *container, CMARK_NODE_ITEM,
                             parser->first_nonspace + 1);
      cmark_list_spread_open(parser, *container);
      /* TODO: static */
      memcpy(&((*container)->as.list), data, sizeof(*data));
      parser->mem->free(data);
    } else if (indented && !maybe_lazy && !parser->blank) {
      S_advance_offset(parser, input, CODE_INDENT, true);
      *container = add_child(parser, *container, CMARK_NODE_CODE_BLOCK,
                             parser->offset + 1);
      (*container)->definition.single_line = parser->definition_lazy_code;
      (*container)->as.code.fenced = false;
      if (parser->options & CMARK_OPT_CODE_METADATA) { cmark_code_metadata_start(*container, 2); (*container)->code_metadata->indented_qualified = true; }
      (*container)->as.code.fence_char = 0;
      (*container)->as.code.fence_length = 0;
      (*container)->as.code.fence_offset = 0;
      (*container)->as.code.info = cmark_chunk_literal("");
    } else {
      cmark_llist *tmp;
      cmark_node *new_container = NULL;

      for (tmp = parser->syntax_extensions; tmp; tmp=tmp->next) {
        cmark_syntax_extension *ext = (cmark_syntax_extension *) tmp->data;

        if (ext->try_opening_block) {
          new_container = ext->try_opening_block(
              ext, indented, parser, *container, input->data, input->len);

          if (new_container) {
            *container = new_container;
            if (parser->options & CMARK_OPT_LIST_SPREAD_METADATA) {
              const char *kind = cmark_node_get_type_string(new_container);
              if (strcmp(kind, "table") == 0)
                cmark_list_spread_leaf(parser, new_container, false);
              else if ((strcmp(kind, "table_row") == 0 || strcmp(kind, "table_header") == 0) &&
                       new_container->parent)
                cmark_list_spread_leaf(parser, new_container->parent, false);
              else if (S_type(new_container) != CMARK_NODE_PARAGRAPH)
                cmark_list_spread_unsupported(parser);
            }
            break;
          }
        }
      }

      if (!new_container) {
        break;
      }
    }

    if (S_type(*container) != CMARK_NODE_CODE_BLOCK) parser->definition_lazy_code = false;
    if (accepts_lines(S_type(*container))) {
      // if it's a line container, it can't contain other containers
      break;
    }

    cont_type = S_type(*container);
    maybe_lazy = false;
  }
}

static void add_text_to_container(cmark_parser *parser, cmark_node *container,
                                  cmark_node *last_matched_container,
                                  cmark_chunk *input) {
  cmark_node *tmp;
  // what remains at parser->offset is a text line.  add the text to the
  // appropriate container.

  S_find_first_nonspace(parser, input);

  if (parser->blank && container->last_child)
    S_set_last_line_blank(container->last_child, true);

  // block quote lines are never blank as they start with >
  // and we don't count blanks in fenced code for purposes of tight/loose
  // lists or breaking out of lists.  we also don't set last_line_blank
  // on an empty list item.
  const cmark_node_type ctype = S_type(container);
  const bool last_line_blank =
      (parser->blank && ctype != CMARK_NODE_BLOCK_QUOTE &&
       ctype != CMARK_NODE_HEADING && ctype != CMARK_NODE_THEMATIC_BREAK &&
       !(ctype == CMARK_NODE_CODE_BLOCK && container->as.code.fenced) &&
       ctype != CMARK_NODE_MATH_BLOCK &&
       !(ctype == CMARK_NODE_ITEM && container->first_child == NULL &&
         container->start_line == parser->line_number));

  S_set_last_line_blank(container, last_line_blank);

  tmp = container;
  while (tmp->parent) {
    S_set_last_line_blank(tmp->parent, false);
    tmp = tmp->parent;
  }

  // If the last line processed belonged to a paragraph node,
  // and we didn't match all of the line prefixes for the open containers,
  // and we didn't start any new containers,
  // and the line isn't blank,
  // then treat this as a "lazy continuation line" and add it to
  // the open paragraph.
  if (parser->current != last_matched_container &&
      container == last_matched_container && (!parser->blank || (parser->options & CMARK_OPT_PRESERVE_WHITESPACE)) &&
      S_type(parser->current) == CMARK_NODE_PARAGRAPH) {
    if ((parser->options & CMARK_OPT_DEFINITION_LISTS) && parser->current->definition.lazy_offset == 0) {
      parser->current->definition.lazy_offset = parser->current->content.size;
      parser->current->definition.lazy_line = parser->line_number;
      parser->current->definition.lazy_column = parser->offset + 1;
    }
    cmark_list_spread_leaf(parser, parser->current, false);
    add_line(parser->current, input, parser);
  } else { // not a lazy continuation
    // Finalize any blocks that were not matched and set cur to container:
    while (parser->current != last_matched_container) {
      parser->current = finalize(parser, parser->current);
      assert(parser->current != NULL);
    }

    if (S_type(container) == CMARK_NODE_CODE_BLOCK || S_type(container) == CMARK_NODE_MATH_BLOCK) {
      cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    } else if (S_type(container) == CMARK_NODE_HTML_BLOCK) {
      add_line(container, input, parser);

      int matches_end_condition;
      switch (container->as.html_block_type) {
      case 1:
        // </script>, </style>, </pre>
        matches_end_condition =
            scan_html_block_end_1(input, parser->first_nonspace);
        break;
      case 2:
        // -->
        matches_end_condition =
            scan_html_block_end_2(input, parser->first_nonspace);
        break;
      case 3:
        // ?>
        matches_end_condition =
            scan_html_block_end_3(input, parser->first_nonspace);
        break;
      case 4:
        // >
        matches_end_condition =
            scan_html_block_end_4(input, parser->first_nonspace);
        break;
      case 5:
        // ]]>
        matches_end_condition =
            scan_html_block_end_5(input, parser->first_nonspace);
        break;
      default:
        matches_end_condition = 0;
        break;
      }

      cmark_list_spread_leaf(parser, container, matches_end_condition != 0);
      if (matches_end_condition) {
        container = finalize(parser, container);
        assert(parser->current != NULL);
      }
    } else if (parser->blank && (parser->options & CMARK_OPT_PRESERVE_WHITESPACE) == 0) {
      // ??? do nothing
    } else if (accepts_lines(S_type(container))) {
      if (S_type(container) == CMARK_NODE_HEADING &&
          container->as.heading.setext == false) {
        chop_trailing_hashtags(input);
      }
      if ((parser->options & CMARK_OPT_PRESERVE_WHITESPACE) == 0 &&
          !((parser->options & CMARK_OPT_DEFINITION_LISTS) && container->type == CMARK_NODE_PARAGRAPH && container->content.size > 0)) {
        parser->math_inline_prefix = parser->first_nonspace - parser->offset;
        S_advance_offset(parser, input, parser->first_nonspace - parser->offset, false);
      }
      cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    } else {
      // create paragraph container for line
      if (parser->options & CMARK_OPT_PRESERVE_WHITESPACE) {
        container = add_child(parser, container, CMARK_NODE_PARAGRAPH,
                              parser->offset + 1);
      } else {
        container = add_child(parser, container, CMARK_NODE_PARAGRAPH,
                              parser->first_nonspace + 1);
        container->definition.term_start_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
        S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                           false);
      }
      cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    }

    parser->current = container;
  }
}

// Definition lists are document containers. Recognize markers while the
// paragraph buffer and container continuation state are still available.
static bool S_open_definition(cmark_parser *parser, cmark_chunk *input,
                              cmark_node **matched) {
  if (!(parser->options & CMARK_OPT_DEFINITION_LISTS)) return false;
  S_find_first_nonspace(parser, input);
  if (parser->indent >= 4 || peek_at(input, parser->first_nonspace) != ':' ||
      !(S_is_space_or_tab(peek_at(input, parser->first_nonspace + 1)) ||
        S_is_line_end_char(peek_at(input, parser->first_nonspace + 1)))) return false;
  /* Definition-list markers can restructure surrounding flow in Web parsing;
   * list spread is not derived for documents containing a marker candidate. */
  cmark_list_spread_unsupported(parser);
  cmark_node *container = *matched;
  cmark_node *list = NULL;
  cmark_node *term = NULL;
  cmark_node *previous_description = NULL;
  bool spread = false;
  bool interrupt = false;
  if (container->type == CMARK_NODE_DEFINITION_LIST) {
    list = container;
    cmark_node *previous = list->last_child;
    previous_description = previous;
    spread = previous && (previous->definition.blank_lines > 0 || previous->definition.initial_blank);
    while (parser->current != list) parser->current = finalize(parser, parser->current);
  } else {
    cmark_node *old = parser->current;
    if (old->type == CMARK_NODE_PARAGRAPH && old->parent &&
        old->parent->type == CMARK_NODE_BLOCK_QUOTE && container != old &&
        container != old->parent) {
      int lazy_offset = old->definition.lazy_offset;
      if (lazy_offset > 0) {
        term = make_block(parser->mem, CMARK_NODE_PARAGRAPH,
                          old->definition.lazy_line, old->definition.lazy_column);
        cmark_strbuf_put(&term->content, old->content.ptr + lazy_offset, old->content.size - lazy_offset);
        cmark_strbuf_truncate(&old->content, lazy_offset);
        old->definition.hardbreak = true;
        int final_line = old->definition.lazy_line - 1;
        while (parser->current != container) {
          cmark_node *closing = parser->current;
          cmark_node *closing_parent = closing->parent;
          parser->current = finalize(parser, closing);
          if (closing_parent->last_child == closing) { closing->end_line = final_line + 1; closing->end_column = 0; }
        }
        cmark_node_append_child(container, term);
        term->definition.no_reference = true;
        parser->current = term;
        container = term;
      } else {
        while (parser->current != container) parser->current = finalize(parser, parser->current);
        return false;
      }
    }
    term = container->type == CMARK_NODE_PARAGRAPH ? container : container->last_child;
    if (!term || term->type != CMARK_NODE_PARAGRAPH || !term->parent ||
        (term->parent->type != CMARK_NODE_DOCUMENT &&
         term->parent->type != CMARK_NODE_BLOCK_QUOTE &&
         term->parent->type != CMARK_NODE_DEFINITION_DESCRIPTION)) return false;
    if (!(term->flags & CMARK_NODE__OPEN) && parser->line_number - term->end_line > 2) return false;
    interrupt = (term->flags & CMARK_NODE__OPEN) != 0;
    cmark_node *parent = term->parent;
    bool omit_term = false;
    if (term->definition.flow_offset > 0) {
      cmark_node *preceding = term;
      int offset = preceding->definition.flow_offset;
      if (offset == preceding->content.size) {
        omit_term = true;
        interrupt = false;
      } else {
        term = make_block(parser->mem, CMARK_NODE_PARAGRAPH, preceding->definition.flow_line,
                          preceding->definition.flow_column);
        cmark_strbuf_put(&term->content, preceding->content.ptr + offset, preceding->content.size - offset);
        cmark_strbuf_truncate(&preceding->content, offset);
        preceding->definition.hardbreak = true;
        if (preceding->flags & CMARK_NODE__OPEN) finalize(parser, preceding);
        preceding->end_line = term->start_line;
        preceding->end_column = 0;
        cmark_node_insert_after(preceding, term);
        term->definition.no_reference = true;
        parser->current = term;
      }
    }
    while (parser->current != parent && parser->current != parser->root)
      parser->current = finalize(parser, parser->current);
    if (parent->last_child != term || term->content.size == 0) { *matched = parent; return false; }
    spread = !omit_term && parent->type != CMARK_NODE_DEFINITION_DESCRIPTION && parser->line_number - term->end_line > 1;
    if (term->prev && term->prev->type == CMARK_NODE_DEFINITION_LIST) {
      list = term->prev;
      if (list->last_child) {
        list->last_child->end_line = term->start_line;
        list->last_child->end_column = (term->definition.term_start_column > 0 ? term->definition.term_start_column : term->start_column) - 1;
      }
      list->flags |= CMARK_NODE__OPEN;
    } else {
      list = make_block(parser->mem, CMARK_NODE_DEFINITION_LIST, term->start_line, term->definition.term_start_column > 0 ? term->definition.term_start_column : term->start_column);
      if (omit_term) {
        list->start_line = term->definition.flow_line;
        list->start_column = term->definition.flow_column;
        cmark_node_insert_after(term, list);
      }
      else cmark_node_insert_before(term, list);
    }
    // Each paragraph flow line is a separate term; inline parsing happens later.
    int line = term->start_line;
    for (bufsize_t start = 0; !omit_term && start < term->content.size; ) {
      bufsize_t end = start;
      while (end < term->content.size && !S_is_line_end_char(term->content.ptr[end])) end++;
      cmark_node *item = make_block(parser->mem, CMARK_NODE_DEFINITION_TERM, line,
                                   line == term->start_line ? term->start_column : 1);
      if (line == term->start_line && term->definition.term_start_column > 0) {
        item->internal_offset = item->start_column - term->definition.term_start_column;
        item->start_column = term->definition.term_start_column;
      }
      item->flags &= ~CMARK_NODE__OPEN;
      item->end_line = line;
      item->end_column = item->start_column + end - start - 1;
      bufsize_t content_end = end;
      bufsize_t after_ending = end;
      if (after_ending < term->content.size && term->content.ptr[after_ending] == '\r') after_ending++;
      if (after_ending < term->content.size && term->content.ptr[after_ending] == '\n') after_ending++;
      if (after_ending < term->content.size) {
        if (end - start >= 2 && term->content.ptr[end - 1] == ' ' && term->content.ptr[end - 2] == ' ') {
          item->definition.hardbreak = true;
        } else {
          int slashes = 0;
          while (content_end > start && term->content.ptr[content_end - 1] == '\\') { slashes++; content_end--; }
          content_end = end;
          if (slashes % 2) { item->definition.hardbreak = true; content_end--; }
        }
      }
      cmark_strbuf_put(&item->content, term->content.ptr + start, content_end - start);
      item->end_line = line + 1;
      item->end_column = 0;
      cmark_node_append_child(list, item);
      if (end < term->content.size && term->content.ptr[end] == '\r') end++;
      if (end < term->content.size && term->content.ptr[end] == '\n') end++;
      start = end;
      line++;
    }
    if (!omit_term) cmark_node_free(term);
    parser->current = parent;
  }
  int initial_column = parser->column;
  S_advance_offset(parser, input, parser->first_nonspace + 1 - parser->offset, false);
  int after_marker_offset = parser->offset;
  int after_marker_column = parser->column;
  bool partial_tab = parser->partially_consumed_tab;
  while (S_is_space_or_tab(peek_at(input, parser->offset))) S_advance_offset(parser, input, 1, true);
  bool empty = S_is_line_end_char(peek_at(input, parser->offset));
  if (parser->column - after_marker_column > 4 && !empty) {
    parser->offset = after_marker_offset;
    parser->column = after_marker_column;
    parser->partially_consumed_tab = partial_tab;
    S_advance_offset(parser, input, 1, true);
  }
  int padding = empty ? after_marker_column - initial_column + 1 : parser->column - initial_column;
  if (previous_description) {
    previous_description->end_line = parser->line_number;
    previous_description->end_column = empty ? after_marker_offset : parser->offset;
  }
  cmark_node *description = make_block(parser->mem, CMARK_NODE_DEFINITION_DESCRIPTION,
                                        parser->line_number, parser->offset + 1);
  if (empty) {
    description->start_column = after_marker_offset + 1;
    if (parser->definition_synthetic_ending && after_marker_offset == input->len - 1) {
      description->start_line = list->start_line;
      description->start_column = list->start_column;
    }
  }
  description->definition.padding = padding;
  description->definition.initial_blank = empty;
  description->definition.spread = spread;
  description->definition.interrupt = interrupt;
  cmark_node_append_child(list, description);
  parser->current = description;
  parser->definition_lazy_code = false;
  *matched = description;
  return true;
}

/* See http://spec.commonmark.org/0.24/#phase-1-block-structure */
static void S_process_line(cmark_parser *parser, const unsigned char *buffer,
                           bufsize_t bytes, bool ensureEndsInNewline) {
  cmark_node *last_matched_container;
  bool all_matched = true;
  cmark_node *container;
  cmark_chunk input;
  cmark_node *current;

  cmark_strbuf_clear(&parser->curline);
  parser->math_inline_prefix = 0;
  parser->math_eol_target = NULL;
  parser->code_footnote_blank_offset = -1;
  parser->code_footnote_blank_container = NULL;

  if (parser->options & CMARK_OPT_VALIDATE_UTF8)
    cmark_utf8proc_check(&parser->curline, buffer, bytes);
  else
    cmark_strbuf_put(&parser->curline, buffer, bytes);

  bytes = parser->curline.size;

  // ensure line ends with a newline:
  if (ensureEndsInNewline && (bytes == 0 || !S_is_line_end_char(parser->curline.ptr[bytes - 1])))
    cmark_strbuf_putc(&parser->curline, '\n');

  parser->offset = 0;
  parser->column = 0;
  parser->first_nonspace = 0;
  parser->first_nonspace_column = 0;
  parser->thematic_break_kill_pos = 0;
  parser->indent = 0;
  parser->blank = false;
  parser->partially_consumed_tab = false;
  parser->definition_lazy_code = false;

  input.data = parser->curline.ptr;
  input.len = parser->curline.size;
  input.alloc = 0;

  // Skip UTF-8 BOM.
  if (parser->line_number == 0 &&
      input.len >= 3 &&
      memcmp(input.data, "\xef\xbb\xbf", 3) == 0)
    parser->offset += 3;

  parser->line_number++;
  cmark_list_spread_line(parser);

  last_matched_container = check_open_blocks(parser, &input, &all_matched);

  if (!last_matched_container)
    goto finished;

  parser->definition_line_offset = parser->offset;
  if (S_open_definition(parser, &input, &last_matched_container)) {
    all_matched = true;
    cmark_list_spread_unsupported(parser);
  }
  parser->definition_line_offset = parser->offset;
  container = last_matched_container;

  current = parser->current;

  // Only open new blocks if we're not limited to inline
  if ((parser->options & CMARK_OPT_INLINE_ONLY) == 0)
    open_new_blocks(parser, &container, &input, all_matched);

  /* Web exits containers after this line's prefixes when input ends there. */
  cmark_list_spread_prefix_only(parser, input.len - parser->offset <= 1);

  /* parser->current might have changed if feed_reentrant was called */
  if (current == parser->current)
  add_text_to_container(parser, container, last_matched_container, &input);

  if (parser->current && parser->current->type == CMARK_NODE_CODE_BLOCK && parser->current->definition.single_line) {
    parser->current->end_line = parser->line_number + 1;
    parser->current = finalize(parser, parser->current);
  }
finished:
  cmark_list_spread_end_line(parser, parser->math_last_line_ending);
  parser->last_line_length = input.len;
  if (parser->last_line_length &&
      input.data[parser->last_line_length - 1] == '\n')
    parser->last_line_length -= 1;
  if (parser->last_line_length &&
      input.data[parser->last_line_length - 1] == '\r')
    parser->last_line_length -= 1;

  if ((parser->options & CMARK_OPT_FOOTNOTES) &&
      (parser->options & CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    for (cmark_node *node = parser->current; node; node = node->parent) {
      if (node->type == CMARK_NODE_FOOTNOTE_DEFINITION &&
          (!parser->blank || node->start_line == parser->line_number)) {
        node->end_line = parser->line_number;
        node->end_column = parser->last_line_length;
      }
    }
  }
  cmark_strbuf_clear(&parser->curline);
}

cmark_node *cmark_parser_finish(cmark_parser *parser) {
  cmark_node *res;
  cmark_llist *extensions;

  /* Parser was already finished once */
  if (parser->root == NULL)
    return NULL;

  if (parser->linebuf.size) {
    parser->definition_synthetic_ending = true;
    parser->math_last_line_ending = false;
    parser->math_current_ending = 0;
    S_process_line(parser, parser->linebuf.ptr, parser->linebuf.size, (parser->options & CMARK_OPT_PRESERVE_WHITESPACE) == 0);
    cmark_strbuf_clear(&parser->linebuf);
  }

  cmark_list_spread_eof(parser);
  finalize_document(parser);

  cmark_consolidate_text_nodes(parser->root);

  cmark_strbuf_free(&parser->curline);
  cmark_strbuf_free(&parser->linebuf);

#if CMARK_DEBUG_NODES
  if (cmark_node_check(parser->root, stderr)) {
    abort();
  }
#endif

  for (extensions = parser->syntax_extensions; extensions; extensions = extensions->next) {
    cmark_syntax_extension *ext = (cmark_syntax_extension *) extensions->data;
    if (ext->postprocess_func) {
      cmark_node *processed = ext->postprocess_func(ext, parser, parser->root);
      if (processed)
        parser->root = processed;
    }
  }

  cmark_list_spread_finish(parser, parser->root);

  res = parser->root;
  parser->root = NULL;

  cmark_parser_reset(parser);

  return res;
}

int cmark_parser_get_line_number(cmark_parser *parser) {
  return parser->line_number;
}

bufsize_t cmark_parser_get_offset(cmark_parser *parser) {
  return parser->offset;
}

bufsize_t cmark_parser_get_column(cmark_parser *parser) {
  return parser->column;
}

int cmark_parser_get_first_nonspace(cmark_parser *parser) {
  return parser->first_nonspace;
}

int cmark_parser_get_first_nonspace_column(cmark_parser *parser) {
  return parser->first_nonspace_column;
}

int cmark_parser_get_indent(cmark_parser *parser) {
  return parser->indent;
}

int cmark_parser_is_blank(cmark_parser *parser) {
  return parser->blank;
}

int cmark_parser_has_partially_consumed_tab(cmark_parser *parser) {
  return parser->partially_consumed_tab;
}

int cmark_parser_get_last_line_length(cmark_parser *parser) {
  return parser->last_line_length;
}

cmark_node *cmark_parser_add_child(cmark_parser *parser,
                                   cmark_node   *parent,
                                   cmark_node_type block_type,
                                   int start_column) {
  return add_child(parser, parent, block_type, start_column);
}

void cmark_parser_advance_offset(cmark_parser *parser,
                                 const char *input,
                                 int count,
                                 int columns) {
  cmark_chunk input_chunk = cmark_chunk_literal(input);

  S_advance_offset(parser, &input_chunk, count, columns != 0);
}

void cmark_parser_set_backslash_ispunct_func(cmark_parser *parser,
                                             cmark_ispunct_func func) {
  parser->backslash_ispunct = func;
}

cmark_llist *cmark_parser_get_syntax_extensions(cmark_parser *parser) {
  return parser->syntax_extensions;
}
