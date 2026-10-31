#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMARK_NO_SHORT_NAMES
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include "node.h"
#include "harness.h"

/* Expected cells and positions come from mdast-util-from-markdown 2.0.3,
 * mdast-util-gfm-table 2.0.0 and micromark-extension-gfm-table 2.1.1 in
 * ai-markdown/ai-markdown e9ec1799fdc380d99ce0a6313023515895bbd7bf.
 * The escaped-code cases also occur in the downstream frozen
 * TableCodeMetadata table/body/root/{1,2,3}/1/0/LF oracle. */
static const struct {
  const char *row;
  int cells;
  const char *values[3];
  int starts[3], ends[3];
} cases[] = {
  {"|", 1, {NULL}, {1}, {2}},
  {"| ", 1, {NULL}, {1}, {3}},
  {"||", 1, {NULL}, {1}, {3}},
  {"|||", 2, {NULL, NULL}, {1, 2}, {2, 4}},
  {"a", 1, {"a"}, {1}, {2}},
  {"a|", 1, {"a"}, {1}, {3}},
  {"|a", 1, {"a"}, {1}, {3}},
  {"|a|b|c|", 3, {"a", "b", "c"}, {1, 3, 5}, {3, 5, 8}},
  {"| `a\\|b` | Y |", 2, {"a|b", "Y"}, {1, 10}, {10, 15}},
  {"| `a\\\\|b` | Y |", 3, {"`a\\", "b`", "Y"}, {1, 7, 11}, {7, 11, 16}},
  {"| `a\\\\\\|b` | Y |", 2, {"a\\\\|b", "Y"}, {1, 12}, {12, 17}}
};

static char *source_for_row(const char *row, const char *ending,
                            const char *prefix) {
  const char *header = "| H | X |", *delimiter = "| - | - |";
  size_t size = strlen(header) + strlen(delimiter) + strlen(row) +
      2 * strlen(ending) + 3 * strlen(prefix) + 1;
  char *source = malloc(size);
  snprintf(source, size, "%s%s%s%s%s%s%s%s", prefix, header, ending,
           prefix, delimiter, ending, prefix, row);
  return source;
}

static cmark_node *parse(const char *source, int options, bool chunked) {
  cmark_parser *parser = cmark_parser_new(options);
  cmark_parser_attach_syntax_extension(parser, cmark_find_syntax_extension("table"));
  if (chunked) {
    for (size_t i = 0; source[i]; i++)
      cmark_parser_feed(parser, source + i, 1);
  } else {
    cmark_parser_feed(parser, source, strlen(source));
  }
  cmark_node *document = cmark_parser_finish(parser);
  cmark_parser_free(parser);
  return document;
}

static cmark_node *find_table(cmark_node *document) {
  cmark_iter *iter = cmark_iter_new(document);
  cmark_node *table = NULL;
  while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
    cmark_node *node = cmark_iter_get_node(iter);
    if (strcmp(cmark_node_get_type_string(node), "table") == 0) {
      table = node;
      break;
    }
  }
  cmark_iter_free(iter);
  return table;
}

static int child_count(cmark_node *node) {
  int count = 0;
  for (cmark_node *child = cmark_node_first_child(node); child;
       child = cmark_node_next(child))
    count++;
  return count;
}

static void rows(test_batch_runner *runner) {
  const char *endings[] = {"\n", "\r\n", "\r"};
  const char *prefixes[] = {"", "> "};
  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
    for (size_t e = 0; e < 3; e++) {
      for (size_t p = 0; p < 2; p++) {
        for (int chunked = 0; chunked <= 1; chunked++) {
          char *source = source_for_row(cases[c].row, endings[e], prefixes[p]);
          cmark_node *document = parse(source, CMARK_OPT_WEB_TABLES |
              CMARK_OPT_WEB_POSITIONS, chunked);
          cmark_node *table = find_table(document);
          cmark_node *row = table ? cmark_node_last_child(table) : NULL;
          OK(runner, table != NULL, "Web table %zu/%zu/%zu/%d", c, e, p, chunked);
          if (table) {
            INT_EQ(runner, child_count(table), 2, "bare dividers continue a table");
            INT_EQ(runner, child_count(row), cases[c].cells, "actual cells retained");
            INT_EQ(runner, cmark_node_get_web_table_cells(row), cases[c].cells,
                   "cell metadata matches actual children");
            int i = 0;
            for (cmark_node *cell = cmark_node_first_child(row); cell && i < cases[c].cells;
                 cell = cmark_node_next(cell), i++) {
              cmark_node *inline_node = cmark_node_first_child(cell);
              if (cases[c].values[i]) {
                STR_EQ(runner, inline_node ? cmark_node_get_literal(inline_node) : "",
                       cases[c].values[i], "cell content %zu/%d", c, i);
              } else {
                OK(runner, inline_node == NULL, "empty cell has no inline nodes");
              }
              int line, start, end;
              INT_EQ(runner, cmark_node_get_web_position(cell, &line, &start, NULL, &end),
                     3, "all raw cells have positions");
              INT_EQ(runner, line, 3, "cell physical line");
              INT_EQ(runner, start, cases[c].starts[i] + (int)strlen(prefixes[p]),
                     "cell starts at its divider");
              INT_EQ(runner, end, cases[c].ends[i] + (int)strlen(prefixes[p]),
                     "cell ends at the next divider or row end");
            }
            char *html = cmark_render_html(document, CMARK_OPT_DEFAULT, NULL);
            OK(runner, html != NULL, "extra cells render with bounded alignments");
            free(html);
          }
          cmark_node_free(document);
          free(source);
        }
      }
    }
  }
}

