# Oma Easel

Generates new background images for an installed [Omarchy](https://omarchy.org) theme, in the style of its existing backgrounds and colors, sized for your screens.

New backgrounds go into `~/.config/omarchy/backgrounds/<theme>/`, where Omarchy picks them up for that theme's background cycle. Themes themselves are never modified.

> Early days.

## How it works

1. Load a theme: its `colors.toml` and every background it has.
2. Pick existing backgrounds as style references and describe a scene.
3. easel builds the full prompt (references, theme palette, your screens' shape) and generates 1–6 candidates in parallel.
4. Keep the ones you like. A kept candidate becomes a lossless master in `~/.local/share/oma-easel/<theme>/` with a JSON sidecar describing how it was made, and a WebP export (Lanczos-scaled, quality 85) in the theme's user backgrounds.

Candidates wait in `~/.cache/oma-easel/<theme>/<batch>/` until kept.

## Backends

- **Codex** (default): runs `codex exec` with Codex's built-in image tool on your ChatGPT login. No API key needed.
- **OpenAI API** (`gpt-image-2` by default) with your API key: native 3440x1440 output.
- **Gemini API** (`gemini-3.1-flash-image-preview` by default) with your API key.
- **Manual** (fallback): when no generator is available, easel prints the same prompt a backend would send, lists the reference images to attach, and gives short steps for any image platform you can use (ChatGPT, Gemini, Copilot…). Bring the result back with `oma-easel import`, then keep it like any other candidate.

`oma-easel backends` shows what this machine has. In the window, **models & export** (`s`) switches between them, edits model ids, and saves API keys to the desktop keyring through `secret-tool` (an `OPENAI_API_KEY` or `GEMINI_API_KEY` environment variable wins). The same menu sets the export format (WebP, JPEG, PNG), quality, and an optional folder that gets a copy of every kept background for the theme, such as a theme repo's `backgrounds/`.

## Window

`oma-easel` (or `oma-easel open [theme]`) opens on the current theme. Pick references, type a scene, and press Enter.

| Key | Action |
|---|---|
| Space | Generate a batch (or make the prompt in manual mode) |
| Enter | Keep the selected candidate |
| D | Discard the selected candidate |
| V | Vary: say what to change, or Esc for fresh takes in the same style |
| R | Use the selected candidate as a reference |
| P | Preview on an empty workspace, with a prompt to keep it or go back |
| H | History: every earlier batch for the theme, with its prompt |
| ← → ↑ ↓ | Move between candidates |
| / | Type a scene |
| I | Import images (or drop them onto the window) |
| C | Copy the prompt |
| Esc | Close, cancel, or leave the scene field |
| ? | Show the keys |
| Q | Quit (asks first while images are generating) |

Generation keeps running when you switch themes; the header shows what's running for which theme and for how long, and clicking it goes back to that batch. Candidates stay in the history drawer whether or not you keep them.

Backgrounds Oma Easel added are marked in the reference strip; hover one and click × to remove it from the theme. It and its master go to the trash, and if it's on the desktop, the desktop switches to another of the theme's backgrounds. Backgrounds Oma Easel didn't add can't be removed.

## Commands

```
oma-easel themes                         List installed themes (* marks the current one)
oma-easel theme [theme]                  Show a theme's colors and backgrounds
oma-easel screens                        Show monitors and the export size
oma-easel backends                       Show which ways of making images are available
oma-easel generate [theme] --prompt "…" [--ref …] [-n 3] [--backend codex|openai|gemini]
oma-easel prompt [theme] --prompt "…" [--ref …]
                                     Print a prompt to paste into another image platform
oma-easel import <file>… [--theme …] [--prompt "…"] [--source "…"]
                                     Add images made elsewhere as candidates
oma-easel keep <candidate> [--name "…"]
oma-easel remove <background> [--theme …]
                                     Move a background easel added to the trash
```

Every command takes `--json` and exits 0 on success or 1 with the reason on stderr. `--ref` accepts a file path, a background file name, or its number in `oma-easel theme`.

```sh
oma-oma-easel generate kanagawa --prompt "a quiet mountain lake at dusk with a lone boat" --ref 1 -n 3
oma-easel keep ~/.cache/oma-easel/kanagawa/<batch>/candidate-2.png
```

## Screens

The export size comes from `hyprctl monitors`: the widest landscape monitor's shape, large enough to fill every landscape monitor. Portrait monitors show a center crop.

## Build

```sh
bin/build      # build/oma-easel
bin/test       # Qt Test, offscreen
```

Needs `qt6-base`, `qt6-declarative`, `imagemagick`, and optionally `codex`.

## Support

By [lutzlstein](https://github.com/lutzlstein/oma-easel). If it's useful, [support it on Ko-fi](https://ko-fi.com/lutzlstein).
