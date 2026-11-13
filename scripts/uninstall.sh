#!/bin/bash
set -euo pipefail

LABEL=local.phlash

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
rm -f "$HOME/Library/LaunchAgents/$LABEL.plist" "$HOME/.local/bin/phlash"

echo "uninstall: phlash removed (config in ~/.config/phlash kept)"
echo "uninstall: remove it from System Settings → Privacy & Security → Accessibility yourself"
