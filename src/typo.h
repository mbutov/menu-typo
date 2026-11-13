/*
 * typo.h - typo-tolerant matching for application menus.
 *
 * typo_distance() returns the restricted Damerau-Levenshtein distance
 * (optimal string alignment) between a pattern and the best-matching
 * substring of a text. The start and the end of the substring are free
 * (Sellers' algorithm), so "frefox" matches "firefox" with one error and
 * "vscod" matches "code-vscode" with none. An error is one inserted,
 * deleted or substituted character, or two adjacent characters swapped.
 *
 * Both strings are UTF-8 and are compared character by character, case
 * insensitively (towlower(), so case folding beyond ASCII needs a UTF-8
 * LC_CTYPE locale, which dmenu and rofi set up). Invalid bytes compare as
 * themselves.
 *
 * typo_max_errors() is how many errors to allow for a pattern of a given
 * length: with a short pattern, even one error matches almost anything.
 *
 * Single header, C99, no dependencies. Copyright (c) 2026 Maxim Butov,
 * MIT license (see LICENSE).
 */
#ifndef TYPO_H
#define TYPO_H

#include <stddef.h>
#include <stdint.h>
#include <wctype.h>

/* Longest pattern, in characters; longer ones never match with errors. */
#define TYPO_PATTERN_MAX 64

/* Default pattern lengths from which 1 and 2 errors are allowed. */
#define TYPO_MIN_LEN_1 5
#define TYPO_MIN_LEN_2 8

/* Decodes one UTF-8 character at *s, advances *s, returns it lowercased. */
static inline uint32_t
typo_next_char(const char **s)
{
	const unsigned char *p = (const unsigned char *)*s;
	uint32_t c = p[0];
	int n = 0, i;

	if (c >= 0xf0 && c < 0xf8)
		n = 3, c &= 0x07;
	else if (c >= 0xe0)
		n = 2, c &= 0x0f;
	else if (c >= 0xc0)
		n = 1, c &= 0x1f;
	for (i = 1; i <= n; i++) {
		if ((p[i] & 0xc0) != 0x80) { /* broken sequence: take the byte */
			c = p[0];
			n = 0;
			break;
		}
		c = (c << 6) | (p[i] & 0x3f);
	}
	*s += n + 1;
	return (uint32_t)towlower((wint_t)c);
}

/* Number of characters in a UTF-8 string. */
static inline size_t
typo_strlen(const char *s)
{
	size_t n = 0;

	while (*s) {
		typo_next_char(&s);
		n++;
	}
	return n;
}

/* Errors allowed for a pattern of len characters (0: exact matches only). */
static inline int
typo_max_errors(size_t len, unsigned int min_len_1, unsigned int min_len_2)
{
	return len >= min_len_2 ? 2 : len >= min_len_1 ? 1 : 0;
}

/*
 * Distance from pattern to the closest substring of text; if it is greater
 * than max, returns max + 1.
 */
static inline int
typo_distance(const char *pattern, const char *text, int max)
{
	uint32_t pat[TYPO_PATTERN_MAX];
	int prev2[TYPO_PATTERN_MAX + 1], prev[TYPO_PATTERN_MAX + 1];
	int cur[TYPO_PATTERN_MAX + 1];
	int m = 0, i, j, best, v;
	uint32_t tc, tprev = 0;

	while (*pattern) {
		if (m == TYPO_PATTERN_MAX)
			return max + 1;
		pat[m++] = typo_next_char(&pattern);
	}
	if (m == 0)
		return 0;
	for (i = 0; i <= m; i++)
		prev[i] = prev2[i] = i;
	best = m;
	for (j = 0; *text; j++) {
		tc = typo_next_char(&text);
		cur[0] = 0; /* the substring may start anywhere */
		for (i = 1; i <= m; i++) {
			v = prev[i - 1] + (pat[i - 1] != tc);              /* substitute */
			if (prev[i] + 1 < v)
				v = prev[i] + 1;                           /* skip in text */
			if (cur[i - 1] + 1 < v)
				v = cur[i - 1] + 1;                        /* skip in pattern */
			if (i > 1 && j > 0 && pat[i - 1] == tprev &&
			    pat[i - 2] == tc && prev2[i - 2] + 1 < v)
				v = prev2[i - 2] + 1;                      /* transpose */
			cur[i] = v;
		}
		if (cur[m] < best)
			best = cur[m]; /* ... and end anywhere */
		for (i = 0; i <= m; i++) {
			prev2[i] = prev[i];
			prev[i] = cur[i];
		}
		tprev = tc;
	}
	return best > max ? max + 1 : best;
}

#endif /* TYPO_H */
