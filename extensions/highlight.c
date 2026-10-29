/*
 * AIMarkdown highlight extension: delimiter plumbing adapted from swift-cmark
 * strikethrough.c (see COPYING). Highlight matching rules
 * ported from ai-markdown remark-mark-highlight 1.0.3, MIT; see
 * AIMarkdown/ThirdPartyLicenses/ai-markdown/LICENSE-remark-mark-highlight.
 */
#include <stdbool.h>

#include "highlight.h"
#include <parser.h>
#include <render.h>

cmark_node_type CMARK_NODE_HIGHLIGHT;

// Web classifies the adjacent UTF-16 code unit; supplementary scalars are ordinary.
#include <stdint.h>
#include <utf8.h>
#include "highlight-unicode.inc"

static int character_class(int32_t code) {
  if (code < 0 || code == 9 || code == 10 || code == 11 || code == 12 ||
      code == 13 || code == 32 || code == 160 || code == 0x1680 ||
      (code >= 0x2000 && code <= 0x200a) || code == 0x2028 || code == 0x2029 ||
      code == 0x202f || code == 0x205f || code == 0x3000 || code == 0xfeff)
    return 1;
  if (code > 0xffff) return 0;
  size_t left = 0, right = sizeof(highlight_punctuation) / sizeof(highlight_punctuation[0]);
  while (left < right) {
    size_t middle = left + (right - left) / 2;
    if (code < highlight_punctuation[middle][0]) right = middle;
    else if (code > highlight_punctuation[middle][1]) left = middle + 1;
    else return 2;
  }
  return 0;
}

static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                        cmark_node *parent, unsigned char character,
                        cmark_inline_parser *inline_parser) {
  if (character != '=') return NULL;
  cmark_chunk *input = cmark_inline_parser_get_chunk(inline_parser);
  int start = cmark_inline_parser_get_offset(inline_parser), end = start;
  while (end < input->len && input->data[end] == '=') end++;
  // Consume the whole run: a long run may not restart at its final two equals.
  cmark_inline_parser_set_offset(inline_parser, end);
  cmark_node *res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  cmark_chunk text = cmark_chunk_dup(input, start, end - start);
  cmark_node_set_literal(res, cmark_chunk_to_cstr(parser->mem, &text));
  cmark_chunk_free(parser->mem, &text);
  res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
  res->end_column = cmark_inline_parser_get_column(inline_parser) - 1;
  res->start_column = res->end_column - (end - start) + 1;
  if (end - start != 2) return res;
  int32_t before = -1, after = -1;
  if (start > 0) {
    int previous = start - 1;
    while (previous > 0 && (input->data[previous] & 0xc0) == 0x80) previous--;
    cmark_utf8proc_iterate(input->data + previous, start - previous, &before);
  }
  if (end < input->len)
    cmark_utf8proc_iterate(input->data + end, input->len - end, &after);
  int b = character_class(before), a = character_class(after);
  int open = a == 0 || (a == 2 && b != 0);
  int close = b == 0 || (b == 2 && a != 0);
  if (open || close)
    cmark_inline_parser_push_delimiter(inline_parser, '=', open, close, res);
  return res;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *highlight;
  cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  highlight = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  if (!cmark_node_set_type(highlight, CMARK_NODE_HIGHLIGHT))
    goto done;

  cmark_node_set_syntax_extension(highlight, self);

  tmp = cmark_node_next(opener->inl_text);

  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = cmark_node_next(tmp);
    cmark_node_append_child(highlight, tmp);
    tmp = next;
  }

  highlight->end_line = closer->inl_text->end_line;
  highlight->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
  cmark_node_free(closer->inl_text);

done:
  delim = closer;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }

  cmark_inline_parser_remove_delimiter(inline_parser, opener);

  return res;
}

static const char *get_type_string(cmark_syntax_extension *extension,
                                   cmark_node *node) {
  return node->type == CMARK_NODE_HIGHLIGHT ? "highlight" : "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  if (node->type != CMARK_NODE_HIGHLIGHT)
    return false;

  return CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "==", false, LITERAL);
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    cmark_strbuf_puts(renderer->html, "<mark>");
  } else {
    cmark_strbuf_puts(renderer->html, "</mark>");
  }
}

static void plaintext_render(cmark_syntax_extension *extension,
                             cmark_renderer *renderer, cmark_node *node,
                             cmark_event_type ev_type, int options) {
  /* Plain text traverses children without adding highlight delimiters. */
}

cmark_syntax_extension *create_highlight_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("highlight");
  cmark_llist *special_chars = NULL;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);
  // These text formatters retain children without claiming native mark styling.
  cmark_syntax_extension_set_latex_render_func(ext, plaintext_render);
  cmark_syntax_extension_set_man_render_func(ext, plaintext_render);
  CMARK_NODE_HIGHLIGHT = cmark_syntax_extension_add_node(1);

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)'=');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  cmark_syntax_extension_set_emphasis(ext, 1);

  return ext;
}
