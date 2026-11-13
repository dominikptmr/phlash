# phlash

Keyboard-driven macOS Spaces: caps lock becomes Hyper, and Hyper + key jumps
instantly (no animation) to whichever space an app is on.

## Features

phlash turns caps lock into a **Hyper** key: hold it and press another key, or
tap it alone or with a modifier. Modifiers (cmd, opt, ctrl, shift, fn, either
side or one side) can also be tapped on their own. You set up what each combination does in
`~/.config/phlash/config` (see [docs/configuration.md](docs/configuration.md)).

- **App shortcuts**: jump straight to an app's window on any space, without the
  sliding animation
- **App fallbacks**: one key for several apps, the first open one wins
- **App tabs**: press an app's key again to go to its next tab
- **App windows**: cycle through all windows of an app
- **Last space**: jump back to the space you were just on
- **Cycle spaces**: step forward or backward through all spaces
- **Desktop**: jump to the first space without a fullscreen app
- **Tabs**: cycle through the tabs of the app in front
- **Windows**: cycle through the windows of the app in front, across spaces
- **Shortcuts**: send any keyboard shortcut to the app in front
- **Commands**: run a shell command

Space switching uses
[InstantSpaceSwitcher](https://github.com/jurplel/InstantSpaceSwitcher) (MIT).

## Requirements

- cmake and the Xcode Command Line Tools
- System Settings → Desktop & Dock: turn off **Automatically rearrange Spaces
  based on most recent use**
- System Settings → Keyboard → Keyboard Shortcuts… → Modifier Keys: **Caps Lock**
  set to ⇪ Caps Lock
- No other caps lock remapper (Hyperkey, Karabiner)

## Install

1. Run `scripts/install.sh`. On first run it creates a signing certificate
   (`phlash-cert`), so macOS keeps the Accessibility permission across
   reinstalls. When `codesign` asks to use the key, click **Always Allow**.
2. System Settings → Privacy & Security → Accessibility: enable `phlash`.
   phlash starts working within a few seconds.

## Usage

| Task | Command |
|---|---|
| Reload config | `pkill -HUP phlash` |
| Restart | `launchctl kickstart -k gui/$(id -u)/local.phlash` |
| Log | `tail -f ~/Library/Logs/phlash.log` |
| Update | `git pull && scripts/install.sh` |
| Uninstall | `scripts/uninstall.sh` |

## License

MIT, see [LICENSE](LICENSE).
