/* Definition resolution must preserve actual inline/EOL events independently
 * of the later remark-breaks rewrite, which creates positionless nodes. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmark-gfm.h"
#include "harness.h"
struct definition_break_event { int hard; const char *value; int sl,sc,el,ec; };
static const struct { const char *id,*input; const struct definition_break_event *events; size_t count; } cases[]={
#include "fixtures/definition_break_cases.inc"
};
static const char *ending(cmark_node *n){switch(cmark_node_get_softbreak_ending(n)){case 10:return "\n";case 13:return "\r";case 0x0d0a:return "\r\n";default:return "";}}
int main(void){
 test_batch_runner *runner=test_batch_runner_new();
 for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++)for(int bytes=0;bytes<2;bytes++){
  cmark_parser *p=cmark_parser_new(CMARK_OPT_DEFINITION_LISTS|CMARK_OPT_WEB_POSITIONS|CMARK_OPT_SOFTBREAK_METADATA|CMARK_OPT_LIST_SPREAD_METADATA);
  size_t size=strlen(cases[c].input);if(bytes){for(size_t i=0;i<size;i++)cmark_parser_feed(p,cases[c].input+i,1);}else cmark_parser_feed(p,cases[c].input,size);
  cmark_node *d=cmark_parser_finish(p);cmark_iter *it=cmark_iter_new(d);cmark_event_type event;size_t count=0;
  while((event=cmark_iter_next(it))!=CMARK_EVENT_DONE){
   cmark_node *n=cmark_iter_get_node(it);cmark_node_type type=cmark_node_get_type(n);
   if(event!=CMARK_EVENT_ENTER||(type!=CMARK_NODE_TEXT&&type!=CMARK_NODE_LINEBREAK&&type!=CMARK_NODE_SOFTBREAK))continue;
   cmark_node *prev=cmark_node_previous(n);if(type==CMARK_NODE_SOFTBREAK&&prev&&cmark_node_get_type(prev)==CMARK_NODE_TEXT)continue;
   if(count<cases[c].count){
    const struct definition_break_event *e=&cases[c].events[count];int sl=0,sc=0,el=0,ec=0;
    INT_EQ(runner,type==CMARK_NODE_LINEBREAK,e->hard,"definition hard/soft token %s/%zu",cases[c].id,count);
    INT_EQ(runner,cmark_node_get_web_position(n,&sl,&sc,&el,&ec),3,"definition actual token event %s",cases[c].id);
    char *value=calloc(size+8,1);if(type==CMARK_NODE_TEXT)strcpy(value,cmark_node_get_literal(n));else if(type==CMARK_NODE_SOFTBREAK)strcpy(value,ending(n));
    cmark_node *next=cmark_node_next(n);
    if(type==CMARK_NODE_TEXT&&next&&cmark_node_get_type(next)==CMARK_NODE_SOFTBREAK){
     int a=0,b=0;INT_EQ(runner,cmark_node_get_web_position(next,&a,&b,&el,&ec),3,"definition detached physical EOL event");
     INT_EQ(runner,strlen(ending(next))>0,1,"definition physical EOL kind");strcat(value,ending(next));
    }
    STR_EQ(runner,value,e->value,"definition raw inline value %s/%zu",cases[c].id,count);free(value);
    INT_EQ(runner,sl,e->sl,"definition inline start line %s",cases[c].id);INT_EQ(runner,sc,e->sc,"definition inline start column %s",cases[c].id);
    INT_EQ(runner,el,e->el,"definition inline end line %s",cases[c].id);INT_EQ(runner,ec,e->ec,"definition inline end column %s",cases[c].id);
   }count++;
  }
  INT_EQ(runner,count,cases[c].count,"definition inline count %s",cases[c].id);cmark_iter_free(it);cmark_node_free(d);cmark_parser_free(p);
 }
 test_print_summary(runner);return test_ok(runner)?0:1;
}
