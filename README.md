# menu-typo

Typo-tolerant matching for [dmenu](https://tools.suckless.org/dmenu/) and
[rofi](https://github.com/davatorium/rofi): while you type, the menu also
lists the entries that match your input with a typo or two.

```
frefox     -> firefox        (missing letter)
firefxo    -> firefox        (two letters swapped)
alacrity   -> alacritty
thndrbird  -> thunderbird    (two errors)
тлеграм    -> Телеграм       (UTF-8: one error per character)
```

Exact matches always come first, as before; matches with typos follow,
fewest errors first. Nothing changes until the input is long enough for a
typo to be meaningful.

It comes as:

- a **patch for dmenu** 5.4;
- **`typorun`**, a rofi mode that runs programs, like the built-in `run`;
- **`typowindow`**, a rofi mode that switches windows of i3 or sway, like
  the built-in `window`;
- **`src/typo.h`**, the matching itself: a single C99 header with no
  dependencies, if you want it in another menu.

## How matching works

An *error* is one inserted, deleted or substituted character, or two
adjacent characters swapped (restricted Damerau-Levenshtein distance). The
input is compared with any part of an entry, so `fox` and `frefox` both
find `firefox`. Case does not matter, and UTF-8 text is compared character
by character, so Cyrillic, Greek etc. work as well as ASCII.

Typos are allowed only when the input is a single word:

| Input length (characters) | Errors allowed |
|---|---|
| 1-4 | 0 (exact matches only) |
| 5-7 | 1 |
| 8 and more | 2 |

With a shorter input, even one error matches almost anything. Both limits
are configurable.

## dmenu

Build a patched dmenu from this repository:

```sh
make dmenu        # downloads dmenu 5.4, checks its sha256, patches, builds
sudo make install # installs it as dmenu-typo, next to your dmenu
dmenu_path | dmenu-typo | ${SHELL:-/bin/sh} &
```

Or apply the patch to your own dmenu tree:

```sh
make dmenu-patch  # writes dmenu-typo-5.4.diff
cd ~/src/dmenu && patch -p1 < /path/to/dmenu-typo-5.4.diff
```

The patch adds `typo.h` and two settings to `config.def.h` (copy them to
your `config.h`):

```c
static unsigned int typo_min_len_1 = 5; /* 1 error from this input length */
static unsigned int typo_min_len_2 = 8; /* 2 errors; 0 disables a level */
```

Matches with typos are appended after the exact, prefix and substring
matches, ordered by the number of errors.

## rofi

Requirements: rofi 2.0 (plugin ABI 7) with its headers, glib, json-glib.
On Arch Linux: `pacman -S --needed rofi json-glib base-devel`.

```sh
make
sudo make install   # into rofi's plugin directory
```

Without installing, point rofi at the build directory with
`-plugin-path build`.

### typorun

```sh
rofi -modi typorun -show typorun -sort -sorting-method fzf
```

- Entries: programs from `dmenu_path` if it is installed, otherwise a scan
  of `$PATH`; recently run programs first.
- History: the one of the built-in `run` mode (`~/.cache/rofi-4.runcache`),
  so switching between the two modes keeps it. Shift+Delete removes an
  entry from the history.
- Shift+Enter (`kb-accept-alt`) runs the program in a terminal.

Options:

| Option | Default | Meaning |
|---|---|---|
| `-typorun-command TEMPLATE` | `{cmd}` | how to run the choice; `{cmd}` is replaced by the shell-quoted command line, e.g. `-typorun-command "my-wrapper {cmd}"` |
| `-typorun-history-size N` | 25 | history entries to keep |

### typowindow

```sh
rofi -modi typowindow -show typowindow -sort -sorting-method fzf
```

- Entries: `workspace  class  title` of every window, most recently focused
  first; on sway, `app_id` stands in for the class of Wayland windows.
- Enter focuses the window, Shift+Delete closes it.
- The IPC socket is taken from `$SWAYSOCK`, `$I3SOCK` or
  `i3 --get-socketpath`.

### Options of both modes

| Option | Default | Meaning |
|---|---|---|
| `-typo-min-len-1 N` | 5 | input length from which 1 error is allowed; 0 disables typos |
| `-typo-min-len-2 N` | 8 | input length from which 2 errors are allowed; 0 disables this level |

### Sorting

A rofi mode decides only *whether* an entry matches; the order of the list
is rofi's. Use `-sort -sorting-method fzf`: it puts entries where the input
letters come in a row on top, so exact matches stay above the ones with
typos. With `levenshtein` sorting, short names with typos may come before
longer exact matches.

### Example: i3 key bindings

```
bindsym $mod+d exec --no-startup-id rofi -modi typorun -show typorun -sort -sorting-method fzf
bindsym $mod+x exec --no-startup-id rofi -modi typowindow -show typowindow -sort -sorting-method fzf
```

## Development

```sh
make test         # unit tests of src/typo.h
make              # rofi plugins
make dmenu        # patched dmenu
make dmenu-patch  # the self-contained dmenu patch
make clean
```

The dmenu changes live in `dmenu/dmenu.c.diff` and
`dmenu/config.def.h.diff`; `make dmenu-patch` combines them with
`src/typo.h` into one patch against a clean dmenu.

## License

MIT, see [LICENSE](LICENSE). dmenu and rofi are MIT-licensed as well.
