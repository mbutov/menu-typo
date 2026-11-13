# Changelog

## 0.1.0 - unreleased

- `src/typo.h`: restricted Damerau-Levenshtein distance from a pattern to
  the closest substring of a text, UTF-8 aware, ignoring case and
  diacritics (`ё` as `е`, `é` as `e`); folding table generated from the
  Unicode database by `tools/gen-fold.py`.
- dmenu 5.4 patch: matches with typos are listed after the exact ones,
  fewest errors first; thresholds in `config.def.h`.
- rofi mode `typorun`: run a program, with typo-tolerant matching and the
  run history shared with the built-in `run` mode.
- rofi mode `typowindow`: switch windows of i3 or sway, with typo-tolerant
  matching.
- Separate AUR packages: `rofi-typo-git` (rofi modes) and `dmenu-typo-git`
  (`dmenu-typo` next to dmenu); `make install-rofi`, `make install-dmenu`.
