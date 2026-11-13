# typo

## Description

Typo-tolerant matching for dmenu.

Every word of the input also matches ignoring diacritics, so `cafe` finds
`café` and `елка` finds `ёлка`. When the input is a single word, items
that contain it with up to one or two errors are listed after the exact,
prefix and substring matches, fewest errors first.

An error is an inserted, deleted or substituted character, or two adjacent
characters swapped (restricted Damerau-Levenshtein distance), so `frefox`,
`firefxo` and `fierfox` all find `firefox`. The input is compared with any
part of an item, character by character in UTF-8.

## Notes

- One error is allowed from 5 input characters, two from 8: set
  `typo_min_len_1` and `typo_min_len_2` in `config.h` (0 disables).
- Case follows dmenu's `-i` switch; diacritics are always ignored.
- Unlike [fuzzymatch](../fuzzymatch/), which forgives only missing letters,
  this patch also forgives extra, wrong and swapped letters, and keeps the
  usual order of exact matches.
- The patch adds `typo.h`, a single-header implementation that can be
  reused elsewhere. Upstream, with rofi modes using the same matching:
  <https://github.com/mbutov/menu-typo>

## Download

- [dmenu-typo-5.4.diff](dmenu-typo-5.4.diff)

## Authors

- Maxim Butov - <mbutov@gmail.com>
