# InstantSpaceSwitcher (vendored)

The space-switching core of [InstantSpaceSwitcher](https://github.com/jurplel/InstantSpaceSwitcher)
by jurplel, MIT licensed (see [LICENSE](LICENSE)). phlash calls it in-process
instead of launching `ISSCli`, which cost 90–150 ms of startup plus a 300 ms wait
per switch.

**Do not edit these files.** They are upstream's, unmodified, so an update is a
plain copy and anything local lives in phlash's own code (`src/switch.c`).

| | |
|---|---|
| Upstream commit | `f7616e42651b1ed8d81625e3640eceed6efa8018` (2026-10-05, "update version + build on xcode 27") |
| Files | `Sources/ISS/ISS.c`, `event_serialize.c`, `event_serialize.h`, `include/ISS.h`, `LICENSE` |

phlash uses only `iss_switch_to_index()`. It doesn't call `iss_init()`, which
would add ISS's own event tap for its trackpad-swipe override.

## Updating

When a macOS update breaks switching (keys are caught, nothing switches),
upstream usually fixes it first:

```bash
cd third_party/InstantSpaceSwitcher
SHA=<new commit>
for f in Sources/ISS/ISS.c Sources/ISS/event_serialize.c Sources/ISS/event_serialize.h LICENSE; do
    curl -fsSL "https://raw.githubusercontent.com/jurplel/InstantSpaceSwitcher/$SHA/$f" -o "$(basename "$f")"
done
curl -fsSL "https://raw.githubusercontent.com/jurplel/InstantSpaceSwitcher/$SHA/Sources/ISS/include/ISS.h" -o include/ISS.h
```

Then update the commit above, check that `iss_switch_to_index()` still has the
same signature and meaning (0-based index on the display under the pointer),
and rebuild.
