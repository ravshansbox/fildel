# fildel

Interactive file viewer with filtering and line deletion.

## Usage

```bash
fildel <filename>
```

## Controls

| Key | Action |
|-----|--------|
| ↑/↓ or k/j | Navigate lines |
| PgUp/PgDown | Fast scroll |
| Home/End | Jump to start/end |
| Space | Select/deselect line |
| d | Delete selected lines (or current line if none selected) |
| w | Save file |
| q | Quit (prompts to save if modified) |
| Type any text | Filter lines |
| Backspace | Remove filter character |
| Delete/Esc | Clear filter |

## Building

```bash
make
```

## Install

```bash
sudo make install
```

Or copy `fildel` to your `$PATH`.
