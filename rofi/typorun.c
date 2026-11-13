/*
 * rofi mode "typorun": run a program, like the built-in "run" mode, with
 * typo-tolerant matching.
 *
 * Entries are the programs in $PATH (dmenu_path if it is installed, else a
 * scan of $PATH), recently run ones first. The run history is the one of
 * the built-in run mode, $XDG_CACHE_HOME/rofi-4.runcache, so both modes
 * share it. Shift+Delete removes an entry from the history.
 *
 * Options:
 *   -typorun-command TEMPLATE   how to run the choice; {cmd} is replaced by
 *                               the shell-quoted command line ("{cmd}")
 *   -typorun-history-size N     history entries to keep (25)
 * plus -typo-min-len-1 / -typo-min-len-2, see common.h.
 *
 * Copyright (c) 2026 Maxim Butov, MIT license (see LICENSE).
 */
#include <gmodule.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

G_MODULE_EXPORT Mode mode;

typedef struct {
	char *name;
	long count;
} HistoryEntry;

typedef struct {
	char **cmds;           /* history first, then the rest */
	unsigned int n;
	unsigned int nhistory; /* how many of cmds come from the history */
	unsigned int history_size;
	TypoMatcher matcher;
} TyporunData;

/* ---- history, in the format of rofi's run mode ------------------------ */

static char *
history_path(void)
{
	return g_build_filename(g_get_user_cache_dir(), "rofi-4.runcache", NULL);
}

static void
history_entry_free(gpointer p)
{
	HistoryEntry *e = p;

	g_free(e->name);
	g_free(e);
}

