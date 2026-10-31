/* Actual document-container events: blank footnote continuations, indented
 * flow interruption and nested list markers. Paragraph positions are owned
 * by the Swift projection; every recorded container/code endpoint is strict. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "harness.h"
struct list_boundary_event { cmark_node_type type; int spread,sl,sc,el,ec; const char *value; };
static const struct { const char *id,*input; const struct list_boundary_event *events; size_t count; } cases[]={
#include "fixtures/list_boundary_cases.inc"
};
static bool selected(cmark_node_type type) {
 switch(type){case CMARK_NODE_LIST:case CMARK_NODE_ITEM:case CMARK_NODE_BLOCK_QUOTE:case CMARK_NODE_FOOTNOTE_DEFINITION:case CMARK_NODE_DEFINITION_LIST:case CMARK_NODE_DEFINITION_TERM:case CMARK_NODE_DEFINITION_DESCRIPTION:case CMARK_NODE_CODE_BLOCK:case CMARK_NODE_PARAGRAPH:return true;default:return false;}
}
static cmark_node *parse(const char *source,bool positions,bool bytes) {
 int flags=CMARK_OPT_WEB_CODE_SYNTAX|CMARK_OPT_DEFINITION_LISTS|CMARK_OPT_FOOTNOTES|CMARK_OPT_FOOTNOTES_LOSSLESS|CMARK_OPT_CODE_METADATA|CMARK_OPT_MATH|CMARK_OPT_LIST_SPREAD_METADATA|CMARK_OPT_WEB_TABLES|CMARK_OPT_WEB_EMPHASIS|CMARK_OPT_REFERENCE_USAGES|CMARK_OPT_WEB_CHARACTER_REFERENCES|(positions?CMARK_OPT_WEB_POSITIONS:0);
 cmark_parser *p=cmark_parser_new(flags);const char *extensions[]={"table","strikethrough","tasklist","web_autolink","highlight"};
 for(size_t i=0;i<sizeof(extensions)/sizeof(extensions[0]);i++)cmark_parser_attach_syntax_extension(p,cmark_find_syntax_extension(extensions[i]));
 size_t size=strlen(source);if(bytes){for(size_t i=0;i<size;i++)cmark_parser_feed(p,source+i,1);}else cmark_parser_feed(p,source,size);
 cmark_node *doc=cmark_parser_finish(p);cmark_parser_free(p);return doc;
}
int main(void) {
 test_batch_runner *runner=test_batch_runner_new();cmark_gfm_core_extensions_ensure_registered();
 for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++)for(int bytes=0;bytes<2;bytes++) {
  cmark_node *doc=parse(cases[c].input,true,bytes),*untracked=parse(cases[c].input,false,bytes);
  cmark_iter *it=cmark_iter_new(doc),*old=cmark_iter_new(untracked);cmark_event_type event;size_t count=0;
  while((event=cmark_iter_next(it))!=CMARK_EVENT_DONE) {
   INT_EQ(runner,cmark_iter_next(old),event,"list tracking preserves traversal %s",cases[c].id);
   if(event!=CMARK_EVENT_ENTER)continue;
   cmark_node *n=cmark_iter_get_node(it),*before=cmark_iter_get_node(old);cmark_node_type type=cmark_node_get_type(n);
   INT_EQ(runner,type,cmark_node_get_type(before),"list tracking preserves kind %s",cases[c].id);
   INT_EQ(runner,cmark_node_get_start_line(n),cmark_node_get_start_line(before),"list tracking preserves start line");
   INT_EQ(runner,cmark_node_get_start_column(n),cmark_node_get_start_column(before),"list tracking preserves start column");
   INT_EQ(runner,cmark_node_get_end_line(n),cmark_node_get_end_line(before),"list tracking preserves end line");
   INT_EQ(runner,cmark_node_get_end_column(n),cmark_node_get_end_column(before),"list tracking preserves end column");
   if(!selected(type))continue;
   if(count<cases[c].count) {
    const struct list_boundary_event *e=&cases[c].events[count];int sl=0,sc=0,el=0,ec=0;
    INT_EQ(runner,type,e->type,"Web list boundary kind %s/%zu",cases[c].id,count);
    if(type!=CMARK_NODE_PARAGRAPH) {
     INT_EQ(runner,cmark_node_get_web_position(n,&sl,&sc,&el,&ec),3,"Web list event %s/%zu",cases[c].id,count);
     INT_EQ(runner,sl,e->sl,"Web list start line %s",cases[c].id);INT_EQ(runner,sc,e->sc,"Web list start column %s",cases[c].id);
     INT_EQ(runner,el,e->el,"Web list end line %s",cases[c].id);INT_EQ(runner,ec,e->ec,"Web list end column %s",cases[c].id);
    }
    if(type==CMARK_NODE_LIST||type==CMARK_NODE_ITEM)INT_EQ(runner,cmark_node_get_list_spread(n),e->spread,"Web list spread %s",cases[c].id);
    if(type==CMARK_NODE_CODE_BLOCK)STR_EQ(runner,cmark_node_get_code_metadata_value(n),e->value,"Web code value %s",cases[c].id);
   }
   count++;
  }
  INT_EQ(runner,count,cases[c].count,"Web list selected count %s/%d",cases[c].id,bytes);
  cmark_iter_free(it);cmark_iter_free(old);cmark_node_free(doc);cmark_node_free(untracked);
 }
 test_print_summary(runner);return test_ok(runner)?0:1;
}
