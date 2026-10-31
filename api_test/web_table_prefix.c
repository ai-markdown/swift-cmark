#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "harness.h"
#include "node.h"
typedef struct {const char *kind, *value; int point[4];} expected_node;
typedef struct {const char *id, *input; const expected_node *nodes; size_t count;} prefix_case;
#include "fixtures/table_prefix_cases.inc"
static const char *selected(const char *kind) {
  if (!strcmp(kind,"table_header")) return "table_row";
  const char *types[]={"paragraph","table","table_row","table_cell","text","code","math_inline","emph","strong","link"};
  for(size_t i=0;i<sizeof(types)/sizeof(*types);i++) if(!strcmp(kind,types[i]))return types[i];
  return NULL;
}
int main(void) {
  test_batch_runner *runner=test_batch_runner_new();
  cmark_gfm_core_extensions_ensure_registered();cmark_enable_safety_checks(true);
  for(size_t i=0;i<sizeof(prefix_cases)/sizeof(*prefix_cases);i++)for(int chunked=0;chunked<2;chunked++) {
    const prefix_case *entry=&prefix_cases[i];size_t at=0;
    int options=CMARK_OPT_MATH|CMARK_OPT_DEFINITION_LISTS|CMARK_OPT_FOOTNOTES|CMARK_OPT_FOOTNOTES_LOSSLESS|CMARK_OPT_REFERENCE_USAGES|CMARK_OPT_WEB_CHARACTER_REFERENCES|CMARK_OPT_WEB_TABLES|CMARK_OPT_WEB_POSITIONS|CMARK_OPT_LIST_SPREAD_METADATA|CMARK_OPT_CODE_METADATA|CMARK_OPT_WEB_CODE_SYNTAX|CMARK_OPT_SOFTBREAK_METADATA|CMARK_OPT_WEB_EMPHASIS;
    cmark_parser *parser=cmark_parser_new(options);
    const char *extensions[]={"table","web_autolink","strikethrough","highlight","tasklist"};
    for(size_t k=0;k<sizeof(extensions)/sizeof(*extensions);k++)cmark_parser_attach_syntax_extension(parser,cmark_find_syntax_extension(extensions[k]));
    size_t size=strlen(entry->input);
    if(chunked)for(size_t b=0;b<size;b++)cmark_parser_feed(parser,entry->input+b,1);
    else cmark_parser_feed(parser,entry->input,size);
    cmark_node *document=cmark_parser_finish(parser);cmark_iter *it=cmark_iter_new(document);cmark_event_type event;
    while((event=cmark_iter_next(it))!=CMARK_EVENT_DONE) {
      if(event!=CMARK_EVENT_ENTER)continue;
      cmark_node *node=cmark_iter_get_node(it);const char *kind=selected(cmark_node_get_type_string(node));if(!kind)continue;
      OK(runner,at<entry->count,"%s node count",entry->id);if(at>=entry->count)break;
      const expected_node *expected=&entry->nodes[at++];
      STR_EQ(runner,kind,expected->kind,"%s node %zu kind",entry->id,at);
      if(expected->value)STR_EQ(runner,cmark_node_get_literal(node),expected->value,"%s node %zu value",entry->id,at);
      int point[4]={0};int flags=cmark_node_get_web_position(node,&point[0],&point[1],&point[2],&point[3]);
      INT_EQ(runner,flags,3,"%s node %zu flags",entry->id,at);
      for(int k=0;k<4;k++)INT_EQ(runner,point[k],expected->point[k],"%s node %zu coordinate %d",entry->id,at,k);
    }
    INT_EQ(runner,(int)at,(int)entry->count,"%s final count",entry->id);
    cmark_iter_free(it);
    /* Position tracking must not change raw source ranges or source catalogs. */
    cmark_parser *plain_parser=cmark_parser_new(options & ~CMARK_OPT_WEB_POSITIONS);
    for(size_t k=0;k<sizeof(extensions)/sizeof(*extensions);k++)cmark_parser_attach_syntax_extension(plain_parser,cmark_find_syntax_extension(extensions[k]));
    if(chunked)for(size_t b=0;b<size;b++)cmark_parser_feed(plain_parser,entry->input+b,1);
    else cmark_parser_feed(plain_parser,entry->input,size);
    cmark_node *plain=cmark_parser_finish(plain_parser);
    cmark_iter *left=cmark_iter_new(document), *right=cmark_iter_new(plain);
    cmark_event_type le, re;
    do {
      le=cmark_iter_next(left); re=cmark_iter_next(right);
      INT_EQ(runner,le,re,"%s tracking traversal",entry->id);
      if(le!=re || le==CMARK_EVENT_DONE)break;
      if(le!=CMARK_EVENT_ENTER)continue;
      cmark_node *a=cmark_iter_get_node(left), *b=cmark_iter_get_node(right);
      STR_EQ(runner,cmark_node_get_type_string(a),cmark_node_get_type_string(b),"%s tracking kind",entry->id);
      INT_EQ(runner,cmark_node_get_start_line(a),cmark_node_get_start_line(b),"%s tracking start line",entry->id);
      INT_EQ(runner,cmark_node_get_start_column(a),cmark_node_get_start_column(b),"%s tracking start column",entry->id);
      INT_EQ(runner,cmark_node_get_end_line(a),cmark_node_get_end_line(b),"%s tracking end line",entry->id);
      INT_EQ(runner,cmark_node_get_end_column(a),cmark_node_get_end_column(b),"%s tracking end column",entry->id);
    } while(le!=CMARK_EVENT_DONE);
    cmark_iter_free(left);cmark_iter_free(right);cmark_node_free(plain);cmark_parser_free(plain_parser);
    cmark_node_free(document);cmark_parser_free(parser);
  }
  test_print_summary(runner);int failed=!test_ok(runner);free(runner);return failed;
}
