#ifndef CMARK_STRING_TOKEN_H
#define CMARK_STRING_TOKEN_H
#include "buffer.h"
/* Source string-token decoding. Never rescan decoded output. These entry points
 * are intentionally separate from legacy CommonMark code/link decoding. */
bufsize_t cmark_decode_string_token_part(cmark_strbuf *out, const unsigned char *data, bufsize_t size);
void cmark_decode_string_token(cmark_strbuf *out, const unsigned char *data, bufsize_t size);
#endif
