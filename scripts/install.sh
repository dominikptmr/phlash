#!/bin/bash
# Builds workspeed (Release), installs it to ~/.local/bin and runs it at login
# as a LaunchAgent. Safe to rerun after changes: it rebuilds and restarts.
set -euo pipefail
cd "$(dirname "$0")/.."

LABEL=local.workspeed
CERT=workspeed-cert
BIN="$HOME/.local/bin/workspeed"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/Library/Logs/workspeed.log"
CONFIG="$HOME/.config/workspeed/config"
BUILD=build/release
DOMAIN="gui/$(id -u)"

# cmake from PATH, else the copy bundled with CLion.
CMAKE=""
for c in "$(command -v cmake || true)" \
         "$HOME"/Applications/CLion.app/Contents/bin/cmake/mac/*/bin/cmake \
         /Applications/CLion.app/Contents/bin/cmake/mac/*/bin/cmake; do
    if [[ -x "$c" ]]; then CMAKE="$c"; break; fi
done
if [[ -z "$CMAKE" ]]; then
    echo "install: cmake not found (install it with Homebrew, or CLion)" >&2
    exit 1
fi

# A real certificate keeps the code signature, and with it the Accessibility
# grant, the same across rebuilds. Ad hoc signing works but loses the grant.
if security find-identity -p codesigning | grep -q "\"$CERT\""; then
    IDENTITY="$CERT"
else
    IDENTITY="-"
    echo "install: no '$CERT' certificate, signing ad hoc (see README:" \
         "you'll have to re-grant Accessibility after every install)" >&2
fi

if [[ ! -f "$CONFIG" ]]; then
    mkdir -p "$(dirname "$CONFIG")"
    cp config.example "$CONFIG"
    echo "install: created $CONFIG from config.example"
fi

"$CMAKE" -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DWORKSPEED_SIGN_IDENTITY="$IDENTITY" >/dev/null
"$CMAKE" --build "$BUILD"
"$BUILD/workspeed" --check >/dev/null

# Stop the running copy and wait until launchd has let go of it.
launchctl bootout "$DOMAIN/$LABEL" 2>/dev/null || true
for _ in $(seq 50); do
    launchctl print "$DOMAIN/$LABEL" >/dev/null 2>&1 || break
    sleep 0.1
done

mkdir -p "$(dirname "$BIN")" "$(dirname "$PLIST")" "$(dirname "$LOG")"
# Replace the file instead of overwriting it: the kernel caches code signatures
# per file, and a signed binary rewritten in place gets killed on launch.
cp "$BUILD/workspeed" "$BIN.new"
mv -f "$BIN.new" "$BIN"

sed -e "s|@BIN@|$BIN|g" -e "s|@LOG@|$LOG|g" launchd/$LABEL.plist > "$PLIST"
launchctl bootstrap "$DOMAIN" "$PLIST"

echo "install: workspeed installed to $BIN and started (signed with '$IDENTITY')"
echo "install: log at $LOG"
