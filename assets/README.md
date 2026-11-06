# Logo

`logo-source.png` was generated with Codex's built-in image tool from the prompt:

> logo for Easel, an omarchy background generator

with a request for a square image on a transparent background.

The app uses only the mark, split in two so it follows the theme:

- `src/assets/logo-outline.png`: the dark outline (pixels under 12% luminance), as a white mask the window paints in the theme's text color.
- `src/assets/logo-color.png`: the painted canvas, in its own colors.

The word "Easel" from the source isn't used; the window shows the app's name as text, so renaming the app doesn't need a new image.