static gint
history_cmp(gconstpointer a, gconstpointer b)
{
	const HistoryEntry *x = *(HistoryEntry *const *)a;
	const HistoryEntry *y = *(HistoryEntry *const *)b;

	return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

/* Lines "<count> <name>\x1f<quoted command>", most used first. */
static GPtrArray *
history_load(void)
{
	GPtrArray *h = g_ptr_array_new_with_free_func(history_entry_free);
	char *path = history_path(), *data = NULL, **lines, *end, *sep;
	unsigned int i;

	if (g_file_get_contents(path, &data, NULL, NULL)) {
		lines = g_strsplit(data, "\n", -1);
		for (i = 0; lines[i]; i++) {
			long count = strtol(lines[i], &end, 10);
			HistoryEntry *e;

			if (end == lines[i] || *end != ' ' || !end[1])
				continue;
			if ((sep = strchr(end + 1, '\x1f')))
				*sep = '\0';
			e = g_new(HistoryEntry, 1);
			e->name = g_strdup(end + 1);
			e->count = count;
			g_ptr_array_add(h, e);
		}
		g_strfreev(lines);
	}
	g_ptr_array_sort(h, history_cmp);
	g_free(data);
	g_free(path);
	return h;
}

static void
history_save(GPtrArray *h, unsigned int size)
{
	GString *out = g_string_new(NULL);
	char *path = history_path(), *quoted;
	unsigned int i;

	g_ptr_array_sort(h, history_cmp);
	for (i = 0; i < h->len && i < size; i++) {
		HistoryEntry *e = g_ptr_array_index(h, i);

		quoted = g_shell_quote(e->name);
		g_string_append_printf(out, "%ld %s\x1f%s\n", e->count, e->name, quoted);
		g_free(quoted);
	}
	g_file_set_contents(path, out->str, out->len, NULL);
	g_string_free(out, TRUE);
	g_free(path);
}

/* Counts one more run of name, or removes it. */
static void
history_update(const char *name, gboolean remove, unsigned int size)
{
	GPtrArray *h = history_load();
	unsigned int i;

	for (i = 0; i < h->len; i++) {
		HistoryEntry *e = g_ptr_array_index(h, i);

		if (strcmp(e->name, name) == 0) {
			if (remove)
				g_ptr_array_remove_index(h, i);
			else
				e->count++;
			break;
		}
	}
	if (i == h->len && !remove) {
		HistoryEntry *e = g_new(HistoryEntry, 1);

		e->name = g_strdup(name);
		e->count = 1;
		g_ptr_array_add(h, e);
	}
	history_save(h, size);
	g_ptr_array_unref(h);
}

/* ---- list of programs -------------------------------------------------- */

static void
add_unique(GPtrArray *cmds, GHashTable *seen, const char *name)
{
	if (*name && !g_hash_table_contains(seen, name)) {
		char *copy = g_strdup(name);

		g_hash_table_add(seen, copy);
		g_ptr_array_add(cmds, copy);
	}
}

static gint
str_cmp(gconstpointer a, gconstpointer b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Executables in $PATH, sorted, when dmenu_path is not available. */
static void
add_path_programs(GPtrArray *cmds, GHashTable *seen)
{
	GPtrArray *found = g_ptr_array_new_with_free_func(g_free);
	char **dirs = g_strsplit(g_getenv("PATH") ? g_getenv("PATH") : "", ":", -1);
	unsigned int i;

	for (i = 0; dirs[i]; i++) {
		GDir *dir = g_dir_open(dirs[i], 0, NULL);
		const char *name;

		if (!dir)
			continue;
		while ((name = g_dir_read_name(dir))) {
			char *full = g_build_filename(dirs[i], name, NULL);

			if (g_file_test(full, G_FILE_TEST_IS_EXECUTABLE) &&
			    !g_file_test(full, G_FILE_TEST_IS_DIR))
				g_ptr_array_add(found, g_strdup(name));
			g_free(full);
		}
		g_dir_close(dir);
	}
	g_ptr_array_sort(found, str_cmp);
	for (i = 0; i < found->len; i++)
		add_unique(cmds, seen, g_ptr_array_index(found, i));
	g_ptr_array_unref(found);
	g_strfreev(dirs);
}

static void
typorun_load(TyporunData *pd)
{
	GPtrArray *h = history_load(), *cmds = g_ptr_array_new();
	GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
	char *out = NULL, *dmenu_path = g_find_program_in_path("dmenu_path");
	unsigned int i;

	for (i = 0; i < h->len; i++)
		add_unique(cmds, seen, ((HistoryEntry *)g_ptr_array_index(h, i))->name);
	pd->nhistory = cmds->len;
	if (dmenu_path && g_spawn_command_line_sync(dmenu_path, &out, NULL, NULL, NULL) && out) {
		char **lines = g_strsplit(out, "\n", -1);

		for (i = 0; lines[i]; i++)
			add_unique(cmds, seen, lines[i]);
		g_strfreev(lines);
	} else
		add_path_programs(cmds, seen);
	g_ptr_array_add(cmds, NULL);
	pd->n = cmds->len - 1;
	pd->cmds = (char **)g_ptr_array_free(cmds, FALSE); /* strings owned here */
	g_hash_table_destroy(seen);
	g_ptr_array_unref(h);
	g_free(dmenu_path);
	g_free(out);
}

/* ---- mode callbacks ----------------------------------------------------- */

static int
typorun_init(Mode *sw)
{
	TyporunData *pd;

	if (mode_get_private_data(sw))
		return TRUE;
	pd = g_malloc0(sizeof *pd);
	typo_matcher_init(&pd->matcher);
	pd->history_size = 25;
	find_arg_uint("-typorun-history-size", &pd->history_size);
	typorun_load(pd);
	mode_set_private_data(sw, pd);
	return TRUE;
}

static unsigned int
typorun_get_num_entries(const Mode *sw)
{
	const TyporunData *pd = mode_get_private_data(sw);

	return pd ? pd->n : 0;
}

static void
typorun_run(const char *cmd, gboolean in_terminal)
{
	char *template = "{cmd}", *quoted = g_shell_quote(cmd), *line;
	char **parts;

	find_arg_str("-typorun-command", &template);
	parts = g_strsplit(template, "{cmd}", -1);
	line = g_strjoinv(quoted, parts);
	/* without a template the command line is run as typed, not quoted */
	if (strcmp(template, "{cmd}") == 0) {
		g_free(line);
		line = g_strdup(cmd);
	}
	helper_execute_command(NULL, line, in_terminal, NULL);
	g_strfreev(parts);
	g_free(quoted);
	g_free(line);
}

static ModeMode
typorun_result(Mode *sw, int mretv, char **input, unsigned int selected_line)
{
	TyporunData *pd = mode_get_private_data(sw);
	gboolean in_terminal = (mretv & MENU_CUSTOM_ACTION) != 0;
	const char *cmd = NULL;

	if (mretv & MENU_NEXT)
		return NEXT_DIALOG;
	if (mretv & MENU_PREVIOUS)
		return PREVIOUS_DIALOG;
	if (mretv & MENU_QUICK_SWITCH)
		return mretv & MENU_LOWER_MASK;
	if (!pd)
		return MODE_EXIT;
	if ((mretv & MENU_ENTRY_DELETE) && selected_line < pd->nhistory) {
		history_update(pd->cmds[selected_line], TRUE, pd->history_size);
		g_strfreev(pd->cmds);
		typorun_load(pd);
		return RELOAD_DIALOG;
	}
	if ((mretv & MENU_OK) && selected_line < pd->n)
		cmd = pd->cmds[selected_line];
	else if ((mretv & MENU_CUSTOM_INPUT) && input && *input && **input)
		cmd = *input;
	if (cmd) {
		history_update(cmd, FALSE, pd->history_size);
		typorun_run(cmd, in_terminal);
	}
	return MODE_EXIT;
}

static void
typorun_destroy(Mode *sw)
{
	TyporunData *pd = mode_get_private_data(sw);

	if (!pd)
		return;
	g_strfreev(pd->cmds);
	typo_matcher_free(&pd->matcher);
	g_free(pd);
	mode_set_private_data(sw, NULL);
}

static char *
typorun_preprocess_input(Mode *sw, const char *input)
{
	TyporunData *pd = mode_get_private_data(sw);

	if (pd)
		typo_matcher_set_input(&pd->matcher, input);
	return g_strdup(input);
}

static int
typorun_token_match(const Mode *sw, rofi_int_matcher **tokens, unsigned int index)
{
	const TyporunData *pd = mode_get_private_data(sw);

	return pd && index < pd->n &&
	       typo_matcher_match(&pd->matcher, tokens, pd->cmds[index]);
}

static char *
typorun_get_display_value(const Mode *sw, unsigned int selected_line,
                          G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr,
                          int get_entry)
{
	const TyporunData *pd = mode_get_private_data(sw);

	if (!get_entry || !pd || selected_line >= pd->n)
		return NULL;
	return g_strdup(pd->cmds[selected_line]);
}

static char *
typorun_get_completion(const Mode *sw, unsigned int selected_line)
{
	const TyporunData *pd = mode_get_private_data(sw);

	return g_strdup(pd && selected_line < pd->n ? pd->cmds[selected_line] : "");
}

Mode mode = {
	.abi_version = ABI_VERSION,
	.name = "typorun",
	.cfg_name_key = "display-typorun",
	._init = typorun_init,
	._get_num_entries = typorun_get_num_entries,
	._result = typorun_result,
	._destroy = typorun_destroy,
	._token_match = typorun_token_match,
	._get_display_value = typorun_get_display_value,
	._get_completion = typorun_get_completion,
	._preprocess_input = typorun_preprocess_input,
	._get_message = NULL,
	.private_data = NULL,
	.free = NULL,
	.type = MODE_TYPE_SWITCHER,
};
