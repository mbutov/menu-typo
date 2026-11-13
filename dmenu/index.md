typo
====

Description
-----------
Typo-tolerant matching: when the input is a single word, items that
contain it with up to one or two errors are listed after the exact, prefix
and substring matches, fewest errors first.

An error is one inserted, deleted or substituted character, or two
adjacent characters swapped (restricted Damerau-Levenshtein distance), so
`frefox`, `firefxo` and `fierfox` all find `firefox`. The input is compared
with any part of an item, case insensitively and character by character in
UTF-8.

Unlike [fuzzymatch](../fuzzymatch/), which forgives only missing letters,
this patch also forgives extra, wrong and swapped letters, and keeps the
usual matching order for exact matches.

Configuration, in config.h: `typo_min_len_1` and `typo_min_len_2`, the
input length (in characters) from which 1 and 2 errors are allowed (5 and
8; 0 disables a level).

Download
--------
* [dmenu-typo-5.4.diff](dmenu-typo-5.4.diff)

Authors
-------
* Maxim Butov
