/* Actual Web inline delimiter events; values and positions are frozen together.
 * Recording metadata must not change legacy source ranges or parsing. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "harness.h"
struct tilde_event { const char *kind, *value; int sl,sc,el,ec; };
static const struct { const char *id,*input; const struct tilde_event *events; size_t count; } cases[] = {
#include "fixtures/tilde_position_cases.inc"
};
static cmark_node *parse(const char *source,bool positions,bool bytes) {
  int flags=CMARK_OPT_WEB_EMPHASIS|CMARK_OPT_DEFINITION_LISTS|(positions?CMARK_OPT_WEB_POSITIONS:0);
  cmark_parser *p=cmark_parser_new(flags);
  cmark_parser_attach_syntax_extension(p,cmark_find_syntax_extension("strikethrough"));
  size_t size=strlen(source);
  if(bytes){for(size_t i=0;i<size;i++)cmark_parser_feed(p,source+i,1);}else cmark_parser_feed(p,source,size);
  cmark_node *doc=cmark_parser_finish(p);cmark_parser_free(p);return doc;
}
static bool multiline_text(cmark_node *node) {
  cmark_node *parent=cmark_node_parent(node);
  for(cmark_node *s=cmark_node_first_child(parent);s;s=cmark_node_next(s))
    if(cmark_node_get_type(s)==CMARK_NODE_SOFTBREAK)return true;
  return false;
}
int main(void) {
  test_batch_runner *runner=test_batch_runner_new();cmark_gfm_core_extensions_ensure_registered();
  for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++)for(int bytes=0;bytes<2;bytes++) {
    cmark_node *doc=parse(cases[c].input,true,bytes),*legacy=parse(cases[c].input,false,bytes);
    cmark_iter *it=cmark_iter_new(doc),*old=cmark_iter_new(legacy);cmark_event_type event;size_t count=0;
    while((event=cmark_iter_next(it))!=CMARK_EVENT_DONE) {
      INT_EQ(runner,cmark_iter_next(old),event,"tilde metadata preserves traversal");
      if(event!=CMARK_EVENT_ENTER)continue;
      cmark_node *n=cmark_iter_get_node(it),*before=cmark_iter_get_node(old);
      STR_EQ(runner,cmark_node_get_type_string(n),cmark_node_get_type_string(before),"tilde metadata preserves kind");
      INT_EQ(runner,cmark_node_get_start_line(n),cmark_node_get_start_line(before),"tilde metadata preserves native start line");
      INT_EQ(runner,cmark_node_get_start_column(n),cmark_node_get_start_column(before),"tilde metadata preserves native start column");
      INT_EQ(runner,cmark_node_get_end_line(n),cmark_node_get_end_line(before),"tilde metadata preserves native end line");
      INT_EQ(runner,cmark_node_get_end_column(n),cmark_node_get_end_column(before),"tilde metadata preserves native end column");
      int sl=0,sc=0,el=0,ec=0;
      INT_EQ(runner,cmark_node_get_web_position(before,&sl,&sc,&el,&ec),0,"tilde positions remain opt-in");
      const char *kind=cmark_node_get_type_string(n);bool text=cmark_node_get_type(n)==CMARK_NODE_TEXT;
      if(strcmp(kind,"strikethrough")&&(!text||multiline_text(n)))continue;
      if(count<cases[c].count) {
        const struct tilde_event *e=&cases[c].events[count];
        STR_EQ(runner,text?"text":"delete",e->kind,"Web tilde kind %s",cases[c].id);
        if(text)STR_EQ(runner,cmark_node_get_literal(n),e->value,"Web tilde value %s",cases[c].id);
        INT_EQ(runner,cmark_node_get_web_position(n,&sl,&sc,&el,&ec),3,"Web tilde event %s",cases[c].id);
        INT_EQ(runner,sl,e->sl,"Web tilde start line %s",cases[c].id);
        INT_EQ(runner,sc,e->sc,"Web tilde start column %s",cases[c].id);
        INT_EQ(runner,el,e->el,"Web tilde end line %s",cases[c].id);
        INT_EQ(runner,ec,e->ec,"Web tilde end column %s",cases[c].id);
      }
      count++;
    }
    INT_EQ(runner,count,cases[c].count,"Web tilde selected node count %s",cases[c].id);
    cmark_iter_free(it);cmark_iter_free(old);cmark_node_free(doc);cmark_node_free(legacy);
  }
  test_print_summary(runner);return test_ok(runner)?0:1;
}
