# Space Navigation App – Plan

## Requirements

* The app is custom built in C and integrates the InstantSpaceSwitcher space switching code directly into the app (in a separate directory to make it more maintainable if something breaks and clearer what is my code and what isn't) to avoid the startup time of the ISSCli when switching spaces
* Other apps like Hyperkey are not used/included.
* `Caps Lock` as `Hyper` key (uses `F18` on `Caps Lock` as a `Hyper` modifier) that uses shortcuts to navigate to specific apps
* App shortcuts and navigation shortcuts are set in the config (for example `Hyper + Q` for Zen Browser or Taping `Hyper` to switch to last used app and its last used window)
* If the app is not open the shortcut opens it in fullscreen
* One shortcut targets a desktop with no fullscreen app, here apps that don't have a specific shortcut launch in window mode
* Mission Control Keyboard Button and maybe Mission Control on Hot Corners can be used as navigation fallback (just a side note, not implemented in the App)

## Recommended Shortcuts

### App Shortcuts

* 3-4-3 grid (`Hyper + Q`, `W`, `E`, `A`, `S`, `D`, `F`, `Y`, `X`, `C`) with most used apps on `A`, `S`, `D`, `F`

| Row | Keys |
| --- | --- |
| Top | `Hyper + Q` · `Hyper + W` · `Hyper + E` |
| Home | `Hyper + A` · `Hyper + S` · `Hyper + D` · `Hyper + F` |
| Bottom | `Hyper + Y` · `Hyper + X` · `Hyper + C` |

### Navigation Shortcuts

#### Option 1

| Action | Shortcut |
| --- | --- |
| Switching between last two used apps (focusing their last used window) | Taping `Hyper` |
| Cycling through all spaces one space at a time | Taping `Cmd + Tab` |
| Cycling through Tabs | Taping `Control` or Taping `Control + Tab` or Taping `Cmd + Hyper`|
| Cycling through multiple Windows of the same app | Taping `Hyper + App Key` when App is already in focus |
| Switching to specific Tabs | `Cmd + numbers` or `Hyper + numbers` |


#### Option 2

| Action | Shortcut |
| --- | --- |
| Switching between last two used apps (focusing their last used window) | Taping `Hyper` |
| Cycling through all spaces one space at a time | Taping `Cmd + Tab` |
| Cycling through Tabs | Taping `Hyper + App Key` when the app is already in focus (Taping `Ctrl` or Taping `Ctrl + Tab` also works for all Apps) |
| Cycling through multiple Windows of the same app | Taping `Hyper + Cmd + App Key` |

## Future Improvements

* Support for multi screen setups
* Apps that cant get a fullscreen space (like Calculator) open in the desktop with no fullscreen apps when their shortcut is used and are focusable with their shortcut
* A future option is to implement a new shortcut that opens a search bar to search for an open app to move to it and focus it. This way the animation can be instant unlike with the Spotlight solution. Another option is to set `Hyper + 1`, `2`, `3`, `4` as assignable shortcuts with `Hyper + Cmd + 1`, `2`, `3`, `4` that bind a app to that shortcut until its closed or overwritten.
* Later a setup process can be implemented that opens and sets up one predefined arrangement of apps and spaces (maybe more different arrangements for different use cases, like coding, personal stuff, etc)
