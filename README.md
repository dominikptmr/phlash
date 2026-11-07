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

## Install

```bash
brew install dominikptmr/tap/phlash
brew services start phlash
```

macOS asks for Accessibility on first start: enable `phlash` in System Settings →
Privacy & Security → Accessibility. phlash waits and starts working as soon as
it's granted (after `brew upgrade` macOS may ask again). On first start it
creates `~/.config/phlash/config` from [config.example](config.example).

| Task | Command |
|---|---|
| Reload config after editing | `pkill -HUP phlash` |
| Check config | `phlash --check` |
| See spaces and their apps | `phlash --list` |
| Log | `tail -f $(brew --prefix)/var/log/phlash.log` |
| Restart / stop | `brew services restart phlash` / `brew services stop phlash` |
| Update | `brew upgrade phlash` |
| Uninstall | `brew services stop phlash && brew uninstall phlash` |

### From source

`scripts/install.sh` builds phlash, installs it to `~/.local/bin` and runs it at
login as the LaunchAgent `local.phlash` (log: `~/Library/Logs/phlash.log`,
uninstall: `scripts/uninstall.sh`). With a self-signed code-signing certificate
named `phlash-cert` (Keychain Access → Certificate Assistant → Create a
Certificate…, Self Signed Root, Code Signing), the Accessibility grant survives
reinstalls; without it, `install.sh` signs ad hoc.

If caps lock ever stays dead after a crash:
`hidutil property --set '{"UserKeyMapping":[]}'`

## Development

Only one phlash runs at a time; a second one exits with "already running". To
run from CLion, stop the installed one first (`brew services stop phlash`, or
`launchctl bootout gui/$(id -u)/local.phlash` for a `scripts/install.sh` copy).

Run with `--debug` to log every key event and triggered hotkey.

Two parts rely on undocumented macOS internals. If an update breaks something,
look there first:

- space lookups: private SkyLight calls in [include/cgs_private.h](include/cgs_private.h)
  (`--list` shows wrong or no spaces)
- switching: synthetic Dock gestures in
  [third_party/InstantSpaceSwitcher](third_party/InstantSpaceSwitcher) (keys are
  caught but nothing switches); upstream breaks the same way and usually gets
  fixed first, see that directory's README for updating.

## Releasing

1. Tag and push: `git tag v0.1.0 && git push origin v0.1.0`
2. With the tap repo `dominikptmr/homebrew-tap` cloned next to this one:
   `scripts/update-formula.sh v0.1.0` writes `Formula/phlash.rb` from
   [packaging/homebrew/phlash.rb](packaging/homebrew/phlash.rb) with the release
   tarball's checksum.
3. Commit and push the tap.

## License

MIT, see [LICENSE](LICENSE). The vendored InstantSpaceSwitcher code is MIT
licensed by its author, see [its license](third_party/InstantSpaceSwitcher/LICENSE).
