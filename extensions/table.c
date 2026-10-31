#include <stdbool.h>

#include <cmark-gfm-extension_api.h>
#include <html.h>
#include <inlines.h>
#include <parser.h>
#include <references.h>
#include <string.h>
#include <render.h>
#include <cmark_ctype.h>

#include "ext_scanners.h"
#include "strikethrough.h"
#include "highlight.h"
#include "table.h"
#include "cmark-gfm-core-extensions.h"

// Limit to prevent a malicious input from causing a denial of service.
#define MAX_AUTOCOMPLETED_CELLS 0x80000

// Custom node flag, initialized in `create_table_extension`.
static cmark_node_internal_flags CMARK_NODE__TABLE_VISITED;

cmark_node_type CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW,
    CMARK_NODE_TABLE_CELL;

typedef struct {
  unsigned colspan, rowspan;
  int cell_index;
} node_cell_data;

typedef struct {
  cmark_strbuf *buf;
  int start_offset, end_offset, internal_offset;
  node_cell_data *cell_data;
} node_cell;

typedef struct {
  uint16_t n_columns;
  int paragraph_offset;
  node_cell *cells;
} table_row;

typedef struct {
  uint16_t n_columns;
  uint8_t *alignments;
  int n_rows;
  int n_nonempty_cells;
} node_table;

typedef struct {
  bool is_header;
} node_table_row;

static void free_table_cell(cmark_mem *mem, node_cell *cell) {
  cmark_strbuf_free((cmark_strbuf *)cell->buf);
  mem->free(cell->buf);
  if (cell->cell_data)
    mem->free(cell->cell_data);
}

static void free_row_cells(cmark_mem *mem, table_row *row) {
  while (row->n_columns > 0) {
    free_table_cell(mem, &row->cells[--row->n_columns]);
  }
  mem->free(row->cells);
  row->cells = NULL;
}

static void free_table_row(cmark_mem *mem, table_row *row) {
  if (!row)
    return;

  free_row_cells(mem, row);
  mem->free(row);
}

static void free_node_table(cmark_mem *mem, void *ptr) {
  node_table *t = (node_table *)ptr;
  mem->free(t->alignments);
  mem->free(t);
}

static void free_node_table_row(cmark_mem *mem, void *ptr) {
  mem->free(ptr);
}

static void free_node_table_cell_data(cmark_mem *mem, void *data) {
  mem->free(data);
}

static int get_n_table_columns(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return -1;

  return (int)((node_table *)node->as.opaque)->n_columns;
}

static int set_n_table_columns(cmark_node *node, uint16_t n_columns) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->n_columns = n_columns;
  return 1;
}

// Increment the number of rows in the table. Also update n_nonempty_cells,
// which keeps track of the number of cells which were parsed from the
// input file. (If one of the rows is too short, then the trailing cells
// are autocompleted. Autocompleted cells are not counted in n_nonempty_cells.)
// The purpose of this is to prevent a malicious input from generating a very
// large number of autocompleted cells, which could cause a denial of service
// vulnerability.
static int incr_table_row_count(cmark_node *node, int i) {
  if (!node || node->type != CMARK_NODE_TABLE) {
    return 0;
  }

  ((node_table *)node->as.opaque)->n_rows++;
  ((node_table *)node->as.opaque)->n_nonempty_cells += i;
  return 1;
}

// Calculate the number of autocompleted cells.
static int get_n_autocompleted_cells(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE) {
    return 0;
  }

  const node_table *nt = (node_table *)node->as.opaque;
  return (nt->n_columns * nt->n_rows) - nt->n_nonempty_cells;
}

static uint8_t *get_table_alignments(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->alignments;
}

static int set_table_alignments(cmark_node *node, uint8_t *alignments) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->alignments = alignments;
  return 1;
}

static uint8_t get_cell_alignment(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 0;
  const uint8_t *alignments = get_table_alignments(node->parent->parent);
  int i = data->cell_index;
  return alignments && i >= 0 && i < get_n_table_columns(node->parent->parent)
             ? alignments[i]
             : 0;
}

static int set_cell_index(cmark_node *node, int i) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 0;
  data->cell_index = i;
  return 1;
}

static unsigned get_cell_colspan(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return UINT_MAX;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 1; // default to 1 in case the cell was created as filler on an incomplete row
  return data->colspan;
}

static unsigned get_cell_rowspan(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return UINT_MAX;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 1; // default to 1 in case the cell was created as filler on an incomplete row
  return data->rowspan;
}

static int set_cell_colspan(cmark_node *node, unsigned colspan) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 0;
  data->colspan = colspan;
  return 1;
}

static int set_cell_rowspan(cmark_node *node, unsigned rowspan) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 0;
  data->rowspan = rowspan;
  return 1;
}

static int increment_cell_rowspan(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node_cell_data *data = (node_cell_data *)node->as.opaque;
  if (!data)
    return 0;
  ++data->rowspan;
  return 1;
}

static cmark_strbuf *unescape_pipes(cmark_mem *mem, unsigned char *string, bufsize_t len, bool preserve)
{
  cmark_strbuf *res = (cmark_strbuf *)mem->calloc(1, sizeof(cmark_strbuf));
  bufsize_t r, w;

  cmark_strbuf_init(mem, res, len + 1);
  cmark_strbuf_put(res, string, len);
  cmark_strbuf_putc(res, '\0');

  for (r = 0, w = 0; r < len; ++r) {
    if (!preserve && res->ptr[r] == '\\' && res->ptr[r + 1] == '|')
      r++;

    res->ptr[w++] = res->ptr[r];
  }

  cmark_strbuf_truncate(res, w);

  return res;
}

