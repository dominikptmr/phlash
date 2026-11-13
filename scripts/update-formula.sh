#!/bin/bash
# writes the formula for a pushed tag into the tap checkout
#
#   scripts/update-formula.sh v0.1.0 [../homebrew-tap]
set -euo pipefail
cd "$(dirname "$0")/.."

TAG="${1:?usage: $0 <tag> [tap directory]}"
TAP="${2:-../homebrew-tap}"
URL="https://github.com/dominikptmr/phlash/archive/refs/tags/$TAG.tar.gz"

if [[ ! -d "$TAP" ]]; then
    echo "update-formula: $TAP not found (clone dominikptmr/homebrew-tap there)" >&2
    exit 1
fi

# needs the tag pushed
SHA=$(curl -fsSL "$URL" | shasum -a 256 | cut -d' ' -f1)

mkdir -p "$TAP/Formula"
sed -e "s|@URL@|$URL|" -e "s|@SHA256@|$SHA|" packaging/homebrew/phlash.rb > "$TAP/Formula/phlash.rb"
echo "update-formula: wrote $TAP/Formula/phlash.rb ($TAG, sha256 $SHA)"
