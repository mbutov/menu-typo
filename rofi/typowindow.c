/*
 * rofi mode "typowindow": switch windows of i3 or sway, like the built-in
 * "window" mode, with typo-tolerant matching.
 *
 * Windows come from the window manager's IPC (get_tree), most recently
 * focused first. An entry reads "workspace  class  title" (on sway, app_id
 * stands in for the class of Wayland windows). Enter focuses the window,
 * Shift+Delete closes it.
 *
 * The IPC socket is $SWAYSOCK, $I3SOCK or the output of
 * "i3 --get-socketpath". Options: -typo-min-len-1 / -typo-min-len-2, see
 * common.h.
 *
 * Copyright (c) 2026 Maxim Butov, MIT license (see LICENSE).
 */
#include <gio/gio.h>
#include <gmodule.h>
#include <json-glib/json-glib.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>
#include <stdint.h>
#include <string.h>

#include "common.h"

G_MODULE_EXPORT Mode mode;

enum { IPC_RUN_COMMAND = 0, IPC_GET_TREE = 4 };

typedef struct {
	gint64 *ids;
	char **lines; /* what is shown and matched */
	unsigned int n;
	TypoMatcher matcher;
} TypowindowData;

/* ---- i3/sway IPC -------------------------------------------------------- */

static char *
ipc_socket_path(void)
{
	const char *env = g_getenv("SWAYSOCK");
	char *out = NULL;

	if (!env || !*env)
		env = g_getenv("I3SOCK");
	if (env && *env)
		return g_strdup(env);
	if (g_spawn_command_line_sync("i3 --get-socketpath", &out, NULL, NULL, NULL) && out)
		return g_strstrip(out);
	g_free(out);
	return NULL;
}

static gboolean
read_all(GInputStream *in, void *buf, gsize len)
{
	gsize got = 0;

	return g_input_stream_read_all(in, buf, len, &got, NULL, NULL) && got == len;
}

/* Sends one message, returns the reply payload (NUL-terminated) or NULL. */
static char *
ipc_request(uint32_t type, const char *payload)
{
	static const char magic[] = "i3-ipc";
	char *path = ipc_socket_path(), *reply = NULL, head[14];
	GSocketClient *client;
	GSocketConnection *conn = NULL;
	GSocketAddress *addr;
	uint32_t len = payload ? strlen(payload) : 0, rlen;

	if (!path)
		return NULL;
	client = g_socket_client_new();
	addr = g_unix_socket_address_new(path);
	conn = g_socket_client_connect(client, G_SOCKET_CONNECTABLE(addr), NULL, NULL);
	if (conn) {
		GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
		GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(conn));

		memcpy(head, magic, 6);
		memcpy(head + 6, &len, 4);
		memcpy(head + 10, &type, 4);
		if (g_output_stream_write_all(out, head, sizeof head, NULL, NULL, NULL) &&
		    (!len || g_output_stream_write_all(out, payload, len, NULL, NULL, NULL)) &&
		    read_all(in, head, sizeof head) && memcmp(head, magic, 6) == 0) {
			memcpy(&rlen, head + 6, 4);
			reply = g_malloc(rlen + 1);
			if (read_all(in, reply, rlen))
				reply[rlen] = '\0';
			else {
				g_free(reply);
				reply = NULL;
			}
		}
		g_object_unref(conn);
	}
	g_object_unref(addr);
	g_object_unref(client);
	g_free(path);
	return reply;
}

/* ---- window list -------------------------------------------------------- */

static const char *
member_string(JsonObject *o, const char *name)
{
	JsonNode *n = json_object_get_member(o, name);

	return n && JSON_NODE_HOLDS_VALUE(n) ? json_node_get_string(n) : NULL;
}

/* Children (tiling and floating) of a container, in focus order. */
static GList *
children_by_focus(JsonObject *o)
{
	GList *all = NULL, *sorted = NULL, *l;
	const char *members[] = {"nodes", "floating_nodes"};
	JsonArray *focus;
	unsigned int i, k;

	for (k = 0; k < 2; k++) {
		JsonArray *a = json_object_has_member(o, members[k]) ?
			json_object_get_array_member(o, members[k]) : NULL;

		for (i = 0; a && i < json_array_get_length(a); i++)
			all = g_list_append(all, json_array_get_object_element(a, i));
	}
	focus = json_object_has_member(o, "focus") ?
		json_object_get_array_member(o, "focus") : NULL;
	for (i = 0; focus && i < json_array_get_length(focus); i++) {
		gint64 id = json_array_get_int_element(focus, i);

		for (l = all; l; l = l->next)
			if (json_object_get_int_member(l->data, "id") == id) {
				sorted = g_list_append(sorted, l->data);
				all = g_list_delete_link(all, l);
				break;
			}
	}
	return g_list_concat(sorted, all);
}