// Adds a new cell to the end of the row. A pointer to the new cell is returned
// for the caller to initialize.
static node_cell* append_row_cell(cmark_mem *mem, table_row *row) {
  const uint32_t n_columns = row->n_columns + 1;
  // realloc when n_columns is a power of 2
  if ((n_columns & (n_columns-1)) == 0) {
    // make sure we never wrap row->n_columns
    // offset will != len and our exit will clean up as intended
    if (n_columns > UINT16_MAX) {
      return NULL;
    }
    // Use realloc to double the size of the buffer.
    row->cells = (node_cell *)mem->realloc(row->cells, (2 * n_columns - 1) * sizeof(node_cell));
  }
  row->n_columns = (uint16_t)n_columns;
  return &row->cells[n_columns-1];
}

/* micromark's table escape state consumes only backslash/backslash and
 * backslash/pipe pairs. This makes an even backslash run end before a divider. */
static bufsize_t scan_web_table_cell(const unsigned char *string, int len,
                                    bufsize_t offset) {
  bufsize_t start = offset;
  while (offset < len && string[offset] != '|' && string[offset] != '\r' &&
         string[offset] != '\n') {
    if (string[offset] == '\\' && offset + 1 < len &&
        (string[offset + 1] == '\\' || string[offset + 1] == '|'))
      offset++;
    offset++;
  }
  return offset - start;
}

static table_row *row_from_string(cmark_syntax_extension *self,
                                  cmark_parser *parser, unsigned char *string,
                                  int len, bool body_row) {
  // Parses a single table row. It has the following form:
  // `delim? table_cell (delim table_cell)* delim? newline`
  // Note that cells are allowed to be empty.
  //
  // From the GitHub-flavored Markdown specification:
  //
  // > Each row consists of cells containing arbitrary text, in which inlines
  // > are parsed, separated by pipes (|). A leading and trailing pipe is also
  // > recommended for clarity of reading, and if there’s otherwise parsing
  // > ambiguity.

  table_row *row = NULL;
  bufsize_t cell_matched = 1, pipe_matched = 1, offset;
  int expect_more_cells = 1;
  int row_end_offset = 0;
  int int_overflow_abort = 0;
  bool web_tables = (parser->options & CMARK_OPT_WEB_TABLES) != 0;

  row = (table_row *)parser->mem->calloc(1, sizeof(table_row));
  row->n_columns = 0;
  row->cells = NULL;

  // Scan past the (optional) leading pipe.
  offset = scan_table_cell_end(string, len, 0);

  // Parse the cells of the row. Stop if we reach the end of the input, or if we
  // cannot detect any more cells.
  while (offset < len && expect_more_cells) {
    cell_matched = web_tables ? scan_web_table_cell(string, len, offset)
                             : scan_table_cell(string, len, offset);
    pipe_matched = scan_table_cell_end(string, len, offset + cell_matched);

    bool empty_pipe_row = web_tables && body_row && row->n_columns == 0 &&
        string[row->paragraph_offset] == '|' &&
        scan_table_row_end(string, len, offset) == len - offset;
    if (cell_matched || pipe_matched || empty_pipe_row) {
      // We are guaranteed to have a cell, since (1) either we found some
      // content and cell_matched, (2) we found an empty cell followed by a
      // pipe, or (3) Web mode accepts a body row containing only its leading pipe.
      cmark_strbuf *cell_buf = unescape_pipes(parser->mem, string + offset,
          cell_matched, (parser->options & (CMARK_OPT_MATH | CMARK_OPT_REFERENCE_USAGES | CMARK_OPT_WEB_TABLES)) != 0);
      cmark_strbuf_trim(cell_buf);

      node_cell *cell = append_row_cell(parser->mem, row);
      if (!cell) {
        int_overflow_abort = 1;
        cmark_strbuf_free(cell_buf);
        parser->mem->free(cell_buf);
        break;
      }
      cell->buf = cell_buf;
      cell->start_offset = offset;
      if (cell_matched > 0)
        cell->end_offset = offset + cell_matched - 1;
      else
        cell->end_offset = offset;
      cell->internal_offset = 0;

      while (cell->start_offset > row->paragraph_offset && string[cell->start_offset - 1] != '|') {
        --cell->start_offset;
        ++cell->internal_offset;
      }
      cell->cell_data = (node_cell_data *)parser->mem->calloc(1, sizeof(node_cell_data));

      if (parser->options & CMARK_OPT_TABLE_SPANS) {
        // Check for a column-spanning cell
        if (row->n_columns > 0 && cmark_strbuf_len(cell->buf) == 0 && cell->start_offset == cell->end_offset) {
          cell->cell_data->colspan = 0;

          // find the last cell that isn't part of a colspan, and increment that colspan
          node_cell *colspan_cell = NULL;
          for (uint16_t i = 0; i < row->n_columns; i++) {
            node_cell *this_cell = &row->cells[i];
            if (this_cell->cell_data->colspan > 0)
              colspan_cell = this_cell;
          }
          if (colspan_cell)
            ++colspan_cell->cell_data->colspan;
        } else {
          cell->cell_data->colspan = 1;
        }

        // Check this cell for a row-span marker, so that the spanning cell's rowspan can be incremented later.
        cell->cell_data->rowspan = 1;
        if (parser->options & CMARK_OPT_TABLE_ROWSPAN_DITTO) {
          if (strcmp(cmark_strbuf_cstr(cell->buf), "\"") == 0) {
            cell->cell_data->rowspan = 0;
          }
        } else {
          if (strcmp(cmark_strbuf_cstr(cell->buf), "^") == 0) {
            cell->cell_data->rowspan = 0;
          }
        }
      } else {
        cell->cell_data->colspan = 1;
        cell->cell_data->rowspan = 1;
      }

      // make sure we never wrap row->n_columns
      // offset will != len and our exit will clean up as intended
      if (row->n_columns == UINT16_MAX) {
          int_overflow_abort = 1;
          break;
      }
    }

    offset += cell_matched + pipe_matched;

    if (pipe_matched) {
      expect_more_cells = 1;
    } else {
      // We've scanned the last cell. Check if we have reached the end of the row
      row_end_offset = scan_table_row_end(string, len, offset);
      offset += row_end_offset;

      // If the end of the row is not the end of the input,
      // the row is not a real row but potentially part of the paragraph
      // preceding the table.
      if (row_end_offset && offset != len) {
        row->paragraph_offset = offset;

        free_row_cells(parser->mem, row);

        // Scan past the (optional) leading pipe.
        offset += scan_table_cell_end(string, len, offset);

        expect_more_cells = 1;
      } else {
        expect_more_cells = 0;
      }
    }
  }

  if (offset != len || row->n_columns == 0 || int_overflow_abort) {
    free_table_row(parser->mem, row);
    row = NULL;
  }

  return row;
}