static void opt_in(test_batch_runner *runner) {
  char *source = source_for_row("|a|b|c|", "\n", "");
  cmark_node *document = parse(source, CMARK_OPT_DEFAULT, false);
  cmark_node *table = find_table(document);
  INT_EQ(runner, child_count(cmark_node_last_child(table)), 2,
         "default still truncates rows to header width");
  cmark_node_free(document);
  document = parse(source, CMARK_OPT_WEB_TABLES, false);
  table = find_table(document);
  INT_EQ(runner, child_count(cmark_node_last_child(table)), 3,
         "Web tables do not require metadata options");
  cmark_node_free(document);
  free(source);

  source = source_for_row("a", "\n", "");
  document = parse(source, CMARK_OPT_DEFAULT, false);
  table = find_table(document);
  INT_EQ(runner, child_count(cmark_node_last_child(table)), 2,
         "default still pads short rows");
  cmark_node_free(document);
  free(source);

  source = source_for_row("|", "\n", "");
  document = parse(source, CMARK_OPT_DEFAULT, false);
  table = find_table(document);
  INT_EQ(runner, child_count(table), 1, "default still ends table before bare pipe");
  cmark_node_free(document);
  free(source);

  static const char *invalid_headers[] = {"|\n|-|\n", "| \n|-|\n"};
  for (size_t i = 0; i < sizeof(invalid_headers) / sizeof(invalid_headers[0]); i++) {
    document = parse(invalid_headers[i], CMARK_OPT_WEB_TABLES, false);
    OK(runner, find_table(document) == NULL,
       "a bare divider is a body row, not a valid table header");
    cmark_node_free(document);
  }
}

static void sparse_rows(test_batch_runner *runner) {
  const int columns = 1024, body_rows = 1024;
  size_t capacity = 2 * columns + 4 * columns + 2 * body_rows + 3;
  char *source = malloc(capacity), *out = source;
  for (int i = 0; i < columns; i++) { *out++ = '|'; *out++ = 'H'; }
  *out++ = '\n';
  for (int i = 0; i < columns; i++) {
    *out++ = '|'; *out++ = '-'; *out++ = '-'; *out++ = '-';
  }
  *out++ = '\n';
  for (int i = 0; i < body_rows; i++) { *out++ = '|'; *out++ = '\n'; }
  *out = 0;
  cmark_node *document = parse(source, CMARK_OPT_WEB_TABLES, false);
  cmark_node *table = find_table(document);
  INT_EQ(runner, child_count(table), body_rows + 1,
         "sparse Web rows do not hit the autocompleted-cell limit");
  int actual_cells = 0;
  for (cmark_node *row = cmark_node_first_child(table); row; row = cmark_node_next(row))
    actual_cells += child_count(row);
  INT_EQ(runner, actual_cells, columns + body_rows,
         "sparse table storage follows actual cells, not header width times rows");
  cmark_node_free(document);
  free(source);
}

int main(void) {
  test_batch_runner *runner = test_batch_runner_new();
  cmark_enable_safety_checks(true);
  cmark_gfm_core_extensions_ensure_registered();
  rows(runner);
  opt_in(runner);
  sparse_rows(runner);
  test_print_summary(runner);
  int result = test_ok(runner) ? 0 : 1;
  free(runner);
  return result;
}
