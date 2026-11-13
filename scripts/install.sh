#!/bin/bash
# builds installs and starts phlash at login
set -euo pipefail
cd "$(dirname "$0")/.."

LABEL=local.phlash
CERT=phlash-cert
BIN="$HOME/.local/bin/phlash"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/phlash.log"
BUILD=build/release
DOMAIN="gui/$(id -u)"

if ! command -v cmake >/dev/null; then
    echo "install: cmake not found (brew install cmake)" >&2
    exit 1
fi

# a stable cert keeps the Accessibility grant across rebuilds
has_cert() { security find-identity -p codesigning | grep -q "\"$CERT\""; }

create_cert() {
    local dir
    dir=$(mktemp -d)
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=$CERT" \
        -addext "extendedKeyUsage=critical,codeSigning" \
        -addext "keyUsage=critical,digitalSignature" \
        -keyout "$dir/key.pem" -out "$dir/cert.pem" 2>/dev/null
    openssl pkcs12 -export -inkey "$dir/key.pem" -in "$dir/cert.pem" \
        -out "$dir/cert.p12" -passout pass:phlash
    security import "$dir/cert.p12" -P phlash -T /usr/bin/codesign >/dev/null
    rm -rf "$dir"
}

if ! has_cert; then
    echo "install: creating code-signing certificate '$CERT' in your login keychain"
    create_cert || true
fi

if has_cert; then
    IDENTITY="$CERT"
else
    IDENTITY="-"
    echo "install: no '$CERT' certificate, signing ad hoc" \
         "(you'll have to re-grant Accessibility after every install)" >&2
fi

cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DPHLASH_SIGN_IDENTITY="$IDENTITY" >/dev/null
cmake --build "$BUILD"
"$BUILD/phlash" --check >/dev/null  # creates the config on first install

launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
for _ in $(seq 50); do
    launchctl print "$DOMAIN/$LABEL" >/dev/null 2>&1 || break
    sleep 0.1
done

mkdir -p "$(dirname "$BIN")" "$(dirname "$PLIST")" "$(dirname "$LOG")"
# replace not overwrite since the kernel caches signatures per file
cp "$BUILD/phlash" "$BIN.new"
# re-sign since CMake only signs on relink
codesign --force --sign "$IDENTITY" --identifier phlash "$BIN.new"
mv -f "$BIN.new" "$BIN"

sed -e "s|@BIN@|$BIN|g" -e "s|@LOG@|$LOG|g" launchd/$LABEL.plist > "$PLIST"
launchctl bootstrap "$DOMAIN" "$PLIST"

echo "install: phlash installed to $BIN and started (signed with '$IDENTITY')"
echo "install: log at $LOG"