/* Web positions (CMARK_OPT_WEB_POSITIONS) from this row scan. micromark's
 * table row starts at the row's first non-whitespace character and ends at the
 * physical end of the line. A cell starts at its leading pipe, or at the row
 * start, and ends where the next scanned cell starts; the last scanned cell
 * ends at the row end. Cells beyond the header's columns, which this parser
 * drops, still bound the previous cell, and their number is recorded. */
static int web_cell_start(const unsigned char *string, const table_row *row,
                          const node_cell *cell) {
  return cell->start_offset > row->paragraph_offset && string[cell->start_offset - 1] == '|'
             ? cell->start_offset - 1
             : cell->start_offset;
}

static int web_line_end(const unsigned char *string, int len) {
  while (len > 0 && (string[len - 1] == '\n' || string[len - 1] == '\r'))
    len--;
  return len;
}

/* The cell scanner treats any backslash before a pipe as an escape, while
 * micromark reads `\\` as an escaped backslash followed by a divider; such
 * rows may split differently and get no Web evidence. */
static bool web_row_scan_comparable(const unsigned char *string, int from, int to) {
  for (int i = from; i < to; i++)
    if (string[i] == '|' && i >= from + 2 && string[i - 1] == '\\' && string[i - 2] == '\\')
      return false;
  return true;
}

/* `column` is the source column of string offset 0 on `line`. */
static void web_row_positions(cmark_node *row_node, const table_row *row,
                              const unsigned char *string, int line_end, int line,
                              int column, bool web_tables) {
  if (!web_tables && !web_row_scan_comparable(string, row->paragraph_offset, line_end))
    return;
  cmark_web_position_set_start(row_node, line, column + row->paragraph_offset);
  cmark_web_position_set_end(row_node, line, column + line_end);
  row_node->web_table_cells = row->n_columns;
  int i = 0;
  for (cmark_node *cell = row_node->first_child; cell && i < row->n_columns;
       cell = cell->next, i++) {
    int start = web_cell_start(string, row, &row->cells[i]);
    int end = i + 1 < row->n_columns ? web_cell_start(string, row, &row->cells[i + 1]) : line_end;
    cmark_web_position_set_start(cell, line, column + start);
    cmark_web_position_set_end(cell, line, column + end);
    /* Inline subjects retain exact original bytes in Web mode. Give each
     * subject its physical content anchor, including converted header rows. */
    const node_cell *scanned = &row->cells[i];
    int content = scanned->start_offset + scanned->internal_offset;
    if (web_tables && content >= row->paragraph_offset && content <= line_end &&
        scanned->buf->size <= line_end - content &&
        memcmp(string + content, scanned->buf->ptr, scanned->buf->size) == 0) {
      cell->math_lines = cmark_node_mem(cell)->calloc(1, sizeof(cmark_math_line));
      cell->math_line_count = cell->math_line_capacity = 1;
      cmark_math_line *record = cell->math_lines;
      record->line = line;
      record->column = column + content;
      record->visual_column = cell->reference_visual_column > 0
          ? cell->reference_visual_column : record->column;
      record->prefix_visual_column = record->visual_column;
    }
  }
}

/* Transfer physical line records before changing the paragraph into a table.
 * The retained paragraph has a trimmed subject but includes its original
 * trailing spaces in the block endpoint, just like micromark's paragraph. */
static void web_prefix_paragraph(cmark_node *paragraph, cmark_node *original,
                                  const unsigned char *string, int length) {
  int first = 0, last = length;
  while (first < last && cmark_isspace(string[first])) first++;
  while (last > first && cmark_isspace(string[last - 1])) last--;
  if (!original->math_line_count || last == first ||
      paragraph->content.size != last - first ||
      memcmp(paragraph->content.ptr, string + first, last - first) != 0)
    return;
  paragraph->math_lines = cmark_node_mem(paragraph)->calloc(original->math_line_count,
                                                      sizeof(cmark_math_line));
  paragraph->math_line_capacity = original->math_line_count;
  int physical_end = web_line_end(string, length);
  const cmark_math_line *last_physical = NULL;
  for (size_t i = 0; i < original->math_line_count; i++) {
    const cmark_math_line *row = &original->math_lines[i];
    if (row->offset < physical_end) last_physical = row;
    int next = i + 1 < original->math_line_count
        ? original->math_lines[i + 1].offset : length;
    if (row->offset >= last || next <= first) continue;
    int begin = row->offset > first ? row->offset : first;
    cmark_math_line *copy = &paragraph->math_lines[paragraph->math_line_count++];
    *copy = *row;
    copy->offset = begin - first;
    copy->column += begin - row->offset;
    copy->visual_column = cmark_reference_column(row->visual_column,
        string + row->offset, begin - row->offset);
    copy->prefix = cmark_chunk_dup(&row->prefix, 0, row->prefix.len);
    cmark_chunk_to_cstr(cmark_node_mem(paragraph), &copy->prefix);
  }
  if (paragraph->math_line_count && last_physical) {
    const cmark_math_line *start = paragraph->math_lines;
    paragraph->start_line = start->line;
    paragraph->start_column = start->column;
    paragraph->end_line = last_physical->line;
    paragraph->end_column = last_physical->column + physical_end - last_physical->offset - 1;
    cmark_web_position_set_start(paragraph, paragraph->start_line, paragraph->start_column);
    cmark_web_position_set_end(paragraph, paragraph->end_line, paragraph->end_column + 1);
  }
}

