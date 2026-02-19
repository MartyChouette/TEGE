# Asset Pack Audit — Beta 0.8 (2026-02-18)

**Status:** 4 of 14 findings fixed. 10 deferred (0 CRIT, 2 HIGH design-level, 5 MED, 4 LOW).

## Fixed

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| PAK-C1 | CRIT | Integer underflow in path length bounds check | `pos > indexBuf.size()` guard |
| PAK-C3 | CRIT | `DecompressData()` silently returns compressed data | Returns empty + error log |
| PAK-H1 | HIGH | No path traversal validation on virtual paths | Rejects `..`, `/`, `\` prefixes |
| PAK-H4 | HIGH | Missing `file.good()` after data write | Returns false on write failure |

## Not a Bug

| ID | Description | Why |
|----|-------------|-----|
| PAK-C2 | Index entries not validated before insertion | `readVal()` + `break` prevents insertion |

## Deferred

| ID | Sev | Description |
|----|-----|-------------|
| PAK-H2 | HIGH | No header version field (format change needed) |
| PAK-H3 | HIGH | u64→streamsize truncation on 32-bit (not a target) |
| PAK-M1-5 | MED | Footer bounds, overlap detection, CRC order, index cap, gcount validation |
| PAK-L1-4 | LOW | XOR weakness, CRC not auth'd, hardcoded key, no timestamps |