static void
collect(JsonObject *o, const char *workspace, GArray *ids, GPtrArray *lines)
{
	const char *type = member_string(o, "type");
	gboolean has_children;
	GList *children, *l;

	if (type && strcmp(type, "dockarea") == 0)
		return; /* bars */
	if (type && strcmp(type, "workspace") == 0)
		workspace = member_string(o, "name");
	children = children_by_focus(o);
	has_children = children != NULL;
	if (!has_children && workspace &&
	    ((json_object_has_member(o, "window") &&
	      !json_node_is_null(json_object_get_member(o, "window"))) ||
	     member_string(o, "app_id"))) {
		const char *cls = member_string(o, "app_id"), *title = member_string(o, "name");
		gint64 id = json_object_get_int_member(o, "id");

		if (!cls && json_object_has_member(o, "window_properties"))
			cls = member_string(json_object_get_object_member(o, "window_properties"), "class");
		g_array_append_val(ids, id);
		g_ptr_array_add(lines, g_strdup_printf("%-3s  %-20s  %s", workspace,
		                                       cls ? cls : "", title ? title : ""));
	}
	for (l = children; l; l = l->next)
		collect(l->data, workspace, ids, lines);
	g_list_free(children);
}

static void
typowindow_load(TypowindowData *pd)
{
	char *tree = ipc_request(IPC_GET_TREE, NULL);
	JsonParser *parser = json_parser_new();
	GArray *ids = g_array_new(FALSE, FALSE, sizeof(gint64));
	GPtrArray *lines = g_ptr_array_new();

	if (tree && json_parser_load_from_data(parser, tree, -1, NULL)) {
		JsonNode *root = json_parser_get_root(parser);

		if (root && JSON_NODE_HOLDS_OBJECT(root))
			collect(json_node_get_object(root), NULL, ids, lines);
	}
	pd->n = ids->len;
	pd->ids = (gint64 *)g_array_free(ids, FALSE);
	g_ptr_array_add(lines, NULL);
	pd->lines = (char **)g_ptr_array_free(lines, FALSE);
	g_object_unref(parser);
	g_free(tree);
}

static void
typowindow_clear(TypowindowData *pd)
{
	g_free(pd->ids);
	g_strfreev(pd->lines);
	pd->ids = NULL;
	pd->lines = NULL;
	pd->n = 0;
}

static void
typowindow_command(gint64 id, const char *what)
{
	char *cmd = g_strdup_printf("[con_id=%" G_GINT64_FORMAT "] %s", id, what);

	g_free(ipc_request(IPC_RUN_COMMAND, cmd));
	g_free(cmd);
}

/* ---- mode callbacks ----------------------------------------------------- */

static int
typowindow_init(Mode *sw)
{
	TypowindowData *pd;

	if (mode_get_private_data(sw))
		return TRUE;
	pd = g_malloc0(sizeof *pd);
	typo_matcher_init(&pd->matcher);
	typowindow_load(pd);
	mode_set_private_data(sw, pd);
	return TRUE;
}

static unsigned int
typowindow_get_num_entries(const Mode *sw)
{
	const TypowindowData *pd = mode_get_private_data(sw);

	return pd ? pd->n : 0;
}

static ModeMode
typowindow_result(Mode *sw, int mretv, G_GNUC_UNUSED char **input,
                  unsigned int selected_line)
{
	TypowindowData *pd = mode_get_private_data(sw);

	if (mretv & MENU_NEXT)
		return NEXT_DIALOG;
	if (mretv & MENU_PREVIOUS)
		return PREVIOUS_DIALOG;
	if (mretv & MENU_QUICK_SWITCH)
		return mretv & MENU_LOWER_MASK;
	if (!pd || selected_line >= pd->n)
		return MODE_EXIT;
	if (mretv & MENU_ENTRY_DELETE) {
		typowindow_command(pd->ids[selected_line], "kill");
		g_usleep(100000); /* let the window leave the tree */
		typowindow_clear(pd);
		typowindow_load(pd);
		return RELOAD_DIALOG;
	}
	if (mretv & MENU_OK)
		typowindow_command(pd->ids[selected_line], "focus");
	return MODE_EXIT;
}

static void
typowindow_destroy(Mode *sw)
{
	TypowindowData *pd = mode_get_private_data(sw);

	if (!pd)
		return;
	typowindow_clear(pd);
	typo_matcher_free(&pd->matcher);
	g_free(pd);
	mode_set_private_data(sw, NULL);
}

static char *
typowindow_preprocess_input(Mode *sw, const char *input)
{
	TypowindowData *pd = mode_get_private_data(sw);

	if (pd)
		typo_matcher_set_input(&pd->matcher, input);
	return g_strdup(input);
}

static int
typowindow_token_match(const Mode *sw, rofi_int_matcher **tokens, unsigned int index)
{
	const TypowindowData *pd = mode_get_private_data(sw);

	return pd && index < pd->n &&
	       typo_matcher_match(&pd->matcher, tokens, pd->lines[index]);
}

static char *
typowindow_get_display_value(const Mode *sw, unsigned int selected_line,
                             G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr,
                             int get_entry)
{
	const TypowindowData *pd = mode_get_private_data(sw);

	if (!get_entry || !pd || selected_line >= pd->n)
		return NULL;
	return g_strdup(pd->lines[selected_line]);
}

Mode mode = {
	.abi_version = ABI_VERSION,
	.name = "typowindow",
	.cfg_name_key = "display-typowindow",
	._init = typowindow_init,
	._get_num_entries = typowindow_get_num_entries,
	._result = typowindow_result,
	._destroy = typowindow_destroy,
	._token_match = typowindow_token_match,
	._get_display_value = typowindow_get_display_value,
	._preprocess_input = typowindow_preprocess_input,
	._get_completion = NULL,
	._get_message = NULL,
	.private_data = NULL,
	.free = NULL,
	.type = MODE_TYPE_SWITCHER,
};
