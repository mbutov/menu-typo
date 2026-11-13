/*
 * Shared part of the menu-typo rofi modes: remembers the raw input and
 * decides whether an entry matches it, exactly (rofi's own token matching)
 * or with typos (src/typo.h).
 *
 * Options, read from the rofi command line:
 *   -typo-min-len-1 N   input length from which 1 error is allowed (5)
 *   -typo-min-len-2 N   input length from which 2 errors are allowed (8)
 * 0 disables the respective level.
 *
 * Copyright (c) 2026 Maxim Butov, MIT license (see LICENSE).
 */
#ifndef MENU_TYPO_COMMON_H
#define MENU_TYPO_COMMON_H

#include <glib.h>
#include <rofi/helper.h>
#include <string.h>

#include "../src/typo.h"

typedef struct {
	char *input;            /* trimmed input, from _preprocess_input */
	int max_errors;         /* for the current input */
	unsigned int min_len_1;
	unsigned int min_len_2;
} TypoMatcher;

static inline void
typo_matcher_init(TypoMatcher *m)
{
	m->input = NULL;
	m->max_errors = 0;
	m->min_len_1 = TYPO_MIN_LEN_1;
	m->min_len_2 = TYPO_MIN_LEN_2;
	find_arg_uint("-typo-min-len-1", &m->min_len_1);
	find_arg_uint("-typo-min-len-2", &m->min_len_2);
}

static inline void
typo_matcher_set_input(TypoMatcher *m, const char *input)
{
	g_free(m->input);
	m->input = g_strstrip(g_strdup(input ? input : ""));
	/* typos only for a single word */
	if (!m->min_len_1 || strchr(m->input, ' '))
		m->max_errors = 0;
	else
		m->max_errors = typo_max_errors(typo_strlen(m->input), m->min_len_1,
		                                m->min_len_2 ? m->min_len_2 : G_MAXUINT);
}

static inline gboolean
typo_matcher_match(const TypoMatcher *m, rofi_int_matcher **tokens, const char *text)
{
	if (helper_token_match(tokens, text))
		return TRUE;
	return m->max_errors > 0 &&
	       typo_distance(m->input, text, m->max_errors) <= m->max_errors;
}

static inline void
typo_matcher_free(TypoMatcher *m)
{
	g_free(m->input);
	m->input = NULL;
}

#endif /* MENU_TYPO_COMMON_H */