static cmark_node *try_inserting_table_header_paragraph(cmark_parser *parser,
                                                 cmark_node *parent_container,
                                                 unsigned char *parent_string,
                                                 int paragraph_offset) {
  cmark_node *paragraph;
  cmark_strbuf *paragraph_content;

  paragraph = cmark_node_new_with_mem(CMARK_NODE_PARAGRAPH, parser->mem);

  paragraph_content = unescape_pipes(parser->mem, parent_string, paragraph_offset, (parser->options & (CMARK_OPT_MATH | CMARK_OPT_REFERENCE_USAGES)) != 0);
  cmark_strbuf_trim(paragraph_content);
  cmark_node_set_string_content(paragraph, (char *) paragraph_content->ptr);
  cmark_strbuf_free(paragraph_content);
  parser->mem->free(paragraph_content);

  if (parser->options & CMARK_OPT_WEB_POSITIONS)
    web_prefix_paragraph(paragraph, parent_container, parent_string, paragraph_offset);
  if (!cmark_node_insert_before(parent_container, paragraph)) {
    cmark_node_free(paragraph);
    return NULL;
  }
  return paragraph;
}

static cmark_node *try_opening_table_header(cmark_syntax_extension *self,
                                            cmark_parser *parser,
                                            cmark_node *parent_container,
                                            unsigned char *input, int len) {
  cmark_node *table_header;
  table_row *header_row = NULL;
  table_row *delimiter_row = NULL;
  node_table_row *ntr;
  const char *parent_string;
  uint16_t i;

  if (parent_container->flags & CMARK_NODE__TABLE_VISITED) {
    return parent_container;
  }

  if (!scan_table_start(input, len, cmark_parser_get_first_nonspace(parser))) {
    return parent_container;
  }

  // Since scan_table_start was successful, we must have a delimiter row.
  delimiter_row = row_from_string(
    self, parser, input + cmark_parser_get_first_nonspace(parser),
    len - cmark_parser_get_first_nonspace(parser), false);
  // assert may be optimized out, don't rely on it for security boundaries
  if (!delimiter_row) {
      return parent_container;
  }

  assert(delimiter_row);

  if (parser->options & CMARK_OPT_REFERENCE_DEFINITIONS)
    cmark_resolve_reference_definitions(parser, parent_container);

  cmark_arena_push();

  // Check for a matching header row. We call `row_from_string` with the entire
  // (potentially long) parent container as input, but this should be safe since
  // `row_from_string` bails out early if it does not find a row.
  parent_string = cmark_node_get_string_content(parent_container);
  header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                               (int)strlen(parent_string), false);
  if (!header_row || header_row->n_columns != delimiter_row->n_columns) {
    free_table_row(parser->mem, delimiter_row);
    free_table_row(parser->mem, header_row);
    cmark_arena_pop();
    parent_container->flags |= CMARK_NODE__TABLE_VISITED;
    return parent_container;
  }

  if (cmark_arena_pop()) {
    delimiter_row = row_from_string(
        self, parser, input + cmark_parser_get_first_nonspace(parser),
        len - cmark_parser_get_first_nonspace(parser), false);
    header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                                 (int)strlen(parent_string), false);
    // row_from_string can return NULL, add additional check to ensure n_columns match
    if (!delimiter_row || !header_row || header_row->n_columns != delimiter_row->n_columns) {
        free_table_row(parser->mem, delimiter_row);
        free_table_row(parser->mem, header_row);
        return parent_container;
    }
  }

  /* The header row is the paragraph's last content line: resolve its physical
   * start before the type change releases the paragraph's line records. */
  int web_header_line = 0, web_header_column = 0, web_header_visual_column = 0;
  if (parser->options & CMARK_OPT_WEB_POSITIONS) {
    for (size_t r = 0; r < parent_container->math_line_count; r++) {
      const cmark_math_line *row = &parent_container->math_lines[r];
      if (row->offset == header_row->paragraph_offset) {
        web_header_line = row->line;
        web_header_column = row->column - row->offset;
        web_header_visual_column = row->visual_column;
      }
    }
  }

  cmark_node *prefix = NULL;
  if (header_row->paragraph_offset)
    prefix = try_inserting_table_header_paragraph(parser, parent_container,
        (unsigned char *)parent_string, header_row->paragraph_offset);

  if (!cmark_node_set_type(parent_container, CMARK_NODE_TABLE)) {
    if (prefix) cmark_node_free(prefix);
    free_table_row(parser->mem, header_row);
    free_table_row(parser->mem, delimiter_row);
    return parent_container;
  }

  cmark_node_set_syntax_extension(parent_container, self);
  parent_container->as.opaque = parser->mem->calloc(1, sizeof(node_table));
  set_n_table_columns(parent_container, header_row->n_columns);

  // allocate alignments based on delimiter_row->n_columns
  // since we populate the alignments array based on delimiter_row->cells
  uint8_t *alignments =
      (uint8_t *)parser->mem->calloc(delimiter_row->n_columns, sizeof(uint8_t));
  for (i = 0; i < delimiter_row->n_columns; ++i) {
    node_cell *node = &delimiter_row->cells[i];
    bool left = node->buf->ptr[0] == ':', right = node->buf->ptr[node->buf->size - 1] == ':';

    if (left && right)
      alignments[i] = 'c';
    else if (left)
      alignments[i] = 'l';
    else if (right)
      alignments[i] = 'r';
  }
  set_table_alignments(parent_container, alignments);

  table_header =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             parent_container->start_column);
  cmark_node_set_syntax_extension(table_header, self);
  table_header->end_column = parent_container->start_column + (int)strlen(parent_string) - 2;
  table_header->start_line = table_header->end_line = parent_container->start_line;

  table_header->as.opaque = ntr = (node_table_row *)parser->mem->calloc(1, sizeof(node_table_row));
  ntr->is_header = true;

  int reference_header_offset = 0;
  int reference_header_column = parent_container->start_column;
  if (web_header_line) {
    reference_header_offset = header_row->paragraph_offset;
    reference_header_column = web_header_visual_column;
  }
  for (i = 0; i < header_row->n_columns; ++i) {
    node_cell *cell = &header_row->cells[i];
    cmark_node *header_cell = cmark_parser_add_child(parser, table_header,
        CMARK_NODE_TABLE_CELL, parent_container->start_column + cell->start_offset);
    header_cell->start_line = header_cell->end_line = parent_container->start_line;
    header_cell->internal_offset = cell->internal_offset;
    if (parser->options & CMARK_OPT_REFERENCE_USAGES) {
      int target = cell->start_offset + cell->internal_offset;
      reference_header_column = cmark_reference_column(reference_header_column, (const unsigned char *)parent_string + reference_header_offset, target - reference_header_offset);
      reference_header_offset = target;
      header_cell->reference_visual_column = reference_header_column;
    }
    header_cell->end_column = parent_container->start_column + cell->end_offset;
    header_cell->as.opaque = cell->cell_data;
    cell->cell_data = NULL;
    cmark_node_set_string_content(header_cell, (char *) cell->buf->ptr);
    cmark_node_set_syntax_extension(header_cell, self);
    set_cell_index(header_cell, i);
  }

  incr_table_row_count(parent_container, i);

  if (web_header_line) {
    const unsigned char *string = (const unsigned char *)parent_string;
    web_row_positions(table_header, header_row, string,
                      web_line_end(string, (int)strlen(parent_string)), web_header_line,
                      web_header_column, (parser->options & CMARK_OPT_WEB_TABLES) != 0);
    cmark_web_position_set_start(parent_container, web_header_line,
                                 web_header_column + header_row->paragraph_offset);
    cmark_web_position_set_end(parent_container, cmark_parser_get_line_number(parser),
                               web_line_end(input, len) + 1);
  }

  cmark_parser_advance_offset(
      parser, (char *)input,
      (int)strlen((char *)input) - 1 - cmark_parser_get_offset(parser), false);

  free_table_row(parser->mem, header_row);
  free_table_row(parser->mem, delimiter_row);
  return parent_container;
}

