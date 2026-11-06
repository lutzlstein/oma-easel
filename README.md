# easel

Generates new background images for an installed [Omarchy](https://omarchy.org) theme, in the style of its existing backgrounds and colors, sized for your screens.

New backgrounds go into `~/.config/omarchy/backgrounds/<theme>/`, where Omarchy picks them up for that theme's background cycle. Themes themselves are never modified.

> Working name. Phase 1: the commands work; the window comes next.

## How it works

1. Load a theme: its `colors.toml` and every background it has.
2. Pick existing backgrounds as style references and describe a scene.
3. easel builds the full prompt (references, theme palette, your screens' shape) and generates 1–6 candidates in parallel.
4. Keep the ones you like. A kept candidate becomes a lossless master in `~/.local/share/easel/<theme>/` with a JSON sidecar describing how it was made, and a WebP export (Lanczos-scaled, quality 85) in the theme's user backgrounds.

Candidates wait in `~/.cache/easel/<theme>/<batch>/` until kept.

## Backends

- **Codex** (default): runs `codex exec` with Codex's built-in image tool on your ChatGPT login. No API key needed.
- **OpenAI API** with `gpt-image-2`: planned.
- **Manual** (fallback): when no generator is available, easel prints the same prompt a backend would send, lists the reference images to attach, and gives short steps for any image platform you can use (ChatGPT, Gemini, Copilot…). Bring the result back with `easel import`, then keep it like any other candidate.

`easel backends` shows what this machine has.

## Commands

```
easel themes                         List installed themes (* marks the current one)
easel theme [theme]                  Show a theme's colors and backgrounds
easel screens                        Show monitors and the export size
easel backends                       Show which ways of making images are available
easel generate [theme] --prompt "…" [--ref …] [-n 3]
easel prompt [theme] --prompt "…" [--ref …]
                                     Print a prompt to paste into another image platform
easel import <file>… [--theme …] [--prompt "…"] [--source "…"]
                                     Add images made elsewhere as candidates
easel keep <candidate> [--name "…"]
```

Every command takes `--json` and exits 0 on success or 1 with the reason on stderr. `--ref` accepts a file path, a background file name, or its number in `easel theme`.

```sh
easel generate kanagawa --prompt "a quiet mountain lake at dusk with a lone boat" --ref 1 -n 3
easel keep ~/.cache/easel/kanagawa/<batch>/candidate-2.png
```

## Screens

The export size comes from `hyprctl monitors`: the widest landscape monitor's shape, large enough to fill every landscape monitor. Portrait monitors show a center crop.

## Build

```sh
bin/build      # build/easel
bin/test       # Qt Test, offscreen
```

Needs `qt6-base`, `imagemagick`, and `codex`.
