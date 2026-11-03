#!/bin/bash
# Stops workspeed and removes the LaunchAgent and binary. Keeps the config and log.
set -euo pipefail

LABEL=local.workspeed

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
rm -f "$HOME/Library/LaunchAgents/$LABEL.plist" "$HOME/.local/bin/workspeed"

# workspeed undoes this on a clean exit; make sure after a crash.
hidutil property --set '{"UserKeyMapping":[]}' >/dev/null

echo "uninstall: workspeed removed (config in ~/.config/workspeed kept)"
echo "uninstall: remove it from System Settings → Privacy & Security → Accessibility yourself"
