# AI Markdown CMark

This branch provides the C parser behavior used by AI Markdown: lossless footnotes and references, highlight and definition-list syntax, math parsing, Web-compatible inline rules, source metadata for code and soft breaks, and Web mdast list and list-item spread derived from a block-parser ledger. Custom parser options retain their opt-in behavior.

`CMARK_OPT_LIST_SPREAD_METADATA` records, per physical line, the containers that continue, open, and close, the leaf block that consumed the line, and whether an actual line ending was present. After parsing, `src/list_spread.c` expands that ledger into the document events read by `mdast-util-from-markdown`'s `prepareList`, applies the container-exit reordering of `micromark-util-subtokenize`, and derives the two independent `spread` values. `cmark_node_get_list_spread` returns 0 when no value was captured: for definition-list marker candidates, footnote definitions that this parser closes on whitespace-only or CR-only blank lines, re-entrant feeding, or lists whose derived items differ from the parsed items. Rendering and `tight` are unchanged.

The upstream baseline is `08ddb528923cc1a6527e02b7a1aee9e516ca749a`. This repository retains upstream names; `ai-markdown-parser-swift` owns symbol isolation and Swift Package distribution. Maintain parser changes here rather than reapplying the same patches in the integration repository.

Original implementation provenance, patch paths, and checksums are recorded in `provenance.json`. Third-party licenses are bundled in `ThirdPartyLicenses`; upstream license files remain unchanged. The source paths in the provenance record refer to the pinned `ai-markdown-swift` source repository, not to files in this fork.
