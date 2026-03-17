> **Status:** All items from this audit have been resolved as of the February 2026 hardening sprint. See [AUDIT_HARDENING_SPRINT.md](AUDIT_HARDENING_SPRINT.md) for details.

# Asset Pack Audit — Beta 0.8 (2026-02-18)

**Status:** 11 of 14 findings fixed. 3 accepted by design.

## Fixed

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| PAK-C1 | CRIT | Integer underflow in path length bounds check | `pos > indexBuf.size()` guard |
| PAK-C3 | CRIT | `DecompressData()` silently returns compressed data | Returns empty + error log |
| PAK-H1 | HIGH | No path traversal validation on virtual paths | Rejects `..`, `/`, `\` prefixes |
| PAK-H2 | HIGH | No header version field | `ENJPAK_FORMAT_VERSION` (u16) written after flags, reader rejects unknown versions |
| PAK-H3 | HIGH | u64→streamsize truncation on 32-bit | `static_assert(sizeof(std::streamoff) >= 8)` in both AssetPacker.cpp and AssetReader.cpp |
| PAK-H4 | HIGH | Missing `file.good()` after data write | Returns false on write failure |
| PAK-M1 | MED | Footer not validated against file size | `indexOffset + indexSize + 8 == fileSize` check after reading footer |
| PAK-M2 | MED | No overlap detection between entries | Per-entry check against index region + post-parse sorted overlap scan |
| PAK-M4 | MED | Partial index parse silently accepted | Changed from WARN to ERROR, clears index, returns false |
| PAK-M5 | MED | No `gcount()` validation after reads | `file.gcount() == expected` check in ReadFile and VerifyIntegrity |
| PAK-L3 | LOW | No warning when using default obfuscation key | `ENJIN_LOG_WARN` in both AssetPacker::Begin and AssetReader::Open |

## Not a Bug

| ID | Description | Why |
|----|-------------|-----|
| PAK-C2 | Index entries not validated before insertion | `readVal()` + `break` prevents insertion |
| PAK-M3 | CRC ordering inconsistent | CRC is computed before obfuscation in both packer and reader — consistent |

## Accepted by Design

| ID | Sev | Description | Rationale |
|----|-----|-------------|-----------|
| PAK-L1 | LOW | XOR obfuscation is not cryptographically secure | Documented in CLAUDE.md security notes. XOR is intentional deterrent, not encryption |
| PAK-L2 | LOW | CRC32 is not authentication | Documented in CLAUDE.md security notes. CRC is for integrity detection, not tamper-proofing |
| PAK-L4 | LOW | No timestamp metadata in pack format | Not needed for current format. Version field (PAK-H2) handles format evolution |
