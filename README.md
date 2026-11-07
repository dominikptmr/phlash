# phlash

Keyboard-driven macOS Spaces: caps lock becomes Hyper, Hyper + key jumps instantly
(no animation) to whichever space an app is on.

Everything it does is set in `~/.config/phlash/config` (see
[config.example](config.example)):

- **Hyper + key** (caps lock held): `app NAME` jumps to the app's window on any
  space (native fullscreen and Split View included) and focuses it; it does
  nothing if the app has no open window. Pressing the key again while the app is
  in front goes to its next tab. Fallbacks: `app WebStorm | PhpStorm`.
- **Taps** (caps lock tapped, alone or while holding modifiers), e.g.
  `hyper : space back`, `cmd + hyper : space cycle`, `opt + hyper : tab next`,
  `ctrl + hyper : window next`. Tab switchers stay open until the modifier is let go.
- Actions: `app`, `desktop`, `space back|cycle`, `tab next|prev`,
  `window next|prev`, `key <shortcut>` (e.g. `hyper - t : key cmd+t`), `exec`.
  Settings: `set tab`, `set tap-ms`, `set app-window-mod`.
- Switching is instant through
  [InstantSpaceSwitcher](https://github.com/jurplel/InstantSpaceSwitcher)'s core,
  vendored unmodified in [third_party/InstantSpaceSwitcher](third_party/InstantSpaceSwitcher)
  (MIT) and called in-process.

## Requirements

- System Settings → Desktop & Dock → Mission Control: turn off
  **Automatically rearrange Spaces based on most recent use**.
- No other caps lock remapper running (Hyperkey, Karabiner caps lock rules).

## Setup

1. **Code-signing certificate** (once). Keeps the Accessibility grant across rebuilds.
   Open Keychain Access (`open -a "Keychain Access"`), then
   Keychain Access menu → Certificate Assistant → Create a Certificate…
   - Name: `phlash-cert`
   - Identity Type: Self Signed Root
   - Certificate Type: Code Signing

   Without it, `install.sh` signs ad hoc and you re-grant Accessibility after every install.

2. **Install and start** (also creates `~/.config/phlash/config` if missing):

   ```bash
   scripts/install.sh
   ```

3. **Permissions.** macOS asks for Accessibility on first start: enable
   `phlash` in System Settings → Privacy & Security → Accessibility. phlash
   waits and starts working as soon as it's granted.

## Day to day

| Task | Command |
|---|---|
| Reload config after editing | `pkill -HUP phlash` |
| Check config | `~/.local/bin/phlash --check` |
| See spaces and their apps | `~/.local/bin/phlash --list` |
| Log | `tail -f ~/Library/Logs/phlash.log` |
| Restart | `launchctl kickstart -k gui/$(id -u)/local.phlash` |
| Stop until next login | `launchctl bootout gui/$(id -u)/local.phlash` |
| Update after code changes | `scripts/install.sh` |
| Uninstall | `scripts/uninstall.sh` |

If caps lock ever stays dead after a crash:
`hidutil property --set '{"UserKeyMapping":[]}'`

## Development

Only one phlash runs at a time; a second one exits with "already running". To
run from CLion, stop the installed one first (`launchctl bootout …` above) and
start it again afterwards with `scripts/install.sh` or
`launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/local.phlash.plist`.

Run with `--debug` to log every key event and triggered hotkey.

Two parts rely on undocumented macOS internals. If an update breaks something,
look there first:

- space lookups: private SkyLight calls in [include/cgs_private.h](include/cgs_private.h)
  (`--list` shows wrong or no spaces)
- switching: synthetic Dock gestures in
  [third_party/InstantSpaceSwitcher](third_party/InstantSpaceSwitcher) (keys are
  caught but nothing switches); upstream breaks the same way and usually gets
  fixed first, see that directory's README for updating.