static cmark_node *try_opening_table_row(cmark_syntax_extension *self,
                                         cmark_parser *parser,
                                         cmark_node *parent_container,
                                         unsigned char *input, int len) {
  cmark_node *table_row_block;
  table_row *row;
  int reference_row_offset = 0, reference_row_column = 1;

  if (cmark_parser_is_blank(parser))
    return NULL;

  if (!(parser->options & CMARK_OPT_WEB_TABLES) &&
      get_n_autocompleted_cells(parent_container) > MAX_AUTOCOMPLETED_CELLS) {
    return NULL;
  }

  table_row_block =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             parent_container->start_column);
  cmark_node_set_syntax_extension(table_row_block, self);
  table_row_block->end_column = parent_container->end_column;
  table_row_block->as.opaque = parser->mem->calloc(1, sizeof(node_table_row));

  row = row_from_string(self, parser, input + cmark_parser_get_first_nonspace(parser),
      len - cmark_parser_get_first_nonspace(parser), true);

  if (!row) {
      // clean up the dangling node
      cmark_node_free(table_row_block);
      return NULL;
  }

  int table_columns = get_n_table_columns(parent_container);
  int row_columns = (parser->options & CMARK_OPT_WEB_TABLES) ? row->n_columns : table_columns;

  if (parser->options & CMARK_OPT_TABLE_SPANS) {
    // Check the new row for rowspan markers and increment the rowspan of the cell it's merging with
    int i;

    for (i = 0; i < row->n_columns && i < row_columns; ++i) {
      node_cell *this_cell = &row->cells[i];
      if (this_cell->cell_data->rowspan == 0) {
        // Rowspan marker. Scan up through previous rows and increment the spanning cell's rowspan
        cmark_node *check_row = table_row_block->prev;
        cmark_node *spanning_cell = NULL;
        while (check_row && !spanning_cell) {
          cmark_node *check_cell = cmark_node_nth_child(check_row, i);
          unsigned check_rowspan = get_cell_rowspan(check_cell);
          if (check_rowspan == 0) {
            check_row = check_row->prev;
          } else {
            spanning_cell = check_cell;
          }
        }
        if (spanning_cell) {
          increment_cell_rowspan(spanning_cell);
          // The rowspan marker cell still has the ^/" marker, clear it out so it won't display
          cmark_strbuf_truncate(this_cell->buf, 0);
        }
      }
    }
  }

  {
    int i;

    for (i = 0; i < row->n_columns && i < row_columns; ++i) {
      node_cell *cell = &row->cells[i];
      cmark_node *node = cmark_parser_add_child(parser, table_row_block,
          CMARK_NODE_TABLE_CELL, parent_container->start_column + cell->start_offset);
      node->internal_offset = cell->internal_offset;
      if (parser->options & CMARK_OPT_REFERENCE_USAGES) {
        int target = cmark_parser_get_first_nonspace(parser) + cell->start_offset + cell->internal_offset;
        reference_row_column = cmark_reference_column(reference_row_column, input + reference_row_offset, target - reference_row_offset);
        reference_row_offset = target;
        node->reference_visual_column = reference_row_column;
      }
      node->end_column = parent_container->start_column + cell->end_offset;
      if (parser->options & CMARK_OPT_REFERENCE_USAGES) {
        node->start_column = cmark_parser_get_first_nonspace(parser) + 1 + cell->start_offset;
        node->end_column = cmark_parser_get_first_nonspace(parser) + 1 + cell->end_offset;
      }
      node->as.opaque = cell->cell_data;
      cell->cell_data = NULL;
      cmark_node_set_string_content(node, (char *) cell->buf->ptr);
      cmark_node_set_syntax_extension(node, self);
      set_cell_index(node, i);
    }

    incr_table_row_count(parent_container, i);

    for (; i < row_columns; ++i) {
      cmark_node *node = cmark_parser_add_child(
          parser, table_row_block, CMARK_NODE_TABLE_CELL, 0);
      cmark_node_set_syntax_extension(node, self);
      set_cell_index(node, i);
    }

    if ((parser->options & CMARK_OPT_WEB_POSITIONS) &&
        (parent_container->web_position_flags & CMARK_WEB_POSITION_END)) {
      int first_nonspace = cmark_parser_get_first_nonspace(parser);
      const unsigned char *string = input + first_nonspace;
      int line_end = web_line_end(string, len - first_nonspace);
      web_row_positions(table_row_block, row, string, line_end,
                        cmark_parser_get_line_number(parser), first_nonspace + 1,
                        (parser->options & CMARK_OPT_WEB_TABLES) != 0);
      cmark_web_position_set_end(parent_container, cmark_parser_get_line_number(parser),
                                 first_nonspace + 1 + line_end);
    }
  }

  free_table_row(parser->mem, row);

  cmark_parser_advance_offset(parser, (char *)input,
                              len - 1 - cmark_parser_get_offset(parser), false);

  return table_row_block;
}

