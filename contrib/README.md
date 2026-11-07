# Per-screen backgrounds for Omarchy

Omarchy's background renderer (`omarchy.background`) shows one image on every screen. `per-screen-backgrounds.patch` teaches it to show each screen the version of the background closest to its own shape:

- A version is a file of the same name in a folder named for the shape, beside the background in the theme's or the user's backgrounds: `~/.config/omarchy/backgrounds/<theme>/9x16/7-add-boat.webp`. Omarchy's background cycle reads only the top folder, so versions never show up in it as backgrounds of their own.
- A screen uses a version only when it fits that screen's shape better than the background itself. With no versions, nothing changes.
- A version that fails to load is dropped, and its screen falls back to the background.

It also adds four IPC functions to the `background` target:

| Call | Does |
|---|---|
| `omarchy-shell background rescanVersions` | Looks for new versions, after one is saved |
| `omarchy-shell background setScreen <screen> <path>` | Shows an image on one screen only, for previews |
| `omarchy-shell background clearScreens` | Ends those previews |
| `omarchy-shell background shown` | Prints what each screen shows, as `name<TAB>path` lines |

Oma Easel uses `shown` to tell whether the renderer supports versions, and falls back to previewing on every screen when it doesn't.

## Using it before it's in Omarchy

```sh
omarchy plugin clone omarchy.background
cd ~/.config/omarchy/plugins/$USER.background
patch -p4 < /path/to/oma-easel/contrib/per-screen-backgrounds.patch
omarchy restart shell
```

The clone replaces the built-in renderer and no longer receives Omarchy's fixes to it. To go back, run `omarchy plugin enable omarchy.background` and remove the clone with `omarchy plugin remove $USER.background`.

The patch is against Omarchy's `shell/plugins/background/Background.qml` and is meant to be proposed upstream.
