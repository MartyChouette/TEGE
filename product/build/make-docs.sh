#!/usr/bin/env bash
# Build the product documents from Markdown source.
#
# Markdown is the source of truth so the text diffs in git. LibreOffice is the
# reading and editing format, because that is what gets opened when somebody
# wants to read the manual rather than maintain it.
#
# Usage:  bash product/build/make-docs.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRODUCT="$(dirname "$HERE")"
OUT="$PRODUCT/out"

# Pandoc ships outside PATH on this machine often enough to be worth looking.
PANDOC="$(command -v pandoc || true)"
if [ -z "$PANDOC" ] && [ -x "$HOME/AppData/Local/Pandoc/pandoc" ]; then
    PANDOC="$HOME/AppData/Local/Pandoc/pandoc"
fi
if [ -z "$PANDOC" ]; then
    echo "pandoc not found. Install it, or edit PANDOC in $0" >&2
    exit 1
fi

mkdir -p "$OUT"

# Chapter order is the numeric filename prefix. Front matter first so pandoc
# picks up its title block; numbering the files is what keeps the order out of
# this script, where it would drift.
CHAPTERS=()
while IFS= read -r f; do CHAPTERS+=("$f"); done < <(find "$PRODUCT/manual" -maxdepth 1 -name '*.md' | sort)

if [ ${#CHAPTERS[@]} -eq 0 ]; then
    echo "no chapters found in $PRODUCT/manual" >&2
    exit 1
fi

echo "Building from ${#CHAPTERS[@]} chapters:"
for c in "${CHAPTERS[@]}"; do echo "  $(basename "$c")"; done

"$PANDOC" "${CHAPTERS[@]}" \
    --from=markdown \
    --to=odt \
    --toc \
    --toc-depth=2 \
    --standalone \
    --reference-doc="$HERE/reference.odt" \
    --output="$OUT/TEGE-Manual.odt"

echo
echo "Wrote $OUT/TEGE-Manual.odt"
echo "Open it in LibreOffice Writer."
