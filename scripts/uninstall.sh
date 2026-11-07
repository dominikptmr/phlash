#!/bin/bash
# Stops phlash and removes the LaunchAgent and binary. Keeps the config and log.
set -euo pipefail

LABEL=local.phlash

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
rm -f "$HOME/Library/LaunchAgents/$LABEL.plist" "$HOME/.local/bin/phlash"

# phlash undoes this on a clean exit; make sure after a crash.
hidutil property --set '{"UserKeyMapping":[]}' >/dev/null

echo "uninstall: phlash removed (config in ~/.config/phlash kept)"
echo "uninstall: remove it from System Settings → Privacy & Security → Accessibility yourself"
