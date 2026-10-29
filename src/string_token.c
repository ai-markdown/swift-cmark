/* Port of micromark string-token semantics; provenance and licenses are tracked
 * in AIMarkdown/provenance.json and AIMarkdown/ThirdPartyLicenses. */
#include "string_token.h"
#include "houdini.h"
#include "utf8.h"
#include "cmark_ctype.h"

// micromark-util-decode-numeric-character-reference and character-reference digit limits.
static bufsize_t string_entity(cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  if (!size || data[0] != '#') return houdini_unescape_ent(out, data, size);
  bufsize_t i = 1;
  int radix = 10, limit = 7, count = 0;
  if (i < size && (data[i] == 'x' || data[i] == 'X')) { radix = 16; limit = 6; i++; }
  int code = 0;
  for (; i < size; i++) {
    unsigned char c = data[i]; int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (radix == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (radix == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0) break;
    count++; code = code * radix + digit;
    if (code > 0x110000) code = 0x110000;
  }
  if (!count || count > limit || i >= size || data[i] != ';') return 0;
  if (code < 9 || code == 11 || (code > 13 && code < 32) ||
      (code > 126 && code < 160) || (code >= 0xd800 && code < 0xe000) ||
      (code >= 0xfdd0 && code <= 0xfdef) || (code & 0xffff) == 0xffff ||
      (code & 0xffff) == 0xfffe || code > 0x10ffff) code = 0xfffd;
  cmark_utf8proc_encode_char(code, out);
  return i + 1;
}

bufsize_t cmark_decode_string_token_part(cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  if (!size) return 0;
  if (data[0] == '\\' && size > 1 && cmark_ispunct(data[1])) {
    cmark_strbuf_putc(out, data[1]);
    return 2;
  }
  if (data[0] == '&') {
    bufsize_t consumed = string_entity(out, data + 1, size - 1);
    if (consumed) return consumed + 1;
  }
  cmark_strbuf_putc(out, data[0]);
  return 1;
}

void cmark_decode_string_token(cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  for (bufsize_t i = 0; i < size; ) i += cmark_decode_string_token_part(out, data + i, size - i);
}
