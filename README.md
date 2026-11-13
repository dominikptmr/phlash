# phlash

Keyboard-driven macOS Spaces: caps lock becomes Hyper, and Hyper + key jumps
instantly (no animation) to whichever space an app is on.

Space switching uses
[InstantSpaceSwitcher](https://github.com/jurplel/InstantSpaceSwitcher) (MIT).

## Requirements

- macOS, cmake and the Xcode Command Line Tools
- System Settings → Desktop & Dock: turn off **Automatically rearrange Spaces
  based on most recent use**
- System Settings → Keyboard → Keyboard Shortcuts… → Modifier Keys: **Caps Lock**
  set to ⇪ Caps Lock
- No other caps lock remapper

## Install

1. Run `scripts/install.sh`. When `codesign` asks to use the `phlash-cert` key,
   click **Always Allow**.
2. System Settings → Privacy & Security → Accessibility: enable `phlash`.

The install creates an empty `~/.config/phlash/config`.

## Quick start

Find the names of your apps:

```
~/.local/bin/phlash --list
```

Put this into `~/.config/phlash/config`:

```
# jump back and forth between the last two used spaces
hyper : space back
# cycle through tabs
shift + hyper : tab next

# jump to Safari if open, press again to cycle through its tabs
hyper - s : app Safari
hyper - d : app Terminal
hyper - f : app Mail

# holding cmd while pressing an app shortcut cycles through the apps windows instead of its tabs
set app-window-mod cmd

# max duration in ms for a key press to count as a tap
set tap-ms 200
```

## Configuration

One binding or setting per line:

```
<trigger> : <action>
set <setting> <value>
```

- `#` starts a comment, but only at the beginning of a line
- each trigger can be bound once; a second line fails with `already bound`
- the order of lines doesn't matter
- phlash reloads the config within a second of saving, and when the keyboard
  layout changes; a config with errors is reported with line numbers in the
  log and the previous one stays active
- `~/.local/bin/phlash --check` checks the config without reloading

### Modifiers

| Name | Key | Use |
|---|---|---|
| `hyper` | caps lock | every trigger except a single modifier tap |
| `cmd` `opt` `ctrl` `shift` | either side | triggers and shortcuts |
| `lcmd` `lopt` `lctrl` `lshift` | left side | single modifier taps |
| `rcmd` `ropt` `rctrl` `rshift` | right side | single modifier taps |
| `fn` | fn / globe | single modifier taps |

While phlash runs, caps lock is remapped to F18 and acts as `hyper`, a
modifier no app uses. Caps lock no longer toggles capital letters; the mapping
is removed when phlash stops.

### Keys

| Form | Examples |
|---|---|
| what the key types in your layout, lowercase | `s` `ö` `#` `1` `-` |
| name | `return` `tab` `space` `delete` `forwarddelete` `escape` |
| | `left` `right` `up` `down` `home` `end` `pageup` `pagedown` `f1` … `f20` |
| macOS keycode in hex, for keys without a name | `0x0a` |

Characters follow the active keyboard layout: on a German keyboard `z` and `y`
are swapped compared to US.

### Triggers

**Key combo**: hold Hyper, optionally with more modifiers, and press a key.

```
hyper [+ <mod> ...] - <key> : <action>
```

```
hyper - s : app Safari
hyper + shift - s : app Mail
hyper - - : space back
```

- every key pressed with Hyper is swallowed, bound or not, so it never types
  into the app
- holding the key down doesn't repeat the action

**Tap**: press and release without pressing another key, within `tap-ms`.

```
[<mod> + ...] hyper : <action>
<modifier> : <action>
```

```
hyper : space back
cmd + hyper : space cycle
cmd + shift + hyper : space cycle prev
shift : tab next
rcmd : app Ghostty
```

- Hyper can be tapped while holding `cmd` `opt` `ctrl` `shift`; each
  combination is its own trigger
- a single modifier must be tapped alone, with no other key, modifier or mouse
  click in between; it still reaches the app, so shortcuts like cmd+c keep
  working
- a one-side line (`rcmd`) wins over an either-side line (`cmd`) for the same key

### Shortcuts

A key with modifiers that phlash types into the app in front, used by `key`
and `set tab`.

```
[<mod>+...]<key>
```

```
cmd+t
ctrl+shift+tab
ctrl+right
```

- modifiers are `cmd` `opt` `ctrl` `shift`, joined with `+`
- `cmd++` is cmd and the plus key
- for `tab prev` phlash reverses the next-tab shortcut: arrow keys flip
  (`ctrl+right` → `ctrl+left`), any other shortcut gets shift added or removed
  (`ctrl+tab` → `ctrl+shift+tab`)

### Actions

| Action | Does |
|---|---|
| `app <name> [\| <name> ...]` | jump to the app's window |
| `space back` | jump to the last used space |
| `space cycle [prev]` | next / previous space of the current display, wrapping around |
| `desktop` | first space of the current display without a fullscreen app |
| `tab next \| prev` | next / previous tab of the app in front |
| `window next \| prev` | next / previous window of the app in front, across spaces |
| `key <shortcut>` | type a [shortcut](#shortcuts) into the app in front |
| `exec <command>` | run a shell command with `/bin/sh -c` |

**app**

```
hyper - s : app Safari
hyper - d : app WebStorm | PhpStorm | Visual Studio Code
```

- the name is what `phlash --list` shows or the `.app` name, in any case
- with `|`, the first app that has an open window wins
- prefers a window on the current space, else the frontmost one; minimized
  windows don't count
- never launches apps: nothing happens if none has a window
- pressing the key again while the app is in front sends its next-tab
  shortcut, see `set tab`

**Multiple displays**: spaces switch instantly only on the display under the
mouse pointer. An app on another display is brought to the front with the
normal macOS animation.

**space back** also remembers space changes made with the trackpad, Mission
Control or by clicking an app.

**tab** keeps the shortcut's modifiers held, so tab switchers (Firefox, Zen)
stay open while you keep going:

- on a Hyper tap line until you let go of its modifiers (shift aside)
- on a key combo line until you let go of Hyper
- on a single modifier tap line not at all

### Settings

| Setting | Default | Does |
|---|---|---|
| `set tab <shortcut>` | `ctrl+tab` | next-tab shortcut for all apps |
| `set tab <shortcut> for <name> [\| <name> ...]` | | next-tab shortcut for these apps |
| `set tab none for <name> [\| <name> ...]` | | no tab switching in these apps |
| `set tap-ms <50-1000>` | `300` | how fast a tap must be released, in ms |
| `set app-window-mod <mods> \| none` | `none` | Hyper + these + an app key cycles that line's windows |

```
set tab ctrl+tab
set tab ctrl+right for WebStorm | PhpStorm | CLion
set tab none for Finder
set tap-ms 200
set app-window-mod cmd
```

- each app can be in only one `set tab … for` list; apps don't need an app line
- with `app-window-mod`, Hyper + cmd + an app key cycles all windows of that
  line's apps: app by app in line order, each by space from left to right,
  wrapping around. If none of them is in front, it jumps like the app key.
  It only applies if that combination isn't bound itself

### Example

```
set tab ctrl+right for WebStorm | CLion
set app-window-mod cmd

hyper : space back
cmd + hyper : space cycle
cmd + shift + hyper : space cycle prev
opt + hyper : tab next
opt + shift + hyper : tab prev
ctrl + hyper : window next
shift : tab next

hyper - a : desktop
hyper - s : app Zen | Google Chrome
hyper - d : app WebStorm | CLion | Visual Studio Code
hyper - f : app Claude | ChatGPT
hyper - tab : key cmd+t
```

## Usage

| Task | Command |
|---|---|
| Check config | `~/.local/bin/phlash --check` |
| List spaces and apps | `~/.local/bin/phlash --list` |
| Restart | `launchctl kickstart -k gui/$(id -u)/local.phlash` |
| Log | `tail -f ~/Library/Logs/phlash.log` |
| Debug log | stop phlash with `launchctl bootout gui/$(id -u)/local.phlash`, then run `~/.local/bin/phlash --debug` |
| Update | `git pull && scripts/install.sh` |
| Uninstall | `scripts/uninstall.sh` |

## Troubleshooting

- **Shortcuts stopped working:** check the log. If something reset the caps
  lock mapping, phlash restores it within a second and logs it.
- **An app key does nothing:** the app has no open window, or the name doesn't
  match `~/.local/bin/phlash --list`.
- **Accessibility is asked after every install:** allow `codesign` to use the
  `phlash-cert` key.
- **`already running`:** only one phlash can run; stop the background copy first
  (see Debug log). Start it again with `scripts/install.sh`.

## License

MIT, see [LICENSE](LICENSE).
