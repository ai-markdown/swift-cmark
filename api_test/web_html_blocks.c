/* Reuse the strict block/position/tracking harness with independent actual
 * Web HTML declaration and lazy-container fixtures. */
#define WEB_HTML_BLOCK_BOUNDARIES
#define BLOCK_BOUNDARY_FIXTURE "fixtures/html_block_cases.inc"
#include "web_list_boundaries.c"