static cmark_node *try_opening_table_block(cmark_syntax_extension *self,
                                           int indented, cmark_parser *parser,
                                           cmark_node *parent_container,
                                           unsigned char *input, int len) {
  cmark_node_type parent_type = cmark_node_get_type(parent_container);

  if (!indented && parent_type == CMARK_NODE_PARAGRAPH) {
    return try_opening_table_header(self, parser, parent_container, input, len);
  } else if (!indented && parent_type == CMARK_NODE_TABLE) {
    return try_opening_table_row(self, parser, parent_container, input, len);
  }

  return NULL;
}

static int matches(cmark_syntax_extension *self, cmark_parser *parser,
                   unsigned char *input, int len,
                   cmark_node *parent_container) {
  int res = 0;

  if (cmark_node_get_type(parent_container) == CMARK_NODE_TABLE) {
    cmark_arena_push();
    table_row *new_row = row_from_string(
        self, parser, input + cmark_parser_get_first_nonspace(parser),
        len - cmark_parser_get_first_nonspace(parser), true);
    if (new_row && new_row->n_columns)
      res = 1;
    free_table_row(parser->mem, new_row);
    cmark_arena_pop();
  }

  return res;
}

static const char *get_type_string(cmark_syntax_extension *self,
                                   cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    return "table";
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (((node_table_row *)node->as.opaque)->is_header)
      return "table_header";
    else
      return "table_row";
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return "table_cell";
  }

  return "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  if (node->type == CMARK_NODE_TABLE) {
    return child_type == CMARK_NODE_TABLE_ROW;
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    return child_type == CMARK_NODE_TABLE_CELL;
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return child_type == CMARK_NODE_MATH_INLINE || child_type == CMARK_NODE_TEXT || child_type == CMARK_NODE_CODE ||
           child_type == CMARK_NODE_EMPH || child_type == CMARK_NODE_STRONG ||
           child_type == CMARK_NODE_LINK || child_type == CMARK_NODE_IMAGE || child_type == CMARK_NODE_ATTRIBUTE ||
           child_type == CMARK_NODE_STRIKETHROUGH || child_type == CMARK_NODE_HIGHLIGHT ||
           child_type == CMARK_NODE_HTML_INLINE ||
           child_type == CMARK_NODE_FOOTNOTE_REFERENCE;
  }
  return false;
}

