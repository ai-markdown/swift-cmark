# AI Markdown CMark

This branch provides the C parser behavior used by AI Markdown: lossless footnotes and references, highlight and definition-list syntax, math parsing, Web-compatible inline rules, and source metadata for code and soft breaks. Custom parser options retain their opt-in behavior.

The upstream baseline is `08ddb528923cc1a6527e02b7a1aee9e516ca749a`. This repository retains upstream names; `ai-markdown-parser-swift` owns symbol isolation and Swift Package distribution. Maintain parser changes here rather than reapplying the same patches in the integration repository.

Original implementation provenance, patch paths, and checksums are recorded in `provenance.json`. Third-party licenses are bundled in `ThirdPartyLicenses`; upstream license files remain unchanged. The source paths in the provenance record refer to the pinned `ai-markdown-swift` source repository, not to files in this fork.
