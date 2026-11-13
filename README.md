# phlash

Keyboard-driven macOS Spaces: caps lock becomes Hyper, and Hyper + key jumps
instantly (no animation) to whichever space an app is on.

Space switching uses
[InstantSpaceSwitcher](https://github.com/jurplel/InstantSpaceSwitcher) (MIT).

## Features

- **[App shortcuts](#app)**: jump to an app's window on any space, without the
  sliding animation, with fallbacks if the first app isn't open
- **[App tabs](#set-tab)**: press an app's key again to go to its next tab
- **[App windows](#set-app-window-mod)**: cycle through all windows of an app
- **[Last space](#space-back)**, **[cycle spaces](#space-cycle)**, **[desktop](#desktop)**
- **[Tabs](#tab)** and **[windows](#window)** of the app in front
- **[Shortcuts](#key)** and **[shell commands](#exec)**
- Triggers: [Hyper + key](#hyper--key), [tapping Hyper](#hyper-tap) alone or
  with modifiers, or [tapping a single modifier](#modifier-tap)

## Requirements

- macOS, cmake and the Xcode Command Line Tools
- System Settings → Desktop & Dock: turn off **Automatically rearrange Spaces
  based on most recent use**
- System Settings → Keyboard → Keyboard Shortcuts… → Modifier Keys: **Caps Lock**
  set to ⇪ Caps Lock (not "No Action")
- No other caps lock remapper (Hyperkey, Karabiner caps lock rules)

## Install

1. Run `scripts/install.sh`. On first run it creates a signing certificate
   (`phlash-cert`), so macOS keeps the Accessibility permission across
   reinstalls. When `codesign` asks to use the key, click **Always Allow**.
2. System Settings → Privacy & Security → Accessibility: enable `phlash`.
   phlash starts working within a few seconds.

The install creates an empty `~/.config/phlash/config`. Until you add lines,
caps lock does nothing.

## Quick start

Find the names of your apps:

```
~/.local/bin/phlash --list
```

Put this into `~/.config/phlash/config`, with your own apps:

```
hyper : space back
hyper - s : app Safari
hyper - t : app Terminal
hyper - m : app Mail
opt + hyper : tab next
```

Reload with `pkill -HUP phlash`. Now caps lock + s jumps to Safari, tapping
caps lock jumps back.

## Configuration

phlash reads `~/.config/phlash/config`. It is created empty on install and
whenever it's missing.

- [Config file](#config-file) · [Hyper](#hyper)
- Triggers: [Hyper + key](#hyper--key) · [Hyper tap](#hyper-tap) · [Modifier tap](#modifier-tap)
- Building blocks: [Modifiers](#modifiers) · [Keys](#keys) · [Shortcuts](#shortcuts)
- Actions: [app](#app) · [desktop](#desktop) · [space back](#space-back) ·
  [space cycle](#space-cycle) · [tab](#tab) · [window](#window) · [key](#key) · [exec](#exec)
- Settings: [set tab](#set-tab) · [set tap-ms](#set-tap-ms) · [set app-window-mod](#set-app-window-mod)
- [Command line](#command-line)

### Config file

One binding or setting per line. Lines starting with `#` and empty lines are ignored.

```
<trigger> : <action>
set <setting> <value>
# comment
```

- `<trigger>` says which keys fire the line, `<action>` what happens
- every trigger can be bound only once, a second line fails with `already bound`
- order of lines doesn't matter
- reload after editing with `pkill -HUP phlash`; if the new file has errors
  phlash reports them and keeps the previous config

```
# jump to Safari with caps lock + s
hyper - s : app Safari
set tap-ms 250
```

Check the config without reloading:

```
phlash --check
```

### Hyper

Caps lock becomes Hyper, an extra modifier no app uses.

While phlash runs, caps lock is remapped to F18 and phlash treats F18 as Hyper.
Caps lock itself no longer toggles capital letters. The mapping is removed
when phlash stops.

See [Requirements](#requirements) for the caps lock settings.

### Hyper + key

Hold caps lock and press a key.

```
hyper [+ <mod> ...] - <key> : <action>
```

| Part | Meaning |
|---|---|
| `hyper` | caps lock held |
| `+ <mod>` | optional [modifiers](#modifiers) held as well |
| `- <key>` | the [key](#keys) pressed |

- every key pressed with Hyper is swallowed, bound or not, so it never types
  into the app
- holding the key down doesn't repeat the action

```
hyper - s : app Safari
hyper + shift - s : app Mail
hyper - - : space back          # the minus key
```

### Hyper tap

Tap caps lock on its own, optionally while holding modifiers.

```
[<mod> + ...] hyper : <action>
```

| Part | Meaning |
|---|---|
| `<mod> +` | optional [modifiers](#modifiers) held while tapping |
| `hyper` | caps lock pressed and released |

- counts as a tap when released within [tap-ms](#set-tap-ms) and no other key
  was pressed in between (modifiers don't count)
- modifiers may be written before or after `hyper`
- each combination of modifiers is a separate trigger

```
hyper : space back
cmd + hyper : space cycle
cmd + shift + hyper : space cycle prev
```

### Modifier tap

Tap a single modifier key on its own.

```
<modifier> : <action>
```

| Modifier | Keys |
|---|---|
| `cmd` `opt` `ctrl` `shift` | either side |
| `lcmd` `lopt` `lctrl` `lshift` | left side only |
| `rcmd` `ropt` `rctrl` `rshift` | right side only |
| `fn` | fn / globe |

- counts as a tap when released within [tap-ms](#set-tap-ms), with no other key,
  modifier or mouse click in between
- the modifier still reaches the app, so normal shortcuts keep working
- a one-side line wins over an either-side line for the same key

```
shift : tab next
rcmd : app Ghostty
```

### Modifiers

Modifiers held together with Hyper or inside a [shortcut](#shortcuts).

```
cmd | opt | ctrl | shift
```

- combine with `+`, in any order
- one-side names (`lcmd`, …) only exist for [modifier taps](#modifier-tap)

```
hyper + cmd + shift - t : …
opt + hyper : …
```

### Keys

The key part of a trigger or shortcut.

| Form | Example | Meaning |
|---|---|---|
| character | `y` `ö` `#` `1` | what the key types in your keyboard layout, lowercase |
| name | `return` `tab` `space` `delete` `forwarddelete` `escape` | fixed name |
| | `left` `right` `up` `down` `home` `end` `pageup` `pagedown` | |
| | `f1` … `f20` | |
| hex keycode | `0x0a` | macOS virtual keycode, for keys without a name |

- characters follow the active layout: on a German keyboard `y` is the key
  left of `x`
- the layout is read at start and on every reload

```
hyper - ö : app Finder
hyper - 0x0a : desktop
```

### Shortcuts

A key with modifiers, used by [key](#key) and [set tab](#set-tab).

```
[<mod>+...]<key>
```

- modifiers and key joined with `+`, no spaces needed
- `cmd++` is cmd and the plus key

```
cmd+t
ctrl+shift+tab
ctrl+right
```

### app

Jump to an app's window instantly, wherever its space is.

```
app <name> [| <name> ...]
```

| Part | Meaning |
|---|---|
| `<name>` | app name as `phlash --list` shows it, or the `.app` name, any case |
| `\| <name>` | fallbacks in order of preference |

- picks the first app that has a window on a space; minimized windows don't count
- prefers a window on the current space, else the frontmost one
- never launches apps: nothing happens if none has a window
- pressing the key again while the app is in front sends its next-tab
  shortcut, see [set tab](#set-tab)

```
hyper - s : app Safari
hyper - d : app WebStorm | PhpStorm | Visual Studio Code
hyper - c : app Code              # owner name of Visual Studio Code
```

### desktop

Jump to the first space without a fullscreen app.

```
desktop
```

- only the current display is searched

```
hyper - a : desktop
```

### space back

Jump to the last used space.

```
space back
```

- also remembers space changes made with the trackpad, Mission Control or by
  clicking an app
- if that space is gone (fullscreen app closed), nothing happens

```
hyper : space back
```

### space cycle

Step through the spaces of the current display.

```
space cycle [prev]
```

- `prev` goes backwards
- wraps around at both ends

```
cmd + hyper : space cycle
cmd + shift + hyper : space cycle prev
```

### tab

Next or previous tab of the app in front.

```
tab next | tab prev
```

- uses the app's tab shortcut from [set tab](#set-tab)
- `tab prev` reverses it automatically: arrows flip, otherwise shift is toggled
- the shortcut's modifiers stay held so tab switchers (Firefox, Zen) stay open:
  - on a [Hyper tap](#hyper-tap) line until you let go of its modifiers (shift aside)
  - on a [Hyper + key](#hyper--key) line until you let go of Hyper
  - on a [modifier tap](#modifier-tap) line not at all

```
opt + hyper : tab next
opt + shift + hyper : tab prev
shift : tab next
```

### window

Next or previous window of the app in front, across spaces.

```
window next | window prev
```

- order: by space from left to right, then by window
- wraps around
- for all windows of an app line see [set app-window-mod](#set-app-window-mod)

```
ctrl + hyper : window next
ctrl + shift + hyper : window prev
```

### key

Send a keyboard shortcut to the app in front.

```
key <shortcut>
```

- `<shortcut>` see [Shortcuts](#shortcuts)

```
hyper - tab : key cmd+t
hyper - w : key cmd+w
```

### exec

Run a shell command.

```
exec <command>
```

- runs with `/bin/sh -c`, phlash doesn't wait for it
- Homebrew's `bin` directories are on the `PATH`

```
hyper - l : exec pmset displaysleepnow
hyper - o : exec open ~/Downloads
```

### set tab

The next-tab shortcut used by [tab](#tab) and by pressing an [app](#app) key again.

```
set tab <shortcut>
set tab <shortcut> for <name> [| <name> ...]
set tab none for <name> [| <name> ...]
```

| Form | Meaning |
|---|---|
| `set tab <shortcut>` | default for all apps, `ctrl+tab` if not set |
| `… for <names>` | shortcut for these apps |
| `none for <names>` | no tab switching in these apps |

- each app can appear in only one `for` list
- names as in [app](#app); apps don't need an app line

```
set tab ctrl+tab
set tab ctrl+right for WebStorm | PhpStorm | CLion
set tab ctrl+pagedown for Visual Studio Code
set tab none for Finder
```

### set tap-ms

How quickly a [Hyper tap](#hyper-tap) or [modifier tap](#modifier-tap) must be released.

```
set tap-ms <50-1000>
```

- in milliseconds, default `300`
- lower values prevent accidental taps, higher ones make tapping easier

```
set tap-ms 200
```

### set app-window-mod

Hyper + these modifiers + an app key cycles all windows of that app line.

```
set app-window-mod <mod>[+<mod>...] | none
```

- off by default
- order: apps in line order, then by space, then by window; wraps around
- if none of the line's apps is in front, it jumps like the app key itself
- only for [app](#app) lines, and only if the combination isn't bound itself
- forward only

```
set app-window-mod cmd
hyper - d : app WebStorm | PhpStorm
# hyper + cmd - d: all WebStorm windows, then all PhpStorm windows
```

### Command line

```
phlash [--check] [--list] [--debug] [--help]
```

`scripts/install.sh` puts phlash at `~/.local/bin/phlash`; add `~/.local/bin` to
your `PATH` to call it as `phlash`.

| Flag | Meaning |
|---|---|
| `--check` | check the config, print it as understood and exit |
| `--list` | list spaces (`*` = current) and the apps on them |
| `--debug` | log every key event and hotkey (stop the running copy first) |
| `--help` | show usage |

```
phlash --list
```
```
display 1
   1   desktop     Spotify
   2   fullscreen  Ghostty
   3 * fullscreen  Zen
```

## Usage

| Task | Command |
|---|---|
| Reload config | `pkill -HUP phlash` |
| Check config | `~/.local/bin/phlash --check` |
| Restart | `launchctl kickstart -k gui/$(id -u)/local.phlash` |
| Log | `tail -f ~/Library/Logs/phlash.log` |
| Update | `git pull && scripts/install.sh` |
| Uninstall | `scripts/uninstall.sh` |

## Troubleshooting

**Shortcuts stopped working.** Check whether caps lock is still mapped:

```
hidutil property --get UserKeyMapping
```

If it prints `()`, the mapping was lost. Restart phlash (see [Usage](#usage)).

**`already running`.** Only one phlash can run. To use `--debug`, stop the
background copy first:

```
launchctl bootout gui/$(id -u)/local.phlash
```

Start it again with `scripts/install.sh`.

**An app key does nothing.** The app has no open window, or its name doesn't
match. `phlash --list` shows the names phlash sees.

**Spaces change order.** Turn off **Automatically rearrange Spaces based on most
recent use** (see [Requirements](#requirements)).

**Caps lock does nothing at all.** Check the Accessibility permission and the
Modifier Keys setting from [Requirements](#requirements), then look at the log.

**Accessibility is asked again after every install.** The signing certificate
is missing. `scripts/install.sh` creates it; allow `codesign` to use the key.

## License

MIT, see [LICENSE](LICENSE).