static int contains_inlines(cmark_syntax_extension *extension,
                            cmark_node *node) {
  return node->type == CMARK_NODE_TABLE_CELL;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    renderer->blankline(renderer);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      renderer->cr(renderer);
      renderer->out(renderer, node, "|", false, LITERAL);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    unsigned colspan = get_cell_colspan(node);
    unsigned rowspan = get_cell_rowspan(node);
    if (entering) {
      if (colspan > 0) {
        renderer->out(renderer, node, " ", false, LITERAL);
        if (rowspan == 0) {
          if (options & CMARK_OPT_TABLE_ROWSPAN_DITTO) {
            renderer->out(renderer, node, "\"", false, LITERAL);
          } else {
            renderer->out(renderer, node, "^", false, LITERAL);
          }
        }
      }
    } else {
      if (colspan > 0) {
        renderer->out(renderer, node, " ", false, LITERAL);
      }
      renderer->out(renderer, node, "|", false, LITERAL);
      if (((node_table_row *)node->parent->as.opaque)->is_header &&
          !node->next) {
        int i;
        uint8_t *alignments = get_table_alignments(node->parent->parent);
        uint16_t n_cols =
            ((node_table *)node->parent->parent->as.opaque)->n_columns;
        renderer->cr(renderer);
        renderer->out(renderer, node, "|", false, LITERAL);
        for (i = 0; i < n_cols; i++) {
          switch (alignments[i]) {
          case 0:   renderer->out(renderer, node, " --- |", false, LITERAL); break;
          case 'l': renderer->out(renderer, node, " :-- |", false, LITERAL); break;
          case 'c': renderer->out(renderer, node, " :-: |", false, LITERAL); break;
          case 'r': renderer->out(renderer, node, " --: |", false, LITERAL); break;
          }
        }
        renderer->cr(renderer);
      }
    }
  } else {
    assert(false);
  }
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{table}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{tabular}{", false, LITERAL);

      n_cols = ((node_table *)node->as.opaque)->n_columns;
      for (i = 0; i < n_cols; i++) {
        switch(alignments[i]) {
        case 0:
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }
      renderer->out(renderer, node, "}", false, LITERAL);
      renderer->cr(renderer);
    } else {
      renderer->out(renderer, node, "\\end{tabular}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\end{table}", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering) {
      if (node->next) {
        renderer->out(renderer, node, " & ", false, LITERAL);
      } else {
        renderer->out(renderer, node, " \\\\", false, LITERAL);
      }
    }
  } else {
    assert(false);
  }
}

static const char *xml_attr(cmark_syntax_extension *extension,
                            cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE_CELL) {
    if (cmark_gfm_extensions_get_table_row_is_header(node->parent)) {
      switch (get_cell_alignment(node)) {
      case 'l': return " align=\"left\"";
      case 'c': return " align=\"center\"";
      case 'r': return " align=\"right\"";
      }
    } else {
      unsigned colspan = get_cell_colspan(node);
      unsigned rowspan = get_cell_rowspan(node);
      // XXX: The extension API doesn't allow you to return a dynamic string without leaking it, so
      // specific column- and row-span information isn't printed
      if (colspan == 0) {
        return " colspan_filler";
      } else if (rowspan == 0) {
        return " rowspan_filler";
      } else if (colspan > 1 && rowspan > 1) {
        return " colspan rowspan";
      } else if (colspan > 1) {
        return " colspan";
      } else if (rowspan > 1) {
        return " rowspan";
      }
    }
  }

  return NULL;
}

static void man_render(cmark_syntax_extension *extension,
                       cmark_renderer *renderer, cmark_node *node,
                       cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, ".TS", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "tab(@);", false, LITERAL);
      renderer->cr(renderer);

      n_cols = ((node_table *)node->as.opaque)->n_columns;

      for (i = 0; i < n_cols; i++) {
        switch (alignments[i]) {
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 0:
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }

      if (n_cols) {
        renderer->out(renderer, node, ".", false, LITERAL);
        renderer->cr(renderer);
      }
    } else {
      renderer->out(renderer, node, ".TE", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering && node->next) {
      renderer->out(renderer, node, "@", false, LITERAL);
    }
  } else {
    assert(false);
  }
}

static void html_table_add_align(cmark_strbuf* html, const char* align, int options) {
  if (options & CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES) {
    cmark_strbuf_puts(html, " style=\"text-align: ");
    cmark_strbuf_puts(html, align);
    cmark_strbuf_puts(html, "\"");
  } else {
    cmark_strbuf_puts(html, " align=\"");
    cmark_strbuf_puts(html, align);
    cmark_strbuf_puts(html, "\"");
  }
}

static void html_table_add_spans(cmark_strbuf *html, unsigned colspan, unsigned rowspan) {
  if (colspan > 1) {
    char n[32];
    snprintf(n, sizeof(n), "%d", colspan);
    cmark_strbuf_puts(html, " colspan=\"");
    cmark_strbuf_puts(html, n);
    cmark_strbuf_puts(html, "\"");
  }
  if (rowspan > 1) {
    char n[32];
    snprintf(n, sizeof(n), "%d", rowspan);
    cmark_strbuf_puts(html, " rowspan=\"");
    cmark_strbuf_puts(html, n);
    cmark_strbuf_puts(html, "\"");
  }
}

