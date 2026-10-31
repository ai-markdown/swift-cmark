/* Frozen raw link fields and token ranges from the actual default Web parser
 * ai-markdown/ai-markdown e9ec1799fdc380d99ce0a6313023515895bbd7bf.
 * Includes all 187 AutolinkTokenizer fixtures plus focused marker prefixes,
 * Unicode, containers, physical line endings, attention, and protected controls.
 * Full raw MDAST and HAST evidence is exported separately; these assertions do
 * not claim that the C HTML renderer implements the complete Web display chain. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CMARK_NO_SHORT_NAMES
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include "harness.h"

struct expected_link { const char *url, *text; int start_line, start_column, end_line, end_column; int text_line, text_column, text_end_line, text_end_column; };
struct web_case { const char *id, *source; int count; const struct expected_link *links; };
static const struct expected_link links_0[] = {
  {"http://example.com/path","http://example.com/path",1,7,1,30,1,7,1,30},
};
static const struct expected_link links_1[] = {
  {"https://example.com/a?b=c&d=e#f","https://example.com/a?b=c&d=e#f",1,1,1,32,1,1,1,32},
};
static const struct expected_link links_2[] = {
  {"http://www.example.com/path","www.example.com/path",1,1,1,21,1,1,1,21},
};
static const struct expected_link links_3[] = {
  {"mailto:me@example.com","me@example.com",1,1,1,15,1,1,1,15},
};
static const struct expected_link links_4[] = {
  {"mailto:first.last+tag@example.co.uk","first.last+tag@example.co.uk",1,1,1,29,1,1,1,29},
};
static const struct expected_link links_5[] = {
  {"HTTP://EXAMPLE.COM","HTTP://EXAMPLE.COM",1,1,1,19,1,1,1,19},
  {"http://WWW.EXAMPLE.COM","WWW.EXAMPLE.COM",1,20,1,35,1,20,1,35},
  {"mailto:ME@EXAMPLE.COM","ME@EXAMPLE.COM",1,36,1,50,1,36,1,50},
};
static const struct expected_link links_7[] = {
  {"https://example.com/a","https://example.com/a",1,1,1,22,1,1,1,22},
};
static const struct expected_link links_8[] = {
  {"https://example.com/a(b)c","https://example.com/a(b)c",1,2,1,27,1,2,1,27},
};
static const struct expected_link links_9[] = {
  {"https://example.com/a(b)","https://example.com/a(b)",1,1,1,25,1,1,1,25},
};
static const struct expected_link links_10[] = {
  {"http://www.example.com/a","www.example.com/a",1,1,1,18,1,1,1,18},
};
static const struct expected_link links_11[] = {
  {"mailto:me@example.com","me@example.com",1,2,1,16,1,2,1,16},
  {"mailto:me@example.com","me@example.com",1,19,1,33,1,19,1,33},
};
static const struct expected_link links_12[] = {
  {"https://example.com/a&amp;b=1","https://example.com/a&amp;b=1",1,1,1,30,1,1,1,30},
};
static const struct expected_link links_13[] = {
  {"https://example.com/a","https://example.com/a",1,1,1,22,1,1,1,22},
};
static const struct expected_link links_14[] = {
  {"http://example.com","http://example.com",0,0,0,0,0,0,0,0},
  {"http://www.example.com","www.example.com",0,0,0,0,0,0,0,0},
  {"mailto:me@example.com","me@example.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_15[] = {
  {"https://example.com/a\\*b","https://example.com/a\\*b",1,1,1,25,1,1,1,25},
};
static const struct expected_link links_16[] = {
  {"https://example.com/\344\270\255\346\226\207/\360\237\231\202","https://example.com/\344\270\255\346\226\207/\360\237\231\202",1,1,1,32,1,1,1,32},
};
static const struct expected_link links_17[] = {
  {"https://\344\276\213\345\255\220.\346\265\213\350\257\225/a","https://\344\276\213\345\255\220.\346\265\213\350\257\225/a",1,1,1,24,1,1,1,24},
};
static const struct expected_link links_18[] = {
  {"https://example.com/a\344\270\255\346\226\207","https://example.com/a\344\270\255\346\226\207",1,7,1,34,1,7,1,34},
};
static const struct expected_link links_20[] = {
  {"mailto:me@example.com","me@example.com",1,7,1,21,1,7,1,21},
};
static const struct expected_link links_21[] = {
  {"https://elsewhere.com","https://example.com",1,1,1,45,1,2,1,21},
};
static const struct expected_link links_26[] = {
  {"http://www.example.com","www.example.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_27[] = {
  {"https://example.com","https://example.com",3,7,3,26,3,7,3,26},
  {"mailto:me@example.com","me@example.com",3,27,3,41,3,27,3,41},
};
static const struct expected_link links_28[] = {
  {"http://www.example.com","www.example.com",3,3,3,18,3,3,3,18},
};
static const struct expected_link links_29[] = {
  {"https://example.com","https://example.com",1,2,1,21,1,2,1,21},
  {"mailto:me@example.com","me@example.com",1,25,1,39,1,25,1,39},
};
static const struct expected_link links_31[] = {
  {"http://localhost","http://localhost",1,43,1,59,1,43,1,59},
};
static const struct expected_link links_32[] = {
  {"http://www","www",1,1,1,4,1,1,1,4},
  {"http://www.x","www.x",1,6,1,11,1,6,1,11},
  {"http://www..example.com","www..example.com",1,12,1,28,1,12,1,28},
};
static const struct expected_link links_33[] = {
  {"mailto:a@b.c","a@b.c",1,5,1,10,1,5,1,10},
};
static const struct expected_link links_34[] = {
  {"https://example.com","https://example.com",1,1,1,20,1,1,1,20},
  {"http://www.example.com","www.example.com",2,1,2,16,2,1,2,16},
};
static const struct expected_link links_35[] = {
  {"https://example.com","https://example.com",1,1,1,20,1,1,1,20},
  {"mailto:me@example.com","me@example.com",1,21,1,35,1,21,1,35},
};
static const struct expected_link links_36[] = {
  {"https://example.com/a","https://example.com/a",1,1,1,24,1,2,1,23},
  {"mailto:me@example.com","me@example.com",1,25,1,41,1,26,1,40},
};
static const struct expected_link links_37[] = {
  {"http://www.example.com/#a","www.example.com/#a",1,1,1,19,1,1,1,19},
};
static const struct expected_link links_38[] = {
  {"https://foo_bar.example.com/a_b","https://foo_bar.example.com/a_b",1,1,1,32,1,1,1,32},
};
static const struct expected_link links_39[] = {
  {"mailto:xme@example.com","xme@example.com",1,38,1,53,1,38,1,53},
};
static const struct expected_link links_40[] = {
  {"https://example.com/a","https://example.com/a",1,1,1,22,1,1,1,22},
  {"https://example.com/a}","https://example.com/a}",1,24,1,46,1,24,1,46},
};
static const struct expected_link links_41[] = {
  {"http://www.example.com","www.example.com",1,1,1,16,1,1,1,16},
};
static const struct expected_link links_42[] = {
  {"https://example.com","https://example.com",1,1,1,20,1,1,1,20},
};
static const struct expected_link links_44[] = {
  {"HtTp://example.com","HtTp://example.com",1,1,1,19,1,1,1,19},
  {"hTtPs://example.com/a","hTtPs://example.com/a",1,20,1,41,1,20,1,41},
  {"http://WwW.example.com","WwW.example.com",1,42,1,57,1,42,1,57},
};
static const struct expected_link links_46[] = {
  {"http://www","www",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_47[] = {
  {"http://-a.example","http://-a.example",0,0,0,0,0,0,0,0},
  {"http://a-.example","http://a-.example",1,19,1,36,1,19,1,36},
  {"http://a--b.example","http://a--b.example",1,37,1,56,1,37,1,56},
};
static const struct expected_link links_49[] = {
  {"http://a..bc","http://a..bc",1,1,1,13,1,1,1,13},
  {"http://.a.bc","http://.a.bc",0,0,0,0,0,0,0,0},
  {"http://a.bc","http://a.bc",1,27,1,38,1,27,1,38},
  {"http://www..a.bc","www..a.bc",1,40,1,49,1,40,1,49},
};
static const struct expected_link links_50[] = {
  {"https://example.com:443/a","https://example.com:443/a",1,1,1,26,1,1,1,26},
  {"https://example.com:abc/a","https://example.com:abc/a",1,27,1,52,1,27,1,52},
};
static const struct expected_link links_51[] = {
  {"https://user:pass@example.com/a","https://user:pass@example.com/a",1,1,1,32,1,1,1,32},
  {"http://user@example.com","http://user@example.com",1,33,1,56,1,33,1,56},
};
static const struct expected_link links_52[] = {
  {"http://127.0.0.1/a","http://127.0.0.1/a",1,1,1,19,1,1,1,19},
};
static const struct expected_link links_53[] = {
  {"http://\344\276\213\345\255\220.com/a","http://\344\276\213\345\255\220.com/a",1,1,1,20,1,1,1,20},
  {"http://www.\344\276\213\345\255\220.com/a","www.\344\276\213\345\255\220.com/a",1,21,1,37,1,21,1,37},
  {"http://a.\346\265\213\350\257\225/a","http://a.\346\265\213\350\257\225/a",1,38,1,55,1,38,1,55},
};
static const struct expected_link links_54[] = {
  {"http://\360\237\246\212.com/a","http://\360\237\246\212.com/a",1,1,1,18,1,1,1,18},
  {"http://www.\360\237\246\212.com/a","www.\360\237\246\212.com/a",1,19,1,33,1,19,1,33},
};
static const struct expected_link links_55[] = {
  {"http://e\314\201.com/a","http://e\314\201.com/a",1,1,1,17,1,1,1,17},
  {"http://www.e\314\201.com/a","www.e\314\201.com/a",1,18,1,31,1,18,1,31},
};
static const struct expected_link links_56[] = {
  {"http://\304\241.com/a","http://\304\241.com/a",1,1,1,16,1,1,1,16},
  {"http://\304\200.com/a","http://\304\200.com/a",1,17,1,32,1,17,1,32},
  {"http://www.\304\241.com/a","www.\304\241.com/a",1,33,1,45,1,33,1,45},
};
static const struct expected_link links_57[] = {
  {"http://a.bc/a\304\241","http://a.bc/a\304\241",1,1,1,16,1,1,1,16},
  {"http://a.bc/a\304\200","http://a.bc/a\304\200",1,17,1,32,1,17,1,32},
  {"http://a.bc/a\304\256","http://a.bc/a\304\256",1,33,1,48,1,33,1,48},
  {"http://a.bc/a\342\200\241","http://a.bc/a\342\200\241",1,49,1,65,1,49,1,65},
  {"http://a.bc/a\342\204\241","http://a.bc/a\342\204\241",1,66,1,82,1,66,1,82},
  {"http://a.bc/a\342\200\277","http://a.bc/a\342\200\277",1,83,1,99,1,83,1,99},
};
static const struct expected_link links_58[] = {
  {"http://123.456/a","http://123.456/a",1,1,1,17,1,1,1,17},
  {"http://www.123.456/a","www.123.456/a",1,18,1,31,1,18,1,31},
};
static const struct expected_link links_59[] = {
  {"http://a.bc/a[]{}","http://a.bc/a[]{}",1,1,1,18,1,1,1,18},
  {"http://a.bc/a\"b","http://a.bc/a\"b",1,21,1,36,1,21,1,36},
  {"http://a.bc/a'b","http://a.bc/a'b",1,37,1,52,1,37,1,52},
};
static const struct expected_link links_60[] = {
  {"http://a.bc/(a(b)c)d","http://a.bc/(a(b)c)d",1,1,1,21,1,1,1,21},
  {"http://a.bc/a((b)","http://a.bc/a((b)",1,23,1,40,1,23,1,40},
  {"http://a.bc/a","http://a.bc/a",1,41,1,54,1,41,1,54},
};
static const struct expected_link links_61[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://a.bc/a","http://a.bc/a",1,24,1,37,1,24,1,37},
  {"http://a.bc/a","http://a.bc/a",1,42,1,55,1,42,1,55},
};
static const struct expected_link links_62[] = {
  {"http://a.bc/a!b?c:d;e*f_g~h.i,j","http://a.bc/a!b?c:d;e*f_g~h.i,j",1,1,1,32,1,1,1,32},
};
static const struct expected_link links_63[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://a.bc/a&#123","http://a.bc/a&#123",1,21,1,39,1,21,1,39},
  {"http://a.bc/a&#x7b","http://a.bc/a&#x7b",1,41,1,59,1,41,1,59},
};
static const struct expected_link links_64[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://a.bc/a&#","http://a.bc/a&#",1,22,1,37,1,22,1,37},
  {"http://a.bc/a&#x","http://a.bc/a&#x",1,39,1,55,1,39,1,55},
};
static const struct expected_link links_65[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://a.bc/a","http://a.bc/a",1,21,1,34,1,21,1,34},
  {"http://a.bc/a&copy;x","http://a.bc/a&copy;x",1,41,1,61,1,41,1,61},
};
static const struct expected_link links_66[] = {
  {"http://a.bc/a\\!b","http://a.bc/a\\!b",1,1,1,17,1,1,1,17},
  {"http://a.bc/a$$b$$","http://a.bc/a$$b$$",1,18,1,36,1,18,1,36},
  {"http://a.bc/a\\","http://a.bc/a\\",1,37,1,51,1,37,1,51},
};
static const struct expected_link links_67[] = {
  {"https://a.bc/a\\==b==","https://a.bc/a\\==b==",1,1,1,21,1,1,1,21},
};
static const struct expected_link links_68[] = {
  {"http://www.a.bc","www.a.bc",1,2,1,10,1,2,1,10},
  {"http://www.a.bc","www.a.bc",0,0,0,0,0,0,0,0},
  {"http://www.a.bc","www.a.bc",0,0,0,0,0,0,0,0},
  {"http://www.a.bc","www.a.bc",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_69[] = {
  {"http://www.a.bc","www.a.bc",1,12,1,20,1,12,1,20},
};
static const struct expected_link links_70[] = {
  {"https://a.bc","https://a.bc",1,16,1,28,1,16,1,28},
  {"https://a.bc","https://a.bc",1,32,1,44,1,32,1,44},
  {"https://a.bc","https://a.bc",1,49,1,61,1,49,1,61},
};
static const struct expected_link links_71[] = {
  {"mailto:a.b+c-d_e@example.com","a.b+c-d_e@example.com",1,1,1,22,1,1,1,22},
  {"mailto:b@example.com","b@example.com",1,25,1,38,1,25,1,38},
  {"mailto:b@example.com","b@example.com",1,41,1,54,1,41,1,54},
};
static const struct expected_link links_72[] = {
  {"mailto:.me@example.com",".me@example.com",1,1,1,16,1,1,1,16},
  {"mailto:-me@example.com","-me@example.com",1,17,1,32,1,17,1,32},
  {"mailto:_me@example.com","_me@example.com",0,0,0,0,0,0,0,0},
  {"mailto:__me@example.com","__me@example.com",0,0,0,0,0,0,0,0},
  {"mailto:+me@example.com","+me@example.com",1,66,1,81,1,66,1,81},
};
static const struct expected_link links_73[] = {
  {"mailto:me@example.com","me@example.com",0,0,0,0,0,0,0,0},
  {"mailto:me@example.com","me@example.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@example.com",".me@example.com",1,37,1,52,1,37,1,52},
  {"mailto:-me@example.com","-me@example.com",1,56,1,71,1,56,1,71},
};
static const struct expected_link links_74[] = {
  {"mailto:.a@example.com",".a@example.com",1,1,1,15,1,1,1,15},
  {"mailto:a..b@example.com","a..b@example.com",1,16,1,32,1,16,1,32},
  {"mailto:a.@example.com","a.@example.com",1,33,1,47,1,33,1,47},
};
static const struct expected_link links_75[] = {
  {"mailto:a@-example.com","a@-example.com",1,1,1,15,1,1,1,15},
  {"mailto:a@example-.com","a@example-.com",1,16,1,30,1,16,1,30},
  {"mailto:a@ex--ample.com","a@ex--ample.com",1,31,1,46,1,31,1,46},
};
static const struct expected_link links_76[] = {
  {"mailto:a@foo_bar.com","a@foo_bar.com",1,1,1,14,1,1,1,14},
  {"mailto:a@example.c_m","a@example.c_m",1,15,1,28,1,15,1,28},
};
static const struct expected_link links_77[] = {
  {"mailto:a@example.com","a@example.com",1,1,1,14,1,1,1,14},
  {"mailto:a@example.com","a@example.com",1,16,1,29,1,16,1,29},
  {"mailto:a@example.com","a@example.com",1,31,1,44,1,31,1,44},
  {"mailto:a@example.com","a@example.com",1,46,1,59,1,46,1,59},
};
static const struct expected_link links_78[] = {
  {"mailto:a@example.com","a@example.com",1,4,1,17,1,4,1,17},
  {"mailto:a@example.com","a@example.com",1,52,1,65,1,52,1,65},
};
static const struct expected_link links_79[] = {
  {"mailto:b@example.com","b@example.com",1,25,1,38,1,25,1,38},
};
static const struct expected_link links_80[] = {
  {"mailto:me@example.com","me@example.com",1,8,1,22,1,8,1,22},
  {"mailto:ME@EXAMPLE.COM","ME@EXAMPLE.COM",1,30,1,44,1,30,1,44},
};
static const struct expected_link links_81[] = {
  {"https://a.b/c==d==","https://a.b/c==d==",1,5,1,23,1,5,1,23},
};
static const struct expected_link links_82[] = {
  {"https://a.b/c==","https://a.b/c==",1,3,1,18,1,3,1,18},
};
static const struct expected_link links_83[] = {
  {"http://www.a.b/==x==","www.a.b/==x==",1,1,1,14,1,1,1,14},
};
static const struct expected_link links_84[] = {
  {"https://a.bc/?x==y==&a=b","https://a.bc/?x==y==&a=b",1,1,1,25,1,1,1,25},
};
static const struct expected_link links_85[] = {
  {"mailto:me@example.com","me@example.com",1,3,1,17,1,3,1,17},
};
static const struct expected_link links_86[] = {
  {"https://a.bc/a","https://a.bc/a",1,6,1,20,1,6,1,20},
  {"https://a.bc/a==x==","https://a.bc/a==x==",1,21,1,40,1,21,1,40},
};
static const struct expected_link links_87[] = {
  {"http://a.bc/a","http://a.bc/a",1,2,1,15,1,2,1,15},
  {"http://www.a.bc","www.a.bc",1,19,1,27,1,19,1,27},
  {"mailto:me@example.com","me@example.com",1,32,1,46,1,32,1,46},
};
static const struct expected_link links_88[] = {
  {"https://elsewhere.com","http://a.bc/a",1,1,1,41,0,0,0,0},
};
static const struct expected_link links_91[] = {
  {"https://elsewhere.com","",1,1,1,48,0,0,0,0},
};
static const struct expected_link links_92[] = {
  {"http://a.bc/a","http://a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_93[] = {
  {"http://www.a.bc/a","www.a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_94[] = {
  {"http://a.bc/a","http://a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_95[] = {
  {"http://a.bc/a","http://a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_96[] = {
  {"http://www.a.bc/a","www.a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_97[] = {
  {"https://elsewhere.com","a [http://a.bc/ax] b",1,1,1,50,0,0,0,0},
};
static const struct expected_link links_98[] = {
  {"https://a.bc/==x==","https://a.bc/==x==",1,1,1,21,1,2,1,20},
  {"mailto:me@example.com","me@example.com",1,22,1,38,1,23,1,37},
};
static const struct expected_link links_99[] = {
  {"http://a.bc/a==x==","http://a.bc/a==x==",1,7,1,25,1,7,1,25},
  {"http://www.a.bc","www.a.bc",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_100[] = {
  {"http://www.a.bc","www.a.bc",5,1,5,9,5,1,5,9},
};
static const struct expected_link links_101[] = {
  {"http://www.a.bc","www.a.bc",1,22,1,30,1,22,1,30},
};
static const struct expected_link links_102[] = {
  {"http://www.a.bc","www.a.bc",5,1,5,9,5,1,5,9},
};
static const struct expected_link links_103[] = {
  {"http://a.bc/a==x==","http://a.bc/a==x==",3,7,3,25,3,7,3,25},
};
static const struct expected_link links_104[] = {
  {"http://a.bc/a==x==","http://a.bc/a==x==",3,3,3,21,3,3,3,21},
  {"mailto:me@example.com","me@example.com",3,24,3,38,3,24,3,38},
};
static const struct expected_link links_105[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://www.a.bc/a","www.a.bc/a",2,1,2,11,2,1,2,11},
  {"mailto:me@example.com","me@example.com",3,1,3,15,3,1,3,15},
};
static const struct expected_link links_106[] = {
  {"http://a.bc/a","http://a.bc/a",1,1,1,14,1,1,1,14},
  {"http://www.a.bc","www.a.bc",1,15,1,23,1,15,1,23},
  {"mailto:me@example.com","me@example.com",1,24,1,38,1,24,1,38},
};
static const struct expected_link links_107[] = {
  {"https://a.bc/a","https://a.bc/a",1,3,1,17,1,3,1,17},
  {"http://www.a.bc/a","www.a.bc/a",1,20,1,30,1,20,1,30},
  {"mailto:me@example.com","me@example.com",1,33,1,47,1,33,1,47},
};
static const struct expected_link links_108[] = {
  {"https://a.bc/a","https://a.bc/a",1,4,1,18,1,4,1,18},
  {"http://www.a.bc/a","www.a.bc/a",1,22,1,32,1,22,1,32},
  {"mailto:me@example.com","me@example.com",1,36,1,50,1,36,1,50},
};
static const struct expected_link links_109[] = {
  {"https://a.bc/a","https://a.bc/a",1,4,1,18,1,4,1,18},
  {"http://www.a.bc/a","www.a.bc/a",1,22,1,32,1,22,1,32},
  {"mailto:me@example.com","me@example.com",1,36,1,50,1,36,1,50},
};
static const struct expected_link links_110[] = {
  {"https://a.bc/a","https://a.bc/a",1,1,1,15,1,1,1,15},
  {"http://www.a.bc/a","www.a.bc/a",1,19,1,29,1,19,1,29},
  {"mailto:me@example.com","me@example.com",1,33,1,47,1,33,1,47},
};
static const struct expected_link links_111[] = {
  {"https://a.bc/a\342\200\213","https://a.bc/a\342\200\213",1,4,1,21,1,4,1,21},
  {"http://www.a.bc/a\342\200\213","www.a.bc/a\342\200\213",1,22,1,35,1,22,1,35},
  {"mailto:me@example.com","me@example.com",1,36,1,50,1,36,1,50},
};
static const struct expected_link links_112[] = {
  {"https://a.bc/a\314\201","https://a.bc/a\314\201",1,3,1,19,1,3,1,19},
  {"http://www.a.bc/a\314\201","www.a.bc/a\314\201",1,20,1,32,1,20,1,32},
  {"mailto:me@example.com","me@example.com",1,33,1,47,1,33,1,47},
};
static const struct expected_link links_113[] = {
  {"https://a.bc/a\360\237\246\212","https://a.bc/a\360\237\246\212",1,5,1,23,1,5,1,23},
  {"http://www.a.bc/a\360\237\246\212","www.a.bc/a\360\237\246\212",1,24,1,38,1,24,1,38},
  {"mailto:me@example.com","me@example.com",1,39,1,53,1,39,1,53},
};
static const struct expected_link links_114[] = {
  {"https://a.bc/a\360\220\204\200","https://a.bc/a\360\220\204\200",1,5,1,23,1,5,1,23},
  {"http://www.a.bc/a\360\220\204\200","www.a.bc/a\360\220\204\200",1,24,1,38,1,24,1,38},
  {"mailto:me@example.com","me@example.com",1,39,1,53,1,39,1,53},
};
static const struct expected_link links_127[] = {
  {"https://\344\276\213","https://\344\276\213",1,9,1,20,1,9,1,20},
};
static const struct expected_link links_128[] = {
  {"https://\344\276\213\345\255\220","https://\344\276\213\345\255\220",1,9,1,23,1,9,1,23},
};
static const struct expected_link links_129[] = {
  {"https://\344\276\213\345\255\220","https://\344\276\213\345\255\220",1,9,1,23,1,9,1,23},
};
static const struct expected_link links_130[] = {
  {"https://\344\276\213\345\255\220.c","https://\344\276\213\345\255\220.c",1,9,1,25,1,9,1,25},
};
static const struct expected_link links_131[] = {
  {"https://\344\276\213\345\255\220.co","https://\344\276\213\345\255\220.co",1,9,1,26,1,9,1,26},
};
static const struct expected_link links_132[] = {
  {"https://\344\276\213\345\255\220.com","https://\344\276\213\345\255\220.com",1,9,1,27,1,9,1,27},
};
static const struct expected_link links_133[] = {
  {"https://\344\276\213\345\255\220.com/","https://\344\276\213\345\255\220.com/",1,9,1,28,1,9,1,28},
};
static const struct expected_link links_134[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207","https://\344\276\213\345\255\220.com/\346\226\207",1,9,1,31,1,9,1,31},
};
static const struct expected_link links_135[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202",1,9,1,35,1,9,1,35},
};
static const struct expected_link links_136[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202=","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202=",1,9,1,36,1,9,1,36},
};
static const struct expected_link links_137[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==",1,9,1,37,1,9,1,37},
};
static const struct expected_link links_138[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x",1,9,1,38,1,9,1,38},
};
static const struct expected_link links_139[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x=","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x=",1,9,1,39,1,9,1,39},
};
static const struct expected_link links_140[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_141[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_142[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_143[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_144[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_145[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_146[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_147[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_148[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_149[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_150[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_151[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_152[] = {
  {"https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==","https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,9,1,40,1,9,1,40},
};
static const struct expected_link links_168[] = {
  {"mailto:me@example.c","me@example.c",1,5,1,17,1,5,1,17},
};
static const struct expected_link links_169[] = {
  {"mailto:me@example.co","me@example.co",1,5,1,18,1,5,1,18},
};
static const struct expected_link links_170[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
};
static const struct expected_link links_171[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
};
static const struct expected_link links_172[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
};
static const struct expected_link links_173[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
};
static const struct expected_link links_174[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
};
static const struct expected_link links_175[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www","www",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_176[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a","www.a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_177[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a","www.a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_178[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.b","www.a.b",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_179[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc","www.a.bc",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_180[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/","www.a.bc/",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_181[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a","www.a.bc/a",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_182[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a(","www.a.bc/a(",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_183[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a(b","www.a.bc/a(b",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_184[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a(b)","www.a.bc/a(b)",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_185[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a(b)","www.a.bc/a(b)",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_186[] = {
  {"mailto:me@example.com","me@example.com",1,5,1,19,1,5,1,19},
  {"http://www.a.bc/a(b)","www.a.bc/a(b)",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_187[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,11,1,20,1,11,1,20},
  {"mailto:-me@x.com","-me@x.com",1,21,1,30,1,21,1,30},
  {"mailto:+me@x.com","+me@x.com",1,31,1,40,1,31,1,40},
};
static const struct expected_link links_188[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_189[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_190[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,15,1,24,1,15,1,24},
  {"mailto:-me@x.com","-me@x.com",1,25,1,34,1,25,1,34},
  {"mailto:+me@x.com","+me@x.com",1,35,1,44,1,35,1,44},
};
static const struct expected_link links_191[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",2,13,2,22,2,13,2,22},
  {"mailto:-me@x.com","-me@x.com",2,23,2,32,2,23,2,32},
  {"mailto:+me@x.com","+me@x.com",2,33,2,42,2,33,2,42},
};
static const struct expected_link links_192[] = {
  {"mailto:.me@x.com",".me@x.com",1,2,1,11,1,2,1,11},
  {"mailto:-me@x.com","-me@x.com",1,12,1,21,1,12,1,21},
};
static const struct expected_link links_193[] = {
  {"mailto:.me@x.com",".me@x.com",1,4,1,13,1,4,1,13},
  {"mailto:-me@x.com","-me@x.com",1,14,1,23,1,14,1,23},
};
static const struct expected_link links_194[] = {
  {"mailto:.me@x.com",".me@x.com",1,4,1,13,1,4,1,13},
  {"mailto:-me@x.com","-me@x.com",1,14,1,23,1,14,1,23},
};
static const struct expected_link links_195[] = {
  {"mailto:.me@x.com",".me@x.com",1,6,1,15,1,6,1,15},
  {"mailto:-me@x.com","-me@x.com",1,16,1,25,1,16,1,25},
};
static const struct expected_link links_196[] = {
  {"mailto:.me@x.com",".me@x.com",2,4,2,13,2,4,2,13},
  {"mailto:-me@x.com","-me@x.com",2,14,2,23,2,14,2,23},
};
static const struct expected_link links_197[] = {
  {"mailto:..me@x.com","..me@x.com",1,2,1,12,1,2,1,12},
  {"mailto:--me@x.com","--me@x.com",1,13,1,23,1,13,1,23},
};
static const struct expected_link links_198[] = {
  {"mailto:..me@x.com","..me@x.com",1,4,1,14,1,4,1,14},
  {"mailto:--me@x.com","--me@x.com",1,15,1,25,1,15,1,25},
};
static const struct expected_link links_199[] = {
  {"mailto:..me@x.com","..me@x.com",1,4,1,14,1,4,1,14},
  {"mailto:--me@x.com","--me@x.com",1,15,1,25,1,15,1,25},
};
static const struct expected_link links_200[] = {
  {"mailto:..me@x.com","..me@x.com",1,6,1,16,1,6,1,16},
  {"mailto:--me@x.com","--me@x.com",1,17,1,27,1,17,1,27},
};
static const struct expected_link links_201[] = {
  {"mailto:..me@x.com","..me@x.com",2,4,2,14,2,4,2,14},
  {"mailto:--me@x.com","--me@x.com",2,15,2,25,2,15,2,25},
};
static const struct expected_link links_202[] = {
  {"mailto:.me@x.com",".me@x.com",1,5,1,14,1,5,1,14},
  {"mailto:-me@x.com","-me@x.com",1,20,1,29,1,20,1,29},
};
static const struct expected_link links_203[] = {
  {"mailto:.me@x.com",".me@x.com",1,7,1,16,1,7,1,16},
  {"mailto:-me@x.com","-me@x.com",1,22,1,31,1,22,1,31},
};
static const struct expected_link links_204[] = {
  {"mailto:.me@x.com",".me@x.com",1,7,1,16,1,7,1,16},
  {"mailto:-me@x.com","-me@x.com",1,22,1,31,1,22,1,31},
};
static const struct expected_link links_205[] = {
  {"mailto:.me@x.com",".me@x.com",1,9,1,18,1,9,1,18},
  {"mailto:-me@x.com","-me@x.com",1,24,1,33,1,24,1,33},
};
static const struct expected_link links_206[] = {
  {"mailto:.me@x.com",".me@x.com",2,7,2,16,2,7,2,16},
  {"mailto:-me@x.com","-me@x.com",2,22,2,31,2,22,2,31},
};
static const struct expected_link links_207[] = {
  {"mailto:.me@x.com",".me@x.com",1,3,1,12,1,3,1,12},
  {"mailto:-me@x.com","-me@x.com",1,17,1,26,1,17,1,26},
};
static const struct expected_link links_208[] = {
  {"mailto:.me@x.com",".me@x.com",1,5,1,14,1,5,1,14},
  {"mailto:-me@x.com","-me@x.com",1,19,1,28,1,19,1,28},
};
static const struct expected_link links_209[] = {
  {"mailto:.me@x.com",".me@x.com",1,5,1,14,1,5,1,14},
  {"mailto:-me@x.com","-me@x.com",1,19,1,28,1,19,1,28},
};
static const struct expected_link links_210[] = {
  {"mailto:.me@x.com",".me@x.com",1,7,1,16,1,7,1,16},
  {"mailto:-me@x.com","-me@x.com",1,21,1,30,1,21,1,30},
};
static const struct expected_link links_211[] = {
  {"mailto:.me@x.com",".me@x.com",2,5,2,14,2,5,2,14},
  {"mailto:-me@x.com","-me@x.com",2,19,2,28,2,19,2,28},
};
static const struct expected_link links_212[] = {
  {"https://example.com",".me@x.com",1,1,1,33,1,2,1,11},
};
static const struct expected_link links_213[] = {
  {"https://example.com",".me@x.com",1,3,1,35,1,4,1,13},
};
static const struct expected_link links_214[] = {
  {"https://example.com",".me@x.com",1,3,1,35,1,4,1,13},
};
static const struct expected_link links_215[] = {
  {"https://example.com",".me@x.com",1,5,1,37,1,6,1,15},
};
static const struct expected_link links_216[] = {
  {"https://example.com",".me@x.com",2,3,2,35,2,4,2,13},
};
static const struct expected_link links_217[] = {
  {"mailto:.me@x.com",".me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:-me@x.com","-me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_218[] = {
  {"mailto:.me@x.com",".me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:-me@x.com","-me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_219[] = {
  {"mailto:.me@x.com",".me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:-me@x.com","-me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_220[] = {
  {"mailto:.me@x.com",".me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:-me@x.com","-me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_221[] = {
  {"mailto:.me@x.com",".me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:-me@x.com","-me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_222[] = {
  {"mailto:.me@x.com",".me@x.com",1,29,1,38,1,29,1,38},
};
static const struct expected_link links_223[] = {
  {"mailto:.me@x.com",".me@x.com",1,31,1,40,1,31,1,40},
};
static const struct expected_link links_224[] = {
  {"mailto:.me@x.com",".me@x.com",1,31,1,40,1,31,1,40},
};
static const struct expected_link links_225[] = {
  {"mailto:.me@x.com",".me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_226[] = {
  {"mailto:.me@x.com",".me@x.com",2,31,2,40,2,31,2,40},
};
static const struct expected_link links_227[] = {
  {"mailto:-me@.com","-me@.com",1,8,1,16,1,8,1,16},
  {"mailto:me@x.com","me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_228[] = {
  {"mailto:-me@.com","-me@.com",1,10,1,18,1,10,1,18},
  {"mailto:me@x.com","me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_229[] = {
  {"mailto:-me@.com","-me@.com",1,10,1,18,1,10,1,18},
  {"mailto:me@x.com","me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_230[] = {
  {"mailto:-me@.com","-me@.com",1,12,1,20,1,12,1,20},
  {"mailto:me@x.com","me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_231[] = {
  {"mailto:-me@.com","-me@.com",2,10,2,18,2,10,2,18},
  {"mailto:me@x.com","me@x.com",0,0,0,0,0,0,0,0},
};
static const struct expected_link links_240[] = {
  {"mailto:.me@x.c",".me@x.c",1,2,1,9,1,2,1,9},
};
static const struct expected_link links_241[] = {
  {"mailto:.me@x.co",".me@x.co",1,2,1,10,1,2,1,10},
};
static const struct expected_link links_242[] = {
  {"mailto:.me@x.com",".me@x.com",1,2,1,11,1,2,1,11},
};
static const struct expected_link links_251[] = {
  {"mailto:-me@x.c","-me@x.c",1,2,1,9,1,2,1,9},
};
static const struct expected_link links_252[] = {
  {"mailto:-me@x.co","-me@x.co",1,2,1,10,1,2,1,10},
};
static const struct expected_link links_253[] = {
  {"mailto:-me@x.com","-me@x.com",1,2,1,11,1,2,1,11},
};
static const struct expected_link links_268[] = {
  {"mailto:.me@x.c",".me@x.c",1,8,1,15,1,8,1,15},
};
static const struct expected_link links_269[] = {
  {"mailto:.me@x.co",".me@x.co",1,8,1,16,1,8,1,16},
};
static const struct expected_link links_270[] = {
  {"mailto:.me@x.com",".me@x.com",1,8,1,17,1,8,1,17},
};
static const struct expected_link links_271[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_272[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_273[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",2,13,2,22,2,13,2,22},
  {"mailto:-me@x.com","-me@x.com",2,23,2,32,2,23,2,32},
  {"mailto:+me@x.com","+me@x.com",2,33,2,42,2,33,2,42},
};
static const struct expected_link links_274[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_275[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",1,13,1,22,1,13,1,22},
  {"mailto:-me@x.com","-me@x.com",1,23,1,32,1,23,1,32},
  {"mailto:+me@x.com","+me@x.com",1,33,1,42,1,33,1,42},
};
static const struct expected_link links_276[] = {
  {"mailto:_me@x.com","_me@x.com",0,0,0,0,0,0,0,0},
  {"mailto:.me@x.com",".me@x.com",2,13,2,22,2,13,2,22},
  {"mailto:-me@x.com","-me@x.com",2,23,2,32,2,23,2,32},
  {"mailto:+me@x.com","+me@x.com",2,33,2,42,2,33,2,42},
};
static const struct web_case cases[] = {
  {"existing/existing-http","visit http://example.com/path now",1,links_0},
  {"existing/existing-https","https://example.com/a?b=c&d=e#f",1,links_1},
  {"existing/existing-www","www.example.com/path",1,links_2},
  {"existing/existing-email","me@example.com",1,links_3},
  {"existing/existing-email-plus","first.last+tag@example.co.uk",1,links_4},
  {"existing/existing-uppercase","HTTP://EXAMPLE.COM WWW.EXAMPLE.COM ME@EXAMPLE.COM",3,links_5},
  {"existing/existing-ftp","ftp://example.com/path",0,NULL},
  {"existing/existing-punctuation","https://example.com/a.,:!?*_~",1,links_7},
  {"existing/existing-balanced","(https://example.com/a(b)c)",1,links_8},
  {"existing/existing-unbalanced","https://example.com/a(b))",1,links_9},
  {"existing/existing-www-punctuation","www.example.com/a?!",1,links_10},
  {"existing/existing-email-punctuation","(me@example.com), me@example.com.",2,links_11},
  {"existing/existing-entity","https://example.com/a&amp;b=1",1,links_12},
  {"existing/existing-entity-tail","https://example.com/a&amp;",1,links_13},
  {"existing/existing-escaped-prefix","http\\://example.com www\\.example.com me\\@example.com",3,links_14},
  {"existing/existing-escaped-tail","https://example.com/a\\*b",1,links_15},
  {"existing/existing-unicode-path","https://example.com/\344\270\255\346\226\207/\360\237\231\202",1,links_16},
  {"existing/existing-unicode-host","https://\344\276\213\345\255\220.\346\265\213\350\257\225/a",1,links_17},
  {"existing/existing-cjk-adjacent","\344\270\255\346\226\207https://example.com/a\344\270\255\346\226\207",1,links_18},
  {"existing/existing-cjk-www","\344\270\255\346\226\207www.example.com\344\270\255\346\226\207",0,NULL},
  {"existing/existing-cjk-email","\344\270\255\346\226\207me@example.com\344\270\255\346\226\207",1,links_20},
  {"existing/existing-in-link","[https://example.com](https://elsewhere.com)",1,links_21},
  {"existing/existing-in-image","![https://example.com me@example.com](image.png)",0,NULL},
  {"existing/existing-inline-code","`https://example.com me@example.com`",0,NULL},
  {"existing/existing-fenced-code","```\012https://example.com\012```",0,NULL},
  {"existing/existing-raw-html","<div>https://example.com me@example.com</div>",0,NULL},
  {"existing/existing-html-attribute","<span title=\"https://example.com\">www.example.com</span>",1,links_26},
  {"existing/existing-footnote","text[^a]\012\012[^a]: https://example.com me@example.com",2,links_27},
  {"existing/existing-table","| URL |\012| --- |\012| www.example.com |",1,links_28},
  {"existing/existing-emphasis","*https://example.com* **me@example.com**",2,links_29},
  {"existing/existing-bare-domain","example.com example.org",0,NULL},
  {"existing/existing-invalid-host","http://_x.example.com http://example_.com http://localhost",1,links_31},
  {"existing/existing-www-invalid","www. www.x www..example.com",3,links_32},
  {"existing/existing-email-invalid","a@b a@b.c a@@example.com",1,links_33},
  {"existing/existing-newline","https://example.com\012www.example.com",2,links_34},
  {"existing/existing-tab","https://example.com\011me@example.com",2,links_35},
  {"existing/existing-angle","<https://example.com/a> <me@example.com>",2,links_36},
  {"existing/existing-fragment","www.example.com/#a",1,links_37},
  {"existing/existing-underscore","https://foo_bar.example.com/a_b",1,links_38},
  {"existing/existing-leading","xhttp://example.com xwww.example.com xme@example.com",1,links_39},
  {"existing/existing-trailing-brackets","https://example.com/a] https://example.com/a}",2,links_40},
  {"existing/existing-html-entity-domain","www.example.com&amp;",1,links_41},
  {"existing/existing-comment","https://example.com<!-- hidden -->next",1,links_42},
  {"existing/existing-email-slash","a/b@example.com",0,NULL},
  {"existing/scheme-mixed","HtTp://example.com hTtPs://example.com/a WwW.example.com",3,links_44},
  {"existing/scheme-invalid","http:/example.com https:example.com httpx://example.com",0,NULL},
  {"existing/scheme-empty","http:// https:// www.",1,links_46},
  {"existing/domain-hyphen","http://-a.example http://a-.example http://a--b.example",3,links_47},
  {"existing/domain-last-underscore","http://a.b_c http://a_b.c http://a_.bc http://a.b_c.d",0,NULL},
  {"existing/domain-empty-parts","http://a..bc http://.a.bc http://a.bc. www..a.bc",4,links_49},
  {"existing/domain-port","https://example.com:443/a https://example.com:abc/a",2,links_50},
  {"existing/domain-auth","https://user:pass@example.com/a http://user@example.com",2,links_51},
  {"existing/domain-ip","http://127.0.0.1/a http://[::1]/a",1,links_52},
  {"existing/domain-cjk-ascii","http://\344\276\213\345\255\220.com/a www.\344\276\213\345\255\220.com/a http://a.\346\265\213\350\257\225/a",3,links_53},
  {"existing/domain-astral","http://\360\237\246\212.com/a www.\360\237\246\212.com/a",2,links_54},
  {"existing/domain-combining","http://e\314\201.com/a www.e\314\201.com/a",2,links_55},
  {"existing/unicode-ascii-lowbyte-domain","http://\304\241.com/a http://\304\200.com/a www.\304\241.com/a",3,links_56},
  {"existing/unicode-ascii-lowbyte-path","http://a.bc/a\304\241 http://a.bc/a\304\200 http://a.bc/a\304\256 http://a.bc/a\342\200\241 http://a.bc/a\342\204\241 http://a.bc/a\342\200\277",6,links_57},
  {"existing/domain-numeric","http://123.456/a www.123.456/a",2,links_58},
  {"existing/path-delimiters","http://a.bc/a[]{}<> http://a.bc/a\"b http://a.bc/a'b",3,links_59},
  {"existing/path-parentheses","http://a.bc/(a(b)c)d) http://a.bc/a((b) http://a.bc/a)))",3,links_60},
  {"existing/path-punctuation-runs","http://a.bc/a:;!?.,*_~ http://a.bc/a._.~ http://a.bc/a*._",3,links_61},
  {"existing/path-interior-punctuation","http://a.bc/a!b?c:d;e*f_g~h.i,j",1,links_62},
  {"existing/path-character-reference","http://a.bc/a&copy; http://a.bc/a&#123; http://a.bc/a&#x7b;",3,links_63},
  {"existing/path-invalid-reference","http://a.bc/a&bogus; http://a.bc/a&#; http://a.bc/a&#x;",3,links_64},
  {"existing/path-reference-tail","http://a.bc/a&amp;. http://a.bc/a&amp;! http://a.bc/a&copy;x",3,links_65},
  {"existing/path-escaped-punctuation","http://a.bc/a\\!b http://a.bc/a$$b$$ http://a.bc/a\\]",3,links_66},
  {"existing/path-escaped-equals","https://a.bc/a\\==b==",1,links_67},
  {"existing/www-prefix-punctuation","(www.a.bc) [www.a.bc] /www.a.bc :www.a.bc",4,links_68},
  {"existing/www-prefix-word","awww.a.bc _www.a.bc \344\270\255www.a.bc \360\237\246\212www.a.bc",1,links_69},
  {"existing/http-prefix-word","ahttps://a.bc _https://a.bc \344\270\255https://a.bc \360\237\246\212https://a.bc",3,links_70},
  {"existing/email-local-punctuation","a.b+c-d_e@example.com a!b@example.com a#b@example.com",3,links_71},
  {"existing/email-local-marker-start",".me@example.com -me@example.com _me@example.com __me@example.com +me@example.com",5,links_72},
  {"existing/email-marker-emphasis","_me@example.com_ __me@example.com__ .me@example.com... -me@example.com --x",4,links_73},
  {"existing/email-local-invalid",".a@example.com a..b@example.com a.@example.com @example.com",3,links_74},
  {"existing/email-domain-hyphen","a@-example.com a@example-.com a@ex--ample.com",3,links_75},
  {"existing/email-domain-underscore","a@foo_bar.com a@example.c_m a@example.com_",2,links_76},
  {"existing/email-domain-punctuation","a@example.com! a@example.com? a@example.com; a@example.com:",4,links_77},
  {"existing/email-unicode","\344\270\255a@example.com a@\344\276\213\345\255\220.com a@example.\346\265\213\350\257\225 \360\237\246\212a@example.com",2,links_78},
  {"existing/email-equals","user==x==@example.com a=b@example.com a==@example.com",1,links_79},
  {"existing/email-scheme","mailto:me@example.com MAILTO:ME@EXAMPLE.COM",2,links_80},
  {"existing/highlight-http","see https://a.b/c==d== end",1,links_81},
  {"existing/highlight-surround-http","==https://a.b/c==",1,links_82},
  {"existing/highlight-www","www.a.b/==x==",1,links_83},
  {"existing/highlight-http-query","https://a.bc/?x==y==&a=b ==next==",1,links_84},
  {"existing/highlight-email","==me@example.com== me==x==@example.com",1,links_85},
  {"existing/highlight-adjacent","==x==https://a.bc/a https://a.bc/a==x==",2,links_86},
  {"existing/nested-emphasis","*http://a.bc/a* **www.a.bc** ~~me@example.com~~",3,links_87},
  {"existing/nested-link-emphasis","[*http://a.bc/a*](https://elsewhere.com)",1,links_88},
  {"existing/nested-link-reference","[www.a.bc][x]\012\012[x]: https://elsewhere.com",0,NULL},
  {"existing/nested-image-emphasis","![*www.a.bc* ==me@example.com==](image.png)",0,NULL},
  {"existing/nested-image-in-link","[![www.a.bc](image.png)](https://elsewhere.com)",1,links_91},
  {"existing/label-unclosed-link","[http://a.bc/a==x==",1,links_92},
  {"existing/label-unclosed-image","![www.a.bc/a==x==",1,links_93},
  {"existing/label-no-destination","[http://a.bc/a==x==]",1,links_94},
  {"existing/label-incomplete-destination","[http://a.bc/a==x==](unfinished",1,links_95},
  {"existing/label-undefined-reference","[www.a.bc/a==x==][missing]",1,links_96},
  {"existing/label-balanced-nested","[a [http://a.bc/a==x==] b](https://elsewhere.com)",1,links_97},
  {"existing/angle-vs-highlight","<https://a.bc/==x==> <me@example.com>",2,links_98},
  {"existing/html-inline-context","<span>http://a.bc/a==x==</span> <a href=\"https://elsewhere.com\">www.a.bc</a>",2,links_99},
  {"existing/html-block-context","<div>\012http://a.bc/a==x==\012</div>\012\012www.a.bc",1,links_100},
  {"existing/code-inline-context","`http://a.bc/a==x==` www.a.bc",1,links_101},
  {"existing/code-fence-context","```text\012http://a.bc/a==x==\012```\012\012www.a.bc",1,links_102},
  {"existing/footnote-highlight-context","x[^a]\012\012[^a]: http://a.bc/a==x== and ==next==",1,links_103},
  {"existing/table-highlight-context","| url | email |\012| --- | --- |\012| http://a.bc/a==x== | me@example.com |",2,links_104},
  {"existing/break-crlf","http://a.bc/a\015\012www.a.bc/a\015me@example.com",3,links_105},
  {"existing/tab-trailing","http://a.bc/a\011www.a.bc\011me@example.com",3,links_106},
  {"existing/unicode-nbsp","\302\240https://a.bc/a\302\240 www.a.bc/a\302\240 me@example.com\302\240",3,links_107},
  {"existing/unicode-line-separator","\342\200\250https://a.bc/a\342\200\250 www.a.bc/a\342\200\250 me@example.com\342\200\250",3,links_108},
  {"existing/unicode-paragraph-separator","\342\200\251https://a.bc/a\342\200\251 www.a.bc/a\342\200\251 me@example.com\342\200\251",3,links_109},
  {"existing/unicode-bom","https://a.bc/a\357\273\277 www.a.bc/a\357\273\277 me@example.com\357\273\277",3,links_110},
  {"existing/unicode-zwsp","\342\200\213https://a.bc/a\342\200\213 www.a.bc/a\342\200\213 me@example.com\342\200\213",3,links_111},
  {"existing/unicode-combining","\314\201https://a.bc/a\314\201 www.a.bc/a\314\201 me@example.com\314\201",3,links_112},
  {"existing/unicode-astral","\360\237\246\212https://a.bc/a\360\237\246\212 www.a.bc/a\360\237\246\212 me@example.com\360\237\246\212",3,links_113},
  {"existing/unicode-astral-punct","\360\220\204\200https://a.bc/a\360\220\204\200 www.a.bc/a\360\220\204\200 me@example.com\360\220\204\200",3,links_114},
  {"existing/prefix-unicode-http-highlight-000","",0,NULL},
  {"existing/prefix-unicode-http-highlight-001","\344\270\255",0,NULL},
  {"existing/prefix-unicode-http-highlight-002","\344\270\255\360\237\246\212",0,NULL},
  {"existing/prefix-unicode-http-highlight-003","\344\270\255\360\237\246\212 ",0,NULL},
  {"existing/prefix-unicode-http-highlight-004","\344\270\255\360\237\246\212 h",0,NULL},
  {"existing/prefix-unicode-http-highlight-005","\344\270\255\360\237\246\212 ht",0,NULL},
  {"existing/prefix-unicode-http-highlight-006","\344\270\255\360\237\246\212 htt",0,NULL},
  {"existing/prefix-unicode-http-highlight-007","\344\270\255\360\237\246\212 http",0,NULL},
  {"existing/prefix-unicode-http-highlight-008","\344\270\255\360\237\246\212 https",0,NULL},
  {"existing/prefix-unicode-http-highlight-009","\344\270\255\360\237\246\212 https:",0,NULL},
  {"existing/prefix-unicode-http-highlight-010","\344\270\255\360\237\246\212 https:/",0,NULL},
  {"existing/prefix-unicode-http-highlight-011","\344\270\255\360\237\246\212 https://",0,NULL},
  {"existing/prefix-unicode-http-highlight-012","\344\270\255\360\237\246\212 https://\344\276\213",1,links_127},
  {"existing/prefix-unicode-http-highlight-013","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220",1,links_128},
  {"existing/prefix-unicode-http-highlight-014","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.",1,links_129},
  {"existing/prefix-unicode-http-highlight-015","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.c",1,links_130},
  {"existing/prefix-unicode-http-highlight-016","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.co",1,links_131},
  {"existing/prefix-unicode-http-highlight-017","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com",1,links_132},
  {"existing/prefix-unicode-http-highlight-018","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/",1,links_133},
  {"existing/prefix-unicode-http-highlight-019","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207",1,links_134},
  {"existing/prefix-unicode-http-highlight-020","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202",1,links_135},
  {"existing/prefix-unicode-http-highlight-021","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202=",1,links_136},
  {"existing/prefix-unicode-http-highlight-022","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==",1,links_137},
  {"existing/prefix-unicode-http-highlight-023","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x",1,links_138},
  {"existing/prefix-unicode-http-highlight-024","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x=",1,links_139},
  {"existing/prefix-unicode-http-highlight-025","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x==",1,links_140},
  {"existing/prefix-unicode-http-highlight-026","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== ",1,links_141},
  {"existing/prefix-unicode-http-highlight-027","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &",1,links_142},
  {"existing/prefix-unicode-http-highlight-028","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &a",1,links_143},
  {"existing/prefix-unicode-http-highlight-029","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &am",1,links_144},
  {"existing/prefix-unicode-http-highlight-030","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp",1,links_145},
  {"existing/prefix-unicode-http-highlight-031","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp;",1,links_146},
  {"existing/prefix-unicode-http-highlight-032","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; ",1,links_147},
  {"existing/prefix-unicode-http-highlight-033","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; =",1,links_148},
  {"existing/prefix-unicode-http-highlight-034","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; ==",1,links_149},
  {"existing/prefix-unicode-http-highlight-035","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; ==\347\273\210",1,links_150},
  {"existing/prefix-unicode-http-highlight-036","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; ==\347\273\210=",1,links_151},
  {"existing/prefix-unicode-http-highlight-037","\344\270\255\360\237\246\212 https://\344\276\213\345\255\220.com/\346\226\207\360\237\231\202==x== &amp; ==\347\273\210==",1,links_152},
  {"existing/prefix-unicode-email-www-000","",0,NULL},
  {"existing/prefix-unicode-email-www-001","e",0,NULL},
  {"existing/prefix-unicode-email-www-002","e\314\201",0,NULL},
  {"existing/prefix-unicode-email-www-003","e\314\201 ",0,NULL},
  {"existing/prefix-unicode-email-www-004","e\314\201 m",0,NULL},
  {"existing/prefix-unicode-email-www-005","e\314\201 me",0,NULL},
  {"existing/prefix-unicode-email-www-006","e\314\201 me@",0,NULL},
  {"existing/prefix-unicode-email-www-007","e\314\201 me@e",0,NULL},
  {"existing/prefix-unicode-email-www-008","e\314\201 me@ex",0,NULL},
  {"existing/prefix-unicode-email-www-009","e\314\201 me@exa",0,NULL},
  {"existing/prefix-unicode-email-www-010","e\314\201 me@exam",0,NULL},
  {"existing/prefix-unicode-email-www-011","e\314\201 me@examp",0,NULL},
  {"existing/prefix-unicode-email-www-012","e\314\201 me@exampl",0,NULL},
  {"existing/prefix-unicode-email-www-013","e\314\201 me@example",0,NULL},
  {"existing/prefix-unicode-email-www-014","e\314\201 me@example.",0,NULL},
  {"existing/prefix-unicode-email-www-015","e\314\201 me@example.c",1,links_168},
  {"existing/prefix-unicode-email-www-016","e\314\201 me@example.co",1,links_169},
  {"existing/prefix-unicode-email-www-017","e\314\201 me@example.com",1,links_170},
  {"existing/prefix-unicode-email-www-018","e\314\201 me@example.com\357\274\214",1,links_171},
  {"existing/prefix-unicode-email-www-019","e\314\201 me@example.com\357\274\214w",1,links_172},
  {"existing/prefix-unicode-email-www-020","e\314\201 me@example.com\357\274\214ww",1,links_173},
  {"existing/prefix-unicode-email-www-021","e\314\201 me@example.com\357\274\214www",1,links_174},
  {"existing/prefix-unicode-email-www-022","e\314\201 me@example.com\357\274\214www.",2,links_175},
  {"existing/prefix-unicode-email-www-023","e\314\201 me@example.com\357\274\214www.a",2,links_176},
  {"existing/prefix-unicode-email-www-024","e\314\201 me@example.com\357\274\214www.a.",2,links_177},
  {"existing/prefix-unicode-email-www-025","e\314\201 me@example.com\357\274\214www.a.b",2,links_178},
  {"existing/prefix-unicode-email-www-026","e\314\201 me@example.com\357\274\214www.a.bc",2,links_179},
  {"existing/prefix-unicode-email-www-027","e\314\201 me@example.com\357\274\214www.a.bc/",2,links_180},
  {"existing/prefix-unicode-email-www-028","e\314\201 me@example.com\357\274\214www.a.bc/a",2,links_181},
  {"existing/prefix-unicode-email-www-029","e\314\201 me@example.com\357\274\214www.a.bc/a(",2,links_182},
  {"existing/prefix-unicode-email-www-030","e\314\201 me@example.com\357\274\214www.a.bc/a(b",2,links_183},
  {"existing/prefix-unicode-email-www-031","e\314\201 me@example.com\357\274\214www.a.bc/a(b)",2,links_184},
  {"existing/prefix-unicode-email-www-032","e\314\201 me@example.com\357\274\214www.a.bc/a(b))",2,links_185},
  {"existing/prefix-unicode-email-www-033","e\314\201 me@example.com\357\274\214www.a.bc/a(b))!",2,links_186},
  {"control/markerStart/root","_me@x.com .me@x.com -me@x.com +me@x.com",4,links_187},
  {"control/markerStart/quote","> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_188},
  {"control/markerStart/list","- _me@x.com .me@x.com -me@x.com +me@x.com",4,links_189},
  {"control/markerStart/nested","> - _me@x.com .me@x.com -me@x.com +me@x.com",4,links_190},
  {"control/markerStart/continuation","> first\012> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_191},
  {"control/punctuation/root"," .me@x.com -me@x.com",2,links_192},
  {"control/punctuation/quote",">  .me@x.com -me@x.com",2,links_193},
  {"control/punctuation/list","-  .me@x.com -me@x.com",2,links_194},
  {"control/punctuation/nested","> -  .me@x.com -me@x.com",2,links_195},
  {"control/punctuation/continuation","> first\012>  .me@x.com -me@x.com",2,links_196},
  {"control/doubled/root"," ..me@x.com --me@x.com",2,links_197},
  {"control/doubled/quote",">  ..me@x.com --me@x.com",2,links_198},
  {"control/doubled/list","-  ..me@x.com --me@x.com",2,links_199},
  {"control/doubled/nested","> -  ..me@x.com --me@x.com",2,links_200},
  {"control/doubled/continuation","> first\012>  ..me@x.com --me@x.com",2,links_201},
  {"control/unicode/root","\344\270\255 .me@x.com \360\237\230\200 -me@x.com",2,links_202},
  {"control/unicode/quote","> \344\270\255 .me@x.com \360\237\230\200 -me@x.com",2,links_203},
  {"control/unicode/list","- \344\270\255 .me@x.com \360\237\230\200 -me@x.com",2,links_204},
  {"control/unicode/nested","> - \344\270\255 .me@x.com \360\237\230\200 -me@x.com",2,links_205},
  {"control/unicode/continuation","> first\012> \344\270\255 .me@x.com \360\237\230\200 -me@x.com",2,links_206},
  {"control/attention/root","* .me@x.com* ** -me@x.com** _word_ __word__",2,links_207},
  {"control/attention/quote","> * .me@x.com* ** -me@x.com** _word_ __word__",2,links_208},
  {"control/attention/list","- * .me@x.com* ** -me@x.com** _word_ __word__",2,links_209},
  {"control/attention/nested","> - * .me@x.com* ** -me@x.com** _word_ __word__",2,links_210},
  {"control/attention/continuation","> first\012> * .me@x.com* ** -me@x.com** _word_ __word__",2,links_211},
  {"control/markdownLink/root","[.me@x.com](https://example.com) ![-me@x.com](/image)",1,links_212},
  {"control/markdownLink/quote","> [.me@x.com](https://example.com) ![-me@x.com](/image)",1,links_213},
  {"control/markdownLink/list","- [.me@x.com](https://example.com) ![-me@x.com](/image)",1,links_214},
  {"control/markdownLink/nested","> - [.me@x.com](https://example.com) ![-me@x.com](/image)",1,links_215},
  {"control/markdownLink/continuation","> first\012> [.me@x.com](https://example.com) ![-me@x.com](/image)",1,links_216},
  {"control/escaped/root","\\.me@x.com \\-me@x.com \\_me@x.com",3,links_217},
  {"control/escaped/quote","> \\.me@x.com \\-me@x.com \\_me@x.com",3,links_218},
  {"control/escaped/list","- \\.me@x.com \\-me@x.com \\_me@x.com",3,links_219},
  {"control/escaped/nested","> - \\.me@x.com \\-me@x.com \\_me@x.com",3,links_220},
  {"control/escaped/continuation","> first\012> \\.me@x.com \\-me@x.com \\_me@x.com",3,links_221},
  {"control/code/root","`.me@x.com -me@x.com` <code>.me@x.com</code>",1,links_222},
  {"control/code/quote","> `.me@x.com -me@x.com` <code>.me@x.com</code>",1,links_223},
  {"control/code/list","- `.me@x.com -me@x.com` <code>.me@x.com</code>",1,links_224},
  {"control/code/nested","> - `.me@x.com -me@x.com` <code>.me@x.com</code>",1,links_225},
  {"control/code/continuation","> first\012> `.me@x.com -me@x.com` <code>.me@x.com</code>",1,links_226},
  {"control/invalid/root"," .me@x -me@.com me@x. /-me@x.com",2,links_227},
  {"control/invalid/quote",">  .me@x -me@.com me@x. /-me@x.com",2,links_228},
  {"control/invalid/list","-  .me@x -me@.com me@x. /-me@x.com",2,links_229},
  {"control/invalid/nested","> -  .me@x -me@.com me@x. /-me@x.com",2,links_230},
  {"control/invalid/continuation","> first\012>  .me@x -me@.com me@x. /-me@x.com",2,links_231},
  {"prefix/dot/0","",0,NULL},
  {"prefix/dot/1"," ",0,NULL},
  {"prefix/dot/2"," .",0,NULL},
  {"prefix/dot/3"," .m",0,NULL},
  {"prefix/dot/4"," .me",0,NULL},
  {"prefix/dot/5"," .me@",0,NULL},
  {"prefix/dot/6"," .me@x",0,NULL},
  {"prefix/dot/7"," .me@x.",0,NULL},
  {"prefix/dot/8"," .me@x.c",1,links_240},
  {"prefix/dot/9"," .me@x.co",1,links_241},
  {"prefix/dot/10"," .me@x.com",1,links_242},
  {"prefix/hyphen/0","",0,NULL},
  {"prefix/hyphen/1"," ",0,NULL},
  {"prefix/hyphen/2"," -",0,NULL},
  {"prefix/hyphen/3"," -m",0,NULL},
  {"prefix/hyphen/4"," -me",0,NULL},
  {"prefix/hyphen/5"," -me@",0,NULL},
  {"prefix/hyphen/6"," -me@x",0,NULL},
  {"prefix/hyphen/7"," -me@x.",0,NULL},
  {"prefix/hyphen/8"," -me@x.c",1,links_251},
  {"prefix/hyphen/9"," -me@x.co",1,links_252},
  {"prefix/hyphen/10"," -me@x.com",1,links_253},
  {"prefix/attention/0","",0,NULL},
  {"prefix/attention/1","_",0,NULL},
  {"prefix/attention/2","_w",0,NULL},
  {"prefix/attention/3","_wo",0,NULL},
  {"prefix/attention/4","_wor",0,NULL},
  {"prefix/attention/5","_word",0,NULL},
  {"prefix/attention/6","_word_",0,NULL},
  {"prefix/attention/7","_word_ ",0,NULL},
  {"prefix/attention/8","_word_ .",0,NULL},
  {"prefix/attention/9","_word_ .m",0,NULL},
  {"prefix/attention/10","_word_ .me",0,NULL},
  {"prefix/attention/11","_word_ .me@",0,NULL},
  {"prefix/attention/12","_word_ .me@x",0,NULL},
  {"prefix/attention/13","_word_ .me@x.",0,NULL},
  {"prefix/attention/14","_word_ .me@x.c",1,links_268},
  {"prefix/attention/15","_word_ .me@x.co",1,links_269},
  {"prefix/attention/16","_word_ .me@x.com",1,links_270},
  {"eol/CRLF/quote","> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_271},
  {"eol/CRLF/list","- _me@x.com .me@x.com -me@x.com +me@x.com",4,links_272},
  {"eol/CRLF/continuation","> first\015\012> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_273},
  {"eol/CR/quote","> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_274},
  {"eol/CR/list","- _me@x.com .me@x.com -me@x.com +me@x.com",4,links_275},
  {"eol/CR/continuation","> first\015> _me@x.com .me@x.com -me@x.com +me@x.com",4,links_276},
};

static cmark_node *parse(const char *source, bool web, bool byte_feed) {
  int options = CMARK_OPT_WEB_EMPHASIS | CMARK_OPT_WEB_CHARACTER_REFERENCES |
      CMARK_OPT_WEB_POSITIONS | CMARK_OPT_WEB_TABLES | CMARK_OPT_FOOTNOTES |
      CMARK_OPT_FOOTNOTES_LOSSLESS | CMARK_OPT_MATH | CMARK_OPT_DEFINITION_LISTS |
      CMARK_OPT_REFERENCE_DEFINITIONS | CMARK_OPT_REFERENCE_USAGES |
      CMARK_OPT_CODE_METADATA | CMARK_OPT_WEB_CODE_SYNTAX;
  cmark_parser *parser = cmark_parser_new(web ? options : 0);
  const char *extensions[] = {"table", "strikethrough", "highlight", "web_autolink"};
  if (web) for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
    cmark_syntax_extension *extension = cmark_find_syntax_extension(extensions[i]);
    if (extension) cmark_parser_attach_syntax_extension(parser, extension);
  }
  if (byte_feed) for (size_t i = 0; source[i]; i++) cmark_parser_feed(parser, source + i, 1);
  else cmark_parser_feed(parser, source, strlen(source));
  cmark_node *document = cmark_parser_finish(parser);
  cmark_parser_free(parser);
  return document;
}

static void check(test_batch_runner *runner, const struct web_case *test, bool byte_feed) {
  cmark_node *document = parse(test->source, true, byte_feed);
  cmark_iter *iter = cmark_iter_new(document);
  cmark_event_type event;
  int index = 0;
  while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cmark_node *node = cmark_iter_get_node(iter);
    if (event != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_LINK ||
        cmark_node_get_reference_usage_type(node) != 0) continue;
    while (index < test->count &&
           strcmp(cmark_node_get_url(node), test->links[index].url) != 0 &&
           test->links[index].start_line == 0) index++;
    if (index < test->count) {
      const struct expected_link *expected = &test->links[index];
      STR_EQ(runner, cmark_node_get_url(node), expected->url, "%s/%d link URL %d", test->id, byte_feed, index);
      cmark_node *child = cmark_node_first_child(node);
      if (child && !cmark_node_next(child) && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
        STR_EQ(runner, cmark_node_get_literal(child), expected->text, "%s/%d link text %d", test->id, byte_feed, index);
        if (expected->text_line != 0) {
          int sl, sc, el, ec;
          INT_EQ(runner, cmark_node_get_web_position(child, &sl, &sc, &el, &ec), 3,
                 "%s/%d actual text token event %d", test->id, byte_feed, index);
          INT_EQ(runner, sl, expected->text_line, "%s/%d text start line %d", test->id, byte_feed, index);
          INT_EQ(runner, sc, expected->text_column, "%s/%d text start column %d", test->id, byte_feed, index);
          INT_EQ(runner, el, expected->text_end_line, "%s/%d text end line %d", test->id, byte_feed, index);
          INT_EQ(runner, ec, expected->text_end_column, "%s/%d text end column %d", test->id, byte_feed, index);
        }
      }
      int sl = 0, sc = 0, el = 0, ec = 0;
      int mask = cmark_node_get_web_position(node, &sl, &sc, &el, &ec);
      if (expected->start_line != 0) {
      INT_EQ(runner, mask, 3, "%s/%d actual token event %d", test->id, byte_feed, index);
      INT_EQ(runner, sl, expected->start_line, "%s/%d start line %d", test->id, byte_feed, index);
      INT_EQ(runner, sc, expected->start_column, "%s/%d start column %d", test->id, byte_feed, index);
      INT_EQ(runner, el, expected->end_line, "%s/%d end line %d", test->id, byte_feed, index);
      INT_EQ(runner, ec, expected->end_column, "%s/%d end column %d", test->id, byte_feed, index);
      }
    } else OK(runner, false, "%s/%d unexpected raw C link", test->id, byte_feed);
    index++;
  }
  while (index < test->count && test->links[index].start_line == 0) index++;
  INT_EQ(runner, index, test->count, "%s/%d all actual positioned Web links (unpositioned fromMarkdown transforms remain explicit)", test->id, byte_feed);
  cmark_iter_free(iter);
  cmark_node_free(document);
}

static void legacy_and_attention(test_batch_runner *runner) {
  const char *source = ".me@x.com -me@x.com _word_ __word__";
  cmark_node *document = parse(source, false, false);
  char *html = cmark_render_html(document, 0, NULL);
  STR_EQ(runner, html, "<p>.me@x.com -me@x.com <em>word</em> <strong>word</strong></p>\n", "extension disabled retains upstream email and attention behavior");
  free(html);
  cmark_node_free(document);
  document = parse("_me@x.com __me@x.com", true, false);
  cmark_iter *iter = cmark_iter_new(document);
  int attention_emails = 0;
  cmark_event_type event;
  while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cmark_node *node = cmark_iter_get_node(iter);
    if (event != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_LINK) continue;
    int sl, sc, el, ec;
    INT_EQ(runner, cmark_node_get_web_position(node, &sl, &sc, &el, &ec), 0,
           "attention-owned email has no Web literal event");
    cmark_node *text = cmark_node_first_child(node);
    INT_EQ(runner, cmark_node_get_web_position(text, &sl, &sc, &el, &ec), 0,
           "attention-owned email text has no Web literal event");
    attention_emails++;
  }
  INT_EQ(runner, attention_emails, 2, "native attention email compatibility count");
  cmark_iter_free(iter);
  cmark_node_free(document);
  cmark_parser *plain_positions = cmark_parser_new(0);
  cmark_parser_attach_syntax_extension(plain_positions, cmark_find_syntax_extension("web_autolink"));
  const char *positionless_source = " .me@x.com -me@x.com";
  cmark_parser_feed(plain_positions, positionless_source, strlen(positionless_source));
  document = cmark_parser_finish(plain_positions);
  cmark_parser_free(plain_positions);
  iter = cmark_iter_new(document);
  int ordinary_links = 0;
  while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cmark_node *node = cmark_iter_get_node(iter);
    if (event != CMARK_EVENT_ENTER || cmark_node_get_type(node) != CMARK_NODE_LINK) continue;
    int sl, sc, el, ec;
    INT_EQ(runner, cmark_node_get_web_position(node, &sl, &sc, &el, &ec), 0,
           "Web event option disabled retains absent link metadata");
    INT_EQ(runner, cmark_node_get_web_position(cmark_node_first_child(node), &sl, &sc, &el, &ec), 0,
           "Web event option disabled retains absent text metadata");
    ordinary_links++;
  }
  INT_EQ(runner, ordinary_links, 2, "hyphen/period matching does not require Web position metadata");
  cmark_iter_free(iter);
  cmark_node_free(document);
  document = parse("_word_ __word__", true, false);
  html = cmark_render_html(document, 0, NULL);
  STR_EQ(runner, html, "<p><em>word</em> <strong>word</strong></p>\n", "underscore remains a builtin attention delimiter");
  free(html);
  cmark_node_free(document);
}

int main(void) {
  cmark_gfm_core_extensions_ensure_registered();
  test_batch_runner *runner = test_batch_runner_new();
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    check(runner, &cases[i], false);
    check(runner, &cases[i], true);
  }
  legacy_and_attention(runner);
  test_print_summary(runner);
  int failed = !test_ok(runner);
  free(runner);
  return failed;
}