struct html_table_state {
  unsigned need_closing_table_body : 1;
  unsigned in_table_header : 1;
};

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  cmark_strbuf *html = renderer->html;

  // XXX: we just monopolise renderer->opaque.
  struct html_table_state *table_state =
      (struct html_table_state *)&renderer->opaque;

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "<table");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
      table_state->need_closing_table_body = false;
    } else {
      if (table_state->need_closing_table_body) {
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "</tbody>");
        cmark_html_render_cr(html);
      }
      table_state->need_closing_table_body = false;
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "</table>");
      cmark_html_render_cr(html);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      cmark_html_render_cr(html);
      if (((node_table_row *)node->as.opaque)->is_header) {
        table_state->in_table_header = 1;
        cmark_strbuf_puts(html, "<thead>");
        cmark_html_render_cr(html);
      } else if (!table_state->need_closing_table_body) {
        cmark_strbuf_puts(html, "<tbody>");
        cmark_html_render_cr(html);
        table_state->need_closing_table_body = 1;
      }
      cmark_strbuf_puts(html, "<tr");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "</tr>");
      if (((node_table_row *)node->as.opaque)->is_header) {
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "</thead>");
        table_state->in_table_header = false;
      }
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    unsigned colspan = get_cell_colspan(node);
    unsigned rowspan = get_cell_rowspan(node);
    // A value of zero in either span means that this cell is a "filler" cell. Don't render those.
    if (colspan > 0 && rowspan > 0) {
      if (entering) {
        cmark_html_render_cr(html);
        if (table_state->in_table_header) {
          cmark_strbuf_puts(html, "<th");
        } else {
          cmark_strbuf_puts(html, "<td");
        }

        switch (get_cell_alignment(node)) {
          case 'l': html_table_add_align(html, "left", options); break;
          case 'c': html_table_add_align(html, "center", options); break;
          case 'r': html_table_add_align(html, "right", options); break;
        }

        html_table_add_spans(html, colspan, rowspan);

        cmark_html_render_sourcepos(node, html, options);
        cmark_strbuf_putc(html, '>');
      } else {
        if (table_state->in_table_header) {
          cmark_strbuf_puts(html, "</th>");
        } else {
          cmark_strbuf_puts(html, "</td>");
        }
      }
    }
  } else {
    assert(false);
  }
}

static void opaque_alloc(cmark_syntax_extension *self, cmark_mem *mem, cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    node->as.opaque = mem->calloc(1, sizeof(node_table));
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    node->as.opaque = mem->calloc(1, sizeof(node_table_row));
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    node->as.opaque = mem->calloc(1, sizeof(node_cell_data));
  }
}

static void opaque_free(cmark_syntax_extension *self, cmark_mem *mem, cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    free_node_table(mem, node->as.opaque);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    free_node_table_row(mem, node->as.opaque);
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    free_node_table_cell_data(mem, node->as.opaque);
  }
}

static int escape(cmark_syntax_extension *self, cmark_node *node, int c) {
  return
    node->type != CMARK_NODE_TABLE &&
    node->type != CMARK_NODE_TABLE_ROW &&
    node->type != CMARK_NODE_TABLE_CELL &&
    c == '|';
}

cmark_syntax_extension *create_table_extension(void) {
  cmark_syntax_extension *self = cmark_syntax_extension_new("table");

  cmark_register_node_flag(&CMARK_NODE__TABLE_VISITED);
  cmark_syntax_extension_set_match_block_func(self, matches);
  cmark_syntax_extension_set_open_block_func(self, try_opening_table_block);
  cmark_syntax_extension_set_get_type_string_func(self, get_type_string);
  cmark_syntax_extension_set_can_contain_func(self, can_contain);
  cmark_syntax_extension_set_contains_inlines_func(self, contains_inlines);
  cmark_syntax_extension_set_commonmark_render_func(self, commonmark_render);
  cmark_syntax_extension_set_plaintext_render_func(self, commonmark_render);
  cmark_syntax_extension_set_latex_render_func(self, latex_render);
  cmark_syntax_extension_set_xml_attr_func(self, xml_attr);
  cmark_syntax_extension_set_man_render_func(self, man_render);
  cmark_syntax_extension_set_html_render_func(self, html_render);
  cmark_syntax_extension_set_opaque_alloc_func(self, opaque_alloc);
  cmark_syntax_extension_set_opaque_free_func(self, opaque_free);
  cmark_syntax_extension_set_commonmark_escape_func(self, escape);
  CMARK_NODE_TABLE = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_ROW = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_CELL = cmark_syntax_extension_add_node(0);

  return self;
}

CMARK_GFM_EXPORT
uint16_t cmark_gfm_extensions_get_table_columns(cmark_node *node) {
  if (node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->n_columns;
}

CMARK_GFM_EXPORT
uint8_t *cmark_gfm_extensions_get_table_alignments(cmark_node *node) {
  if (node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->alignments;
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_set_table_columns(cmark_node *node, uint16_t n_columns) {
  return set_n_table_columns(node, n_columns);
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_set_table_alignments(cmark_node *node, uint16_t ncols, uint8_t *alignments) {
  uint8_t *a = (uint8_t *)cmark_node_mem(node)->calloc(1, ncols);
  memcpy(a, alignments, ncols);
  return set_table_alignments(node, a);
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_get_table_row_is_header(cmark_node *node)
{
  if (!node || node->type != CMARK_NODE_TABLE_ROW)
    return 0;

  return ((node_table_row *)node->as.opaque)->is_header;
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_set_table_row_is_header(cmark_node *node, int is_header)
{
  if (!node || node->type != CMARK_NODE_TABLE_ROW)
    return 0;

  ((node_table_row *)node->as.opaque)->is_header = (is_header != 0);
  return 1;
}

CMARK_GFM_EXPORT
unsigned cmark_gfm_extensions_get_table_cell_colspan(cmark_node *node)
{
  return get_cell_colspan(node);
}

CMARK_GFM_EXPORT
unsigned cmark_gfm_extensions_get_table_cell_rowspan(cmark_node *node)
{
  return get_cell_rowspan(node);
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_set_table_cell_colspan(cmark_node *node, unsigned colspan)
{
  return set_cell_colspan(node, colspan);
}

CMARK_GFM_EXPORT
int cmark_gfm_extensions_set_table_cell_rowspan(cmark_node *node, unsigned rowspan)
{
  return set_cell_rowspan(node, rowspan);
}
