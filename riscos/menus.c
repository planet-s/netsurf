/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Menu creation and handling (implementation).
 */

#include <stdlib.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/help.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


struct ns_menu_entry {
	const char *text;		/**< menu text (from messages) */
	menu_action action;		/**< associated action */
	wimp_w sub_window;		/**< sub-window if any */
};

struct ns_menu {
	const char *title;
	struct ns_menu_entry entries[1];
};

#define NS_MENU(N) \
	struct { \
		const char *title; \
		struct ns_menu_entry entries[N]; \
	}

struct menu_definition_entry {
	menu_action action;			/**< menu action */
	wimp_menu_entry *menu_entry;		/**< corresponding menu entry */
	struct menu_definition_entry *next;	/**< next menu entry */
};

struct menu_definition {
	wimp_menu *menu;			/**< corresponding menu */
	struct menu_definition_entry *entries;	/**< menu entries */
	struct menu_definition *next;		/**< next menu */
};


static wimp_menu *ro_gui_menu_define_menu(struct ns_menu *menu);
static void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		struct ns_menu *menu, int depth, wimp_menu_entry *link,
		int first, int last, const char *prefix, int prefix_length);
static struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu);
static struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action);
static menu_action ro_gui_menu_find_action(wimp_menu *menu,
		wimp_menu_entry *menu_entry);
static void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded);
static void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked);
static void ro_gui_menu_get_window_details(wimp_w w, struct gui_window **g,
		struct browser_window **bw, struct content **content,
		struct toolbar **toolbar, struct tree **tree);
static int ro_gui_menu_get_checksum(void);
static bool ro_gui_menu_prepare_url_suggest(void);
static void ro_gui_menu_prepare_pageinfo(struct gui_window *g);
static void ro_gui_menu_prepare_objectinfo(struct box *box);
static void ro_gui_menu_refresh_toolbar(struct toolbar *toolbar);


/* default menu item flags */
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))


/** The currently defined menus to perform actions for */
static struct menu_definition *ro_gui_menu_definitions;
/** The current menu being worked with (may not be open) */
wimp_menu *current_menu;
/** Whether a menu is currently open */
static bool current_menu_open = false;
/** Box for object under menu, or 0 if no object. */
static struct box *current_menu_object_box = 0;
/** Menu of options for form select controls. */
static wimp_menu *gui_form_select_menu = 0;
/** Form control which gui_form_select_menu is for. */
static struct form_control *gui_form_select_control;
/** Window that owns the current menu */
static wimp_w current_menu_window;
/** The height of the iconbar menu */
int iconbar_menu_height = 5 * 44;
/** The available menus */
wimp_menu *iconbar_menu, *browser_menu, *hotlist_menu, *global_history_menu,
	*image_quality_menu, *browser_toolbar_menu,
	*tree_toolbar_menu, *proxy_auth_menu, *languages_menu;
/** URL suggestion menu */
static wimp_MENU(GLOBAL_HISTORY_RECENT_URLS) url_suggest;
wimp_menu *url_suggest_menu = (wimp_menu *)&url_suggest;


/**
 * Create menu structures.
 */
void ro_gui_menu_init(void) {
	int context = 0, read_count, entries = 0;
	os_error *error;
	osgbpb_INFO(100) info;
	char lang[8] = {0};
	char *lang_name;
	void *temp;

	/* iconbar menu */
	NS_MENU(9) iconbar_definition = {
		"NetSurf", {
			{ "Info", NO_ACTION, dialog_info },
			{ "AppHelp", HELP_OPEN_CONTENTS, 0 },
			{ "Open", NO_ACTION, 0 },
			{ "Open.OpenURL", BROWSER_NAVIGATE_URL, dialog_openurl },
			{ "Open.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Open.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Choices", CHOICES_SHOW, 0 },
			{ "Quit", APPLICATION_QUIT, 0 },
			{NULL, 0, 0}
		}
	};
	iconbar_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&iconbar_definition);

	/* browser menu */
	NS_MENU(66) browser_definition = {
		"NetSurf", {
			{ "Page", BROWSER_PAGE, 0 },
			{ "Page.PageInfo",BROWSER_PAGE_INFO, dialog_pageinfo },
			{ "Page.Save", BROWSER_SAVE, dialog_saveas },
			{ "Page.SaveComp", BROWSER_SAVE_COMPLETE, dialog_saveas },
			{ "Page.Export", NO_ACTION, 0 },
			{ "Page.Export.Draw", BROWSER_EXPORT_DRAW, dialog_saveas },
			{ "Page.Export.Text", BROWSER_EXPORT_TEXT, dialog_saveas },
			{ "Page.SaveURL", NO_ACTION, 0 },
			{ "Page.SaveURL.URI", BROWSER_SAVE_URL_URI, dialog_saveas },
			{ "Page.SaveURL.URL", BROWSER_SAVE_URL_URL, dialog_saveas },
			{ "Page.SaveURL.LinkText", BROWSER_SAVE_URL_TEXT, dialog_saveas },
			{ "Page.Print_", BROWSER_PRINT, dialog_print },
			{ "Page.NewWindow", BROWSER_NEW_WINDOW, 0 },
			{ "Page.ViewSrc", BROWSER_VIEW_SOURCE, 0 },
			{ "Object", BROWSER_OBJECT, 0 },
			{ "Object.ObjInfo", BROWSER_OBJECT_INFO, dialog_objinfo },
			{ "Object.ObjSave", BROWSER_OBJECT_SAVE, dialog_saveas },
			{ "Object.Export", NO_ACTION, 0 },
			{ "Object.Export.Sprite", BROWSER_OBJECT_EXPORT_SPRITE, dialog_saveas },
			{ "Object.SaveURL_", NO_ACTION, 0 },
			{ "Object.SaveURL.URI", BROWSER_OBJECT_SAVE_URL_URI, dialog_saveas },
			{ "Object.SaveURL.URL", BROWSER_OBJECT_SAVE_URL_URL, dialog_saveas },
			{ "Object.SaveURL.LinkText", BROWSER_OBJECT_SAVE_URL_TEXT, dialog_saveas },
			{ "Object.ObjReload", BROWSER_OBJECT_RELOAD, 0 },
			{ "Navigate", NO_ACTION, 0 },
			{ "Navigate.Home", BROWSER_NAVIGATE_HOME, 0 },
			{ "Navigate.Back", BROWSER_NAVIGATE_BACK, 0 },
			{ "Navigate.Forward_", BROWSER_NAVIGATE_FORWARD, 0 },
			{ "Navigate.Reload", BROWSER_NAVIGATE_RELOAD_ALL, 0 },
			{ "Navigate.Stop", BROWSER_NAVIGATE_STOP, 0 },
			{ "View", NO_ACTION, 0 },
			{ "View.ScaleView", BROWSER_SCALE_VIEW, dialog_zoom },
			{ "View.Images", NO_ACTION, 0 },
			{ "View.Images.ForeImg", BROWSER_IMAGES_FOREGROUND, 0 },
			{ "View.Images.BackImg", BROWSER_IMAGES_BACKGROUND, 0 },
			{ "View.Toolbars", NO_ACTION, 0 },
			{ "View.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "View.Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "View.Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "View.Toolbars.ToolStatus_", TOOLBAR_STATUS_BAR, 0 },
			{ "View.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "View.Render_", NO_ACTION, 0 },
			{ "View.Render.RenderAnims", BROWSER_BUFFER_ANIMS, 0 },
			{ "View.Render.RenderAll", BROWSER_BUFFER_ALL, 0 },
			{ "View.OptDefault", BROWSER_SAVE_VIEW, 0 },
			{ "Utilities", NO_ACTION, 0 },
			{ "Utilities.Hotlist", HOTLIST_SHOW, 0 },
			{ "Utilities.Hotlist.HotlistAdd", HOTLIST_ADD_URL, 0 },
			{ "Utilities.Hotlist.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Utilities.History", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.History.HistLocal", HISTORY_SHOW_LOCAL, 0 },
			{ "Utilities.History.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Utilities.FindText", BROWSER_FIND_TEXT, dialog_search },
			{ "Utilities.Window", NO_ACTION, 0 },
			{ "Utilities.Window.WindowSave", BROWSER_WINDOW_DEFAULT, 0 },
			{ "Utilities.Window.WindowStagr", BROWSER_WINDOW_STAGGER, 0 },
			{ "Utilities.Window.WindowSize_", BROWSER_WINDOW_COPY, 0 },
			{ "Utilities.Window.WindowReset", BROWSER_WINDOW_RESET, 0 },
			{ "Help", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpContent", HELP_OPEN_CONTENTS, 0 },
			{ "Help.HelpGuide", HELP_OPEN_GUIDE, 0 },
			{ "Help.HelpInfo_", HELP_OPEN_INFORMATION, 0 },
			{ "Help.HelpAbout_", HELP_OPEN_ABOUT, 0 },
			{ "Help.HelpInter", HELP_LAUNCH_INTERACTIVE, 0 },
			{NULL, 0, 0}
		}
	};
	browser_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&browser_definition);

	/* hotlist menu */
	NS_MENU(24) hotlist_definition = {
		"Hotlist", {
			{ "Hotlist", NO_ACTION, 0 },
			{ "Hotlist.New", NO_ACTION, 0 },
			{ "Hotlist.New.Folder", TREE_NEW_FOLDER, dialog_folder },
			{ "Hotlist.New.Link", TREE_NEW_LINK, dialog_entry },
			{ "Hotlist.Export_", HOTLIST_EXPORT, dialog_saveas },
			{ "Hotlist.Expand", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Hotlist.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Hotlist.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Hotlist.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Hotlist.Toolbars", NO_ACTION, 0 },
			{ "Hotlist.Toolbars.ToolButtons_", TOOLBAR_BUTTONS, 0 },
			{ "Hotlist.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Edit", TREE_SELECTION_EDIT, (wimp_w)1 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "Selection.ResetUsage", TREE_SELECTION_RESET, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	hotlist_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&hotlist_definition);

	/* history menu */
	NS_MENU(19) global_history_definition = {
		"History", {
			{ "History", NO_ACTION, 0 },
			{ "History.Export_", HISTORY_EXPORT, dialog_saveas },
			{ "History.Expand", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "History.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "History.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "History.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "History.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "History.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "History.Toolbars", NO_ACTION, 0 },
			{ "History.Toolbars.ToolButtons_", TOOLBAR_BUTTONS, 0 },
			{ "History.Toolbars.EditToolbar",TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};
	global_history_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&global_history_definition);

	/* image quality menu */
	NS_MENU(5) images_definition = {
		"Display", {
			{ "ImgStyle0", NO_ACTION, 0 },
			{ "ImgStyle1", NO_ACTION, 0 },
			{ "ImgStyle2", NO_ACTION, 0 },
			{ "ImgStyle3", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	image_quality_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&images_definition);

	/* browser toolbar menu */
	NS_MENU(7) browser_toolbar_definition = {
		"Toolbar", {
			{ "Toolbars", NO_ACTION, 0 },
			{ "Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "Toolbars.ToolStatus", TOOLBAR_STATUS_BAR, 0 },
			{ "EditToolbar", TOOLBAR_EDIT, 0 },
			{NULL, 0, 0}
		}
	};
	browser_toolbar_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&browser_toolbar_definition);

	/* tree toolbar menu */
	NS_MENU(4) tree_toolbar_definition = {
		"Toolbar", {
			{ "Toolbars", NO_ACTION, 0 },
			{ "Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "EditToolbar", TOOLBAR_EDIT, 0 },
			{NULL, 0, 0}
		}
	};
	tree_toolbar_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&tree_toolbar_definition);

	/* proxy menu */
	NS_MENU(4) proxy_auth_definition = {
		"ProxyAuth", {
			{ "ProxyNone", NO_ACTION, 0 },
			{ "ProxyBasic", NO_ACTION, 0 },
			{ "ProxyNTLM", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	proxy_auth_menu = ro_gui_menu_define_menu(
			(struct ns_menu *)&proxy_auth_definition);

	/* special case menus */
	url_suggest_menu->title_data.indirected_text.text =
			messages_get("URLSuggest");
	url_suggest_menu->title_fg = wimp_COLOUR_BLACK;
	url_suggest_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	url_suggest_menu->work_fg = wimp_COLOUR_BLACK;
	url_suggest_menu->work_bg = wimp_COLOUR_WHITE;
	url_suggest_menu->width = 200;
	url_suggest_menu->height = wimp_MENU_ITEM_HEIGHT;
	url_suggest_menu->gap = wimp_MENU_ITEM_GAP;

	/* language menu */
	languages_menu = calloc(1, wimp_SIZEOF_MENU(1));
	if (!languages_menu)
		die("Insufficient memory for languages menu.");
	languages_menu->title_data.indirected_text.text =
			messages_get("Languages");
	languages_menu->title_fg = wimp_COLOUR_BLACK;
	languages_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	languages_menu->work_fg = wimp_COLOUR_BLACK;
	languages_menu->work_bg = wimp_COLOUR_WHITE;
	languages_menu->width = 200;
	languages_menu->height = wimp_MENU_ITEM_HEIGHT;
	languages_menu->gap = wimp_MENU_ITEM_GAP;

	while (context != -1) {
		error = xosgbpb_dir_entries_info("<NetSurf$Dir>.Resources",
				(osgbpb_info_list*)&info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error)
			die(error->errmess);
		if ((read_count == 0) || (info.obj_type != fileswitch_IS_DIR))
			continue;

		snprintf(lang, sizeof lang, "lang_%2s", info.name);
		/* we can't duplicate the string returned from our messages as
		 * it causes value->key lookups to fail */
		lang_name = messages_get(lang);
		if ((lang_name == lang) || (strlen(info.name) != 2))
			continue;

		temp = realloc(languages_menu, wimp_SIZEOF_MENU(entries + 1));
		if (!temp)
			die("Insufficient memory for languages menu");

		languages_menu = temp;
		languages_menu->entries[entries].menu_flags = 0;
		languages_menu->entries[entries].sub_menu = wimp_NO_SUB_MENU;
		languages_menu->entries[entries].icon_flags = DEFAULT_FLAGS |
				wimp_ICON_INDIRECTED;
		languages_menu->entries[entries].data.indirected_text.text =
				lang_name;
		languages_menu->entries[entries].data.indirected_text.
				validation = (char *)-1;
		languages_menu->entries[entries].data.indirected_text.size =
				strlen(lang_name) + 1;
		entries++;
	}

	languages_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	languages_menu->entries[entries-1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Display a menu.
 */
void ro_gui_menu_create(wimp_menu *menu, int x, int y, wimp_w w) {
	int doc_x, doc_y;
	wimp_window_state state;
	struct gui_window *g;
	os_error *error;
	int i;
	menu_action action;

	/* read the object under the pointer for a new gui_window menu */
	if ((!current_menu) && (menu == browser_menu)) {
		state.w = w;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		g = ro_gui_window_lookup(w);
		assert(g);

		doc_x = window_x_units(x, &state) / 2 / g->option.scale;
		doc_y = -window_y_units(y, &state) / 2 / g->option.scale;

		current_menu_object_box = NULL;
		if (g->bw->current_content &&
				g->bw->current_content->type == CONTENT_HTML)
			current_menu_object_box = box_object_at_point(
					g->bw->current_content, doc_x, doc_y);
	}

	/* store the menu characteristics */
	current_menu = menu;
	current_menu_window = w;

	/* prepare the menu state */
	if (menu == url_suggest_menu) {
		if (!ro_gui_menu_prepare_url_suggest())
			return;
	} else {
		i = 0;
		do {
			action = ro_gui_menu_find_action(menu,
					&menu->entries[i]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(w, action, false);
		} while (!(menu->entries[i++].menu_flags & wimp_MENU_LAST));
	}

	/* create the menu */
	current_menu_open = true;
	error = xwimp_create_menu(menu, x - 64, y);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		ro_gui_menu_closed();
	}
}


/**
 * Display a pop-up menu next to the specified icon.
 *
 * \param  menu  menu to open
 * \param  w	 window handle
 * \param  i	 icon handle
 */
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i) {
	wimp_window_state state;
	wimp_icon_state icon_state;
	os_error *error;

	state.w = w;
	icon_state.w = w;
	icon_state.i = i;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	ro_gui_menu_create(menu,
			state.visible.x0 + icon_state.icon.extent.x1 + 64,
			state.visible.y1 + icon_state.icon.extent.y1 -
			state.yscroll, w);
}


/**
 * Clean up after a menu has been closed, or forcible close an open menu.
 */
void ro_gui_menu_closed(void) {
	struct gui_window *g;
	struct browser_window *bw;
	struct content *c;
	struct toolbar *t;
	struct tree *tree;
	os_error *error;

	if (!current_menu)
		return;

	error = xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}

	ro_gui_menu_get_window_details(current_menu_window, &g, &bw, &c, &t, &tree);

	current_menu = NULL;
	current_menu_window = NULL;
	current_menu_open = false;

	if (tree)
		ro_gui_tree_menu_closed(tree);
		
}


/**
 * The content has changed, reset object references
 */
void ro_gui_menu_objects_moved(void) {
  	gui_form_select_control = NULL;
	current_menu_object_box = NULL;
	
	ro_gui_menu_prepare_action(0, BROWSER_OBJECT, false);
	if (current_menu == gui_form_select_menu)
		ro_gui_menu_closed();
}


/**
 * Handle menu selection.
 */
void ro_gui_menu_selection(wimp_selection *selection) {
	int i, j;
	wimp_menu_entry *menu_entry;
	menu_action action;
	wimp_pointer pointer;
	struct gui_window *g = NULL;
	wimp_menu *menu;
	os_error *error;

	assert(current_menu);
	assert(current_menu_window);

	/* get the menu entry and associated action */
	menu_entry = &current_menu->entries[selection->items[0]];
	for (i = 1; selection->items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[selection->items[i]];
	action = ro_gui_menu_find_action(current_menu, menu_entry);

	/* perform menu action */
	if (action != NO_ACTION)
		ro_gui_menu_handle_action(current_menu_window, action, false);

	/* perform non-automated actions */
	if (current_menu == url_suggest_menu) {
		g = ro_gui_toolbar_lookup(current_menu_window);
		assert(g);
		browser_window_go(g->bw,
				url_suggest_menu->entries[selection->items[0]].
						data.indirected_text.text, 0);
		global_history_add_recent(url_suggest_menu->
				entries[selection->items[0]].
						data.indirected_text.text);
	} else if (current_menu == proxy_auth_menu) {
		ro_gui_dialog_proxyauth_menu_selection(selection->items[0]);
	} else if (current_menu == image_quality_menu) {
		ro_gui_dialog_image_menu_selection(selection->items[0]);
	} else if (current_menu == languages_menu) {
		ro_gui_dialog_languages_menu_selection(languages_menu->
				entries[selection->items[0]].
				data.indirected_text.text);
	} else if (current_menu == font_menu) {
		ro_gui_dialog_font_menu_selection(selection->items[0]);
	} else if ((current_menu == gui_form_select_menu) &&
			(selection->items[0] >= 0)) {
		g = ro_gui_window_lookup(current_menu_window);
		assert(g);
		browser_window_form_select(g->bw,
				gui_form_select_control,
				selection->items[0]);
	}

	/* re-open the menu for Adjust clicks */
	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed();
		return;
	}

	if (pointer.buttons != wimp_CLICK_ADJUST) {
		ro_gui_menu_closed();
		return;
	}

	/* re-prepare all the visible enties */
	i = 0;
	menu = current_menu;
	do {
		j = 0;
		do {
			action = ro_gui_menu_find_action(current_menu,
					&menu->entries[j]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(current_menu_window,
						action, false);
		} while (!(menu->entries[j++].menu_flags & wimp_MENU_LAST));
		j = selection->items[i++];
		if (j != -1)
			menu = menu->entries[j].sub_menu;
	} while (j != -1);

	if (current_menu == gui_form_select_menu)
		gui_create_form_select_menu(g->bw,
				gui_form_select_control);
	else
		ro_gui_menu_create(current_menu, 0, 0, current_menu_window);
}


/**
 * Handle Message_MenuWarning.
 */
void ro_gui_menu_warning(wimp_message_menu_warning *warning) {
	int i;
	menu_action action;
	wimp_menu_entry *menu_entry;
	wimp_menu *sub_menu;
	os_error *error;
	int menu_check;

	assert(current_menu);
	assert(current_menu_window);

	/* get the sub-menu of the warning */
	if (warning->selection.items[0] == -1)
		return;
	menu_entry = &current_menu->entries[warning->selection.items[0]];
	for (i = 1; warning->selection.items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[warning->selection.items[i]];

	/* the values given in PRM 3-157 for how to check menus/windows are
	 * incorrect so we use a hack of checking if the sub-menu is within
	 * 256KB of its parent */
	menu_check = abs((int)menu_entry->sub_menu - (int)menu_entry);
	if (menu_check < 0x40000) {
		sub_menu = menu_entry->sub_menu;
		i = 0;
		do {
			action = ro_gui_menu_find_action(current_menu,
					&sub_menu->entries[i]);
			if (action != NO_ACTION)
				ro_gui_menu_prepare_action(current_menu_window,
						action, false);
		} while (!(sub_menu->entries[i++].menu_flags & wimp_MENU_LAST));
	} else {
		action = ro_gui_menu_find_action(current_menu, menu_entry);
		if (action != NO_ACTION)
			ro_gui_menu_prepare_action(current_menu_window,
					action, true);
		/* remove the close icon */
		ro_gui_wimp_update_window_furniture((wimp_w)menu_entry->sub_menu,
				wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_BACK_ICON, 0);
	}

	/* open the sub-menu */
	error = xwimp_create_sub_menu(menu_entry->sub_menu,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Refresh a toolbar after it has been updated
 *
 * \param toolbar  the toolbar to update
 */
void ro_gui_menu_refresh_toolbar(struct toolbar *toolbar) {
	int height;

	assert(toolbar);

	toolbar->reformat_buttons = true;
	height = toolbar->height;
	ro_gui_theme_process_toolbar(toolbar, -1);
	if (toolbar->type == THEME_BROWSER_TOOLBAR) {
		ro_gui_window_update_dimensions(
				ro_gui_window_lookup(current_menu_window),
				height - toolbar->height);
	} else if (toolbar->type == THEME_HOTLIST_TOOLBAR) {
		tree_resized(hotlist_tree);
		xwimp_force_redraw((wimp_w)hotlist_tree->handle,
				0,-16384, 16384, 16384);
	} else if (toolbar->type == THEME_HISTORY_TOOLBAR) {
		tree_resized(global_history_tree);
		xwimp_force_redraw((wimp_w)global_history_tree->handle,
				0,-16384, 16384, 16384);
	}
}


/**
 * Builds the URL suggestion menu
 */
bool ro_gui_menu_prepare_url_suggest(void) {
	char **suggest_text;
	int suggestions;
	int i;

	suggest_text = global_history_get_recent(&suggestions);
	if (suggestions < 1)
		return false;

	for (i = 0; i < suggestions; i++) {
		url_suggest_menu->entries[i].menu_flags = 0;
		url_suggest_menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		url_suggest_menu->entries[i].icon_flags =
				DEFAULT_FLAGS | wimp_ICON_INDIRECTED;
		url_suggest_menu->entries[i].data.indirected_text.text =
				suggest_text[i];
		url_suggest_menu->entries[i].data.indirected_text.validation =
				(char *)-1;
		url_suggest_menu->entries[i].data.indirected_text.size =
				strlen(suggest_text[i]) + 1;
	}

	url_suggest_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	url_suggest_menu->entries[suggestions - 1].menu_flags |= wimp_MENU_LAST;
	return true;
}


/**
 * Update navigate menu status and toolbar icons.
 *
 * /param gui  the gui_window to update
 */
void ro_gui_prepare_navigate(struct gui_window *gui) {
	int suggestions;

	ro_gui_menu_prepare_action(gui->window, HOTLIST_SHOW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_STOP, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_RELOAD_ALL, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_BACK, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_NAVIGATE_FORWARD, false);
	ro_gui_menu_prepare_action(gui->window, HOTLIST_SHOW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_SAVE, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_PRINT, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_SCALE_VIEW, false);
	ro_gui_menu_prepare_action(gui->window, BROWSER_FIND_TEXT, false);

	if (gui->toolbar) {
		global_history_get_recent(&suggestions);
		ro_gui_set_icon_shaded_state(gui->toolbar->toolbar_handle,
				ICON_TOOLBAR_SUGGEST, (suggestions <= 0));
	}
}


/**
 * Prepare the image quality menu for use
 *
 * \param tinct_options  the options to set the menu status for
 */
void ro_gui_menu_prepare_image_quality(unsigned int tinct_options) {
	for (int i = 0; i < 4; i++)
		image_quality_menu->entries[i].menu_flags &= ~wimp_MENU_TICKED;
	if (tinct_options & tinct_USE_OS_SPRITE_OP)
		image_quality_menu->entries[0].menu_flags |= wimp_MENU_TICKED;
	else if (tinct_options & tinct_ERROR_DIFFUSE)
		image_quality_menu->entries[3].menu_flags |= wimp_MENU_TICKED;
	else if (tinct_options & tinct_DITHER)
		image_quality_menu->entries[2].menu_flags |= wimp_MENU_TICKED;
	else
		image_quality_menu->entries[1].menu_flags |= wimp_MENU_TICKED;
}


/**
 * Prepare the page info window for use
 *
 * \param g  the gui_window to set the display icons for
 */
void ro_gui_menu_prepare_pageinfo(struct gui_window *g) {
	struct content *c = g->bw->current_content;
	char icon_buf[20] = "file_xxx";
	char enc_buf[40];
	char enc_token[10] = "Encoding0";
	const char *icon = icon_buf;
	const char *title = "-";
	const char *url = "-";
	const char *enc = "-";
	const char *mime = "-";

	assert(c);

	if (c->title)
		title = c->title;
	if (c->url)
		url = c->url;
	if (c->mime_type)
		mime = c->mime_type;

	sprintf(icon_buf, "file_%x", ro_content_filetype(c));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	if (c->type == CONTENT_HTML) {
		if (c->data.html.encoding) {
			enc_token[8] = '0' + c->data.html.encoding_source;
			snprintf(enc_buf, sizeof enc_buf, "%s (%s)",
					c->data.html.encoding,
					messages_get(enc_token));
			enc = enc_buf;
		} else {
			enc = messages_get("EncodingUnk");
		}
	}

	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ICON, icon);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TITLE, title);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_URL, url);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_ENC, enc);
	ro_gui_set_icon_string(dialog_pageinfo, ICON_PAGEINFO_TYPE, mime);
}


/**
 * Prepare the object info window for use
 *
 * \param box  the box to set the display icons for
 */
void ro_gui_menu_prepare_objectinfo(struct box *box) {
	char icon_buf[20] = "file_xxx";
	const char *url = "-";
	const char *target = "-";
	const char *mime = "-";

	sprintf(icon_buf, "file_%.3x",
			ro_content_filetype(box->object));
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		sprintf(icon_buf, "file_xxx");

	if (box->object->url)
		url = box->object->url;
	if (box->href)
		target = box->href;
	if (box->object->mime_type)
		mime = box->object->mime_type;

	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_ICON, icon_buf);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_URL, url);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TARGET, target);
	ro_gui_set_icon_string(dialog_objinfo, ICON_OBJINFO_TYPE, mime);
}


/**
 * Display a menu of options for a form select control.
 *
 * \param  bw	    browser window containing form control
 * \param  control  form control of type GADGET_SELECT
 */
void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control) {
	unsigned int i = 0, j;
	char *text_convert;
	struct form_option *option;
	wimp_pointer pointer;
	os_error *error;

	gui_form_select_control = NULL;
	for (option = control->data.select.items; option; option = option->next)
		i++;
	if (i == 0)
		return;

	if (gui_form_select_menu) {
		for (j = 0; ; j++) {
			free(gui_form_select_menu->entries[j].data.
					indirected_text.text);
			if (gui_form_select_menu->entries[j].menu_flags &
					wimp_MENU_LAST)
				break;
		}
		free(gui_form_select_menu);
		gui_form_select_menu = 0;
	}

	gui_form_select_menu = malloc(wimp_SIZEOF_MENU(i));
	if (!gui_form_select_menu) {
		warn_user("NoMemory", 0);
		ro_gui_menu_closed();
		return;
	}

	gui_form_select_menu->title_data.indirected_text.text =
			messages_get("SelectMenu");
	gui_form_select_menu->title_fg = wimp_COLOUR_BLACK;
	gui_form_select_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	gui_form_select_menu->work_fg = wimp_COLOUR_BLACK;
	gui_form_select_menu->work_bg = wimp_COLOUR_WHITE;
	gui_form_select_menu->width = 200;
	gui_form_select_menu->height = wimp_MENU_ITEM_HEIGHT;
	gui_form_select_menu->gap = wimp_MENU_ITEM_GAP;

	for (i = 0, option = control->data.select.items; option;
			i++, option = option->next) {
		gui_form_select_menu->entries[i].menu_flags = 0;
		if (option->selected)
			gui_form_select_menu->entries[i].menu_flags =
					wimp_MENU_TICKED;
		gui_form_select_menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		gui_form_select_menu->entries[i].icon_flags = wimp_ICON_TEXT |
				wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
				(wimp_COLOUR_BLACK <<
						wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE <<
						wimp_ICON_BG_COLOUR_SHIFT);
		/* \todo  can cnv_str_local_enc() fail? */
		gui_form_select_menu->entries[i].data.indirected_text.text =
				cnv_str_local_enc(option->text);
		/* convert spaces to hard spaces to stop things like 'Go Home'
		 * being treated as if 'Home' is a keyboard shortcut and right
		 * aligned in the menu. */
		text_convert = gui_form_select_menu->entries[i].
				data.indirected_text.text - 1;
		while (*++text_convert != '\0')
			if (*text_convert == 0x20)
				*text_convert = 0xa0;
		gui_form_select_menu->entries[i].data.indirected_text.
				validation = (char *)-1;
		gui_form_select_menu->entries[i].data.indirected_text.size =
				strlen(gui_form_select_menu->entries[i].
				data.indirected_text.text) + 1;
	}

	gui_form_select_menu->entries[0].menu_flags |=
			wimp_MENU_TITLE_INDIRECTED;
	gui_form_select_menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed();
		return;
	}

	gui_form_select_control = control;
	ro_gui_menu_create(gui_form_select_menu,
			pointer.pos.x, pointer.pos.y, bw->window->window);
}


/**
 * Creates a wimp_menu and adds it to the list to handle actions for.
 *
 * \param menu  the data to create the menu with
 * \return the menu created, or NULL on failure
 */
wimp_menu *ro_gui_menu_define_menu(struct ns_menu *menu) {
	struct menu_definition *definition;
	int entry;

	definition = calloc(sizeof(struct menu_definition), 1);
	if (!definition)
		die("No memory to create menu definition.");

	/* link in the menu to our list */
	definition->next = ro_gui_menu_definitions;
	ro_gui_menu_definitions = definition;

	/* create our definitions */
	for (entry = 0; menu->entries[entry].text; entry++);
	ro_gui_menu_define_menu_add(definition, menu, 0, NULL,
			0, entry, NULL, 0);
	return definition->menu;
}

void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		struct ns_menu *menu, int depth, wimp_menu_entry *link,
		int first, int last, const char *prefix, int prefix_length) {
	int entry, id, cur_depth, new_prefix_length;
	int entries = 0;
	int matches[last - first + 1];
	const char *match;
	char *text, *menu_text, *search;
	char *title, *translated;
	wimp_menu *new_menu;
	struct menu_definition_entry *definition_entry;

	/* step 1: store the matches for depth and subset string */
	for (entry = first; entry < last; entry++) {
		cur_depth = 0;
		match = menu->entries[entry].text;
		if ((prefix) && (strncmp(match, prefix, prefix_length)))
			continue;
		while (*match)
			if (*match++ == '.')
				cur_depth++;
		if (depth == cur_depth)
			matches[entries++] = entry;
	}
	matches[entries] = last;

	/* step 2: build and link the menu. we must use realloc to stop
	 * our memory fragmenting so we can test for sub-menus easily */
	if (entries == 0)
		return;
	new_menu = (wimp_menu *)malloc(wimp_SIZEOF_MENU(entries));
	if (!new_menu)
		die("No memory to create menu.");
	if (link) {
		title = link->data.indirected_text.text;
		link->sub_menu = new_menu;
	} else {
		title = messages_get(menu->title);
		if (!title)
			die("No memory to translate root menu title");
		definition->menu = new_menu;
	}
	new_menu->title_data.indirected_text.text = title;
	new_menu->title_fg = wimp_COLOUR_BLACK;
	new_menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	new_menu->work_fg = wimp_COLOUR_BLACK;
	new_menu->work_bg = wimp_COLOUR_WHITE;
	new_menu->width = 200;
	new_menu->height = wimp_MENU_ITEM_HEIGHT;
	new_menu->gap = wimp_MENU_ITEM_GAP;
	for (entry = 0; entry < entries; entry++) {
		/* add the entry */
		id = matches[entry];
		text = strdup(menu->entries[id].text);
		if (!text)
			die("No memory to examine menu text");
		search = menu_text = text;
		while (*search)
			if (*search++ == '.')
				menu_text = search;
		new_menu->entries[entry].menu_flags = 0;
		search = menu_text;
		while (*search)
			if (*search++ == '_') {
				new_menu->entries[entry].menu_flags |=
						wimp_MENU_SEPARATE;
				search[-1] = 0;
				break;
			}
		if (menu->entries[id].sub_window)
			new_menu->entries[entry].sub_menu = (wimp_menu *)menu->
					entries[id].sub_window;
		else
			new_menu->entries[entry].sub_menu = wimp_NO_SUB_MENU;
		new_menu->entries[entry].icon_flags = DEFAULT_FLAGS |
				wimp_ICON_INDIRECTED;
		translated = messages_get(menu_text);
		if (translated != menu_text)
		  	free(text);
		new_menu->entries[entry].data.indirected_text.text = translated;
		new_menu->entries[entry].data.indirected_text.validation =
				(char *)-1;
		new_menu->entries[entry].data.indirected_text.size =
				strlen(translated);

		/* store action */
		if (menu->entries[id].action != NO_ACTION) {
			definition_entry = malloc(
					sizeof(struct menu_definition_entry));
			if (!definition_entry)
				die("Unable to create menu definition entry");
			definition_entry->action = menu->entries[id].action;
			definition_entry->menu_entry =
					&new_menu->entries[entry];
			definition_entry->next = definition->entries;
			definition->entries = definition_entry;
		}

		/* recurse */
		if (new_menu->entries[entry].sub_menu == wimp_NO_SUB_MENU) {
			new_prefix_length = strlen(menu->entries[id].text);
			if (menu->entries[id].text[new_prefix_length - 1] == '_')
				new_prefix_length--;
			ro_gui_menu_define_menu_add(definition, menu, depth + 1,
					&new_menu->entries[entry],
					matches[entry], matches[entry + 1],
					menu->entries[id].text,
					new_prefix_length);
		}

		/* give menu warnings */
		if (new_menu->entries[entry].sub_menu != wimp_NO_SUB_MENU)
			new_menu->entries[entry].menu_flags |=
					wimp_MENU_GIVE_WARNING;
	}
	new_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	new_menu->entries[entries - 1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Finds the menu_definition corresponding to a wimp_menu.
 *
 * \param menu  the menu to find the definition for
 * \return the associated definition, or NULL if one could not be found
 */
struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu) {
	struct menu_definition *definition;

	if (!menu)
		return NULL;

	for (definition = ro_gui_menu_definitions; definition;
			definition = definition->next)
		if (definition->menu == menu)
			return definition;
	return NULL;
}


/**
 * Finds the menu_definition_entry corresponding to an action for a wimp_menu.
 *
 * \param menu	  the menu to search for an action within
 * \param action  the action to find
 * \return the associated menu entry, or NULL if one could not be found
 */
struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action) {
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NULL;

	for (entry = definition->entries; entry; entry = entry->next)
		if (entry->action == action)
			return entry;
	return NULL;
}


/**
 * Finds the action corresponding to a wimp_menu_entry for a wimp_menu.
 *
 * \param menu	      the menu to search for an action within
 * \param menu_entry  the menu_entry to find
 * \return the associated action, or 0 if one could not be found
 */
menu_action ro_gui_menu_find_action(wimp_menu *menu, wimp_menu_entry *menu_entry) {
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NO_ACTION;

	for (entry = definition->entries; entry; entry = entry->next) {
		if (entry->menu_entry == menu_entry)
			return entry->action;
	}
	return NO_ACTION;
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded) {
	struct menu_definition_entry *entry =
			ro_gui_menu_find_entry(menu, action);
	if (entry) {
		if (shaded)
			entry->menu_entry->icon_flags |= wimp_ICON_SHADED;
		else
			entry->menu_entry->icon_flags &= ~wimp_ICON_SHADED;
	}
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked) {
	struct menu_definition_entry *entry =
			ro_gui_menu_find_entry(menu, action);
	if (entry) {
		if (ticked)
			entry->menu_entry->menu_flags |= wimp_MENU_TICKED;
		else
			entry->menu_entry->menu_flags &= ~wimp_MENU_TICKED;
	}
}


/**
 * Handles an action.
 *
 * \param owner		      the window to handle the action for
 * \param action	      the action to handle
 * \param windows_at_pointer  whether to open any windows at the pointer location
 */
bool ro_gui_menu_handle_action(wimp_w owner, menu_action action,
		bool windows_at_pointer) {
	wimp_window_state state;
	struct gui_window *g = NULL;
	struct browser_window *bw = NULL;
	struct content *c = NULL;
	struct toolbar *t = NULL;
	struct tree *tree;
	struct node *node;
	os_error *error;
	char url[80];

	ro_gui_menu_get_window_details(owner, &g, &bw, &c, &t, &tree);

	switch (action) {

		/* help actions */
		case HELP_OPEN_CONTENTS:
			ro_gui_open_help_page("docs");
			return true;
		case HELP_OPEN_GUIDE:
			ro_gui_open_help_page("guide");
			return true;
		case HELP_OPEN_INFORMATION:
			ro_gui_open_help_page("info");
			return true;
		case HELP_OPEN_ABOUT:
			browser_window_create("file:/<NetSurf$Dir>/Docs/about",
					0, 0);
			return true;
		case HELP_LAUNCH_INTERACTIVE:
			ro_gui_interactive_help_start();
			return true;

		/* history actions */
		case HISTORY_SHOW_LOCAL:
			if ((!bw) || (!bw->history))
				return false;
			ro_gui_history_open(bw, bw->history, windows_at_pointer);
			return true;
		case HISTORY_SHOW_GLOBAL:
			ro_gui_tree_show(global_history_tree);
			return true;

		/* hotlist actions */
		case HOTLIST_ADD_URL:
			if ((!hotlist_tree) || (!c))
				return false;
			node = tree_create_URL_node(hotlist_tree->root,
					c->title, c->url, ro_content_filetype(c),
					time(NULL), -1, 0);
			if (node) {
				tree_redraw_area(hotlist_tree,
						node->box.x - NODE_INSTEP, 0,
						NODE_INSTEP, 16384);
				tree_handle_node_changed(hotlist_tree, node,
						false, true);
				ro_gui_tree_scroll_visible(hotlist_tree,
						&node->data);
			}
			return true;
		case HOTLIST_SHOW:
			ro_gui_tree_show(hotlist_tree);
			return true;

		/* page actions */
		case BROWSER_PAGE_INFO:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(g->window, dialog_pageinfo,
					windows_at_pointer);
			return true;
		case BROWSER_PRINT:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(g->window, dialog_print,
					windows_at_pointer);
			return true;
		case BROWSER_NEW_WINDOW:
			if (!c)
				return false;
			browser_window_create(c->url, bw, 0);
			return true;
		case BROWSER_VIEW_SOURCE:
			if (!c)
				return false;
			ro_gui_view_source(c);
			return true;

		/* object actions */
		case BROWSER_OBJECT_INFO:
			if (!current_menu_object_box)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(g->window, dialog_objinfo,
					windows_at_pointer);
			return true;
		case BROWSER_OBJECT_RELOAD:
			if (!current_menu_object_box)
				return false;
			current_menu_object_box->object->fresh = false;
			browser_window_reload(bw, false);
			return true;

		/* save actions */
		case BROWSER_OBJECT_SAVE:
		case BROWSER_OBJECT_EXPORT_SPRITE:
		case BROWSER_OBJECT_SAVE_URL_URI:
		case BROWSER_OBJECT_SAVE_URL_URL:
		case BROWSER_OBJECT_SAVE_URL_TEXT:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
		case BROWSER_SAVE:
		case BROWSER_SAVE_COMPLETE:
		case BROWSER_EXPORT_DRAW:
		case BROWSER_EXPORT_TEXT:
		case BROWSER_SAVE_URL_URI:
		case BROWSER_SAVE_URL_URL:
		case BROWSER_SAVE_URL_TEXT:
			if (!c)
				return false;
		case HOTLIST_EXPORT:
		case HISTORY_EXPORT:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(owner, dialog_saveas,
					windows_at_pointer);
			return true;

		/* navigation actions */
		case BROWSER_NAVIGATE_HOME:
			if (!bw)
				return false;
			if ((option_homepage_url) && (option_homepage_url[0])) {
				browser_window_go(g->bw, option_homepage_url, 0);
			} else {
				snprintf(url, sizeof url,
						"file:/<NetSurf$Dir>/Docs/intro_%s",
						option_language);
				browser_window_go(g->bw, url, 0);
			}
			return true;
		case BROWSER_NAVIGATE_BACK:
			if ((!bw) || (!bw->history))
				return false;
			history_back(bw, bw->history);
			return true;
		case BROWSER_NAVIGATE_FORWARD:
			if ((!bw) || (!bw->history))
				return false;
			history_forward(bw, bw->history);
			return true;
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
			if (!bw)
				return false;
			browser_window_reload(bw,
					(action == BROWSER_NAVIGATE_RELOAD_ALL));
			return true;
		case BROWSER_NAVIGATE_STOP:
			if (!bw)
				return false;
			browser_window_stop(bw);
			return true;
		case BROWSER_NAVIGATE_URL:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(NULL, dialog_openurl,
					windows_at_pointer);
			return true;

		/* browser window/display actions */
		case BROWSER_SCALE_VIEW:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(g->window, dialog_zoom,
					windows_at_pointer);
			return true;
		case BROWSER_FIND_TEXT:
			if (!c)
				return false;
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant(g->window, dialog_search,
					windows_at_pointer);
			return true;
		case BROWSER_IMAGES_BACKGROUND:
			if (!g)
				return false;
			g->option.background_images = !g->option.background_images;
			gui_window_redraw_window(g);
			return true;
		case BROWSER_BUFFER_ANIMS:
			if (!g)
				return false;
			g->option.buffer_animations = !g->option.buffer_animations;
			break;
		case BROWSER_BUFFER_ALL:
			if (!g)
				return false;
			g->option.buffer_everything = !g->option.buffer_everything;
			break;
		case BROWSER_SAVE_VIEW:
			if (!bw)
				return false;
			ro_gui_window_default_options(bw);
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_DEFAULT:
			if (!g)
				return false;
			ro_gui_screen_size(&option_window_screen_width,
					&option_window_screen_height);
			state.w = current_menu_window;
			error = xwimp_get_window_state(&state);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
			option_window_x = state.visible.x0;
			option_window_y = state.visible.y0;
			option_window_width = state.visible.x1 - state.visible.x0;
			option_window_height = state.visible.y1 - state.visible.y0;
			return true;
		case BROWSER_WINDOW_STAGGER:
			option_window_stagger = !option_window_stagger;
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_COPY:
			option_window_size_clone = !option_window_size_clone;
			ro_gui_save_options();
			return true;
		case BROWSER_WINDOW_RESET:
			option_window_screen_width = 0;
			option_window_screen_height = 0;
			ro_gui_save_options();
			return true;

		/* tree actions */
		case TREE_NEW_FOLDER:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant((wimp_w)tree->handle,
					dialog_folder, windows_at_pointer);
			return true;
		case TREE_NEW_LINK:
			ro_gui_menu_prepare_action(owner, action, true);
			ro_gui_dialog_open_persistant((wimp_w)tree->handle,
					dialog_entry, windows_at_pointer);
			return true;
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
			tree_handle_expansion(tree, tree->root, true,
					(action != TREE_EXPAND_LINKS),
					(action != TREE_EXPAND_FOLDERS));
			return true;
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
			tree_handle_expansion(tree, tree->root, false,
					(action != TREE_COLLAPSE_LINKS),
					(action != TREE_COLLAPSE_FOLDERS));
			return true;
		case TREE_SELECTION_EDIT:
			return true;
		case TREE_SELECTION_LAUNCH:
			ro_gui_tree_launch_selected(tree);
			return true;
		case TREE_SELECTION_DELETE:
			tree_delete_selected_nodes(tree, tree->root);
			return true;
		case TREE_SELECTION_RESET:
			tree_reset_URL_nodes(tree, tree->root, true);
			return true;
		case TREE_SELECT_ALL:
			ro_gui_tree_keypress(1, tree); /* CTRL-A */
			return true;
		case TREE_CLEAR_SELECTION:
			ro_gui_tree_keypress(26, tree); /* CTRL-Z */
			return true;

		/* toolbar actions */
		case TOOLBAR_BUTTONS:
			assert(t);
			t->display_buttons = !t->display_buttons;
			ro_gui_menu_refresh_toolbar(t);
			return true;
		case TOOLBAR_ADDRESS_BAR:
			assert(t);
			t->display_url = !t->display_url;
			ro_gui_menu_refresh_toolbar(t);
			if (t->display_url)
				ro_gui_set_caret_first(t->toolbar_handle);
			return true;
		case TOOLBAR_THROBBER:
			assert(t);
			t->display_throbber = !t->display_throbber;
			ro_gui_menu_refresh_toolbar(t);
			return true;
		case TOOLBAR_STATUS_BAR:
			assert(t);
			t->display_status = !t->display_status;
			ro_gui_menu_refresh_toolbar(t);
			return true;
		case TOOLBAR_EDIT:
			assert(t);
			ro_gui_theme_toggle_edit(t);
			return true;

		/* misc actions */
		case APPLICATION_QUIT:
			if (ro_gui_prequit())
				netsurf_quit = true;
			return true;
		case CHOICES_SHOW:
			ro_gui_dialog_open_config();
			return true;

		/* unknown action */
		default:
			return false;
	}
	return false;
}


/**
 * Prepares an action for use.
 *
 * \param owner	   the window to prepare the action for
 * \param action   the action to prepare
 * \param windows  whether to update sub-windows
 */
void ro_gui_menu_prepare_action(wimp_w owner, menu_action action, bool windows) {
	struct menu_definition_entry *entry;
	struct gui_window *g;
	struct browser_window *bw;
	struct content *c;
	struct toolbar *t;
	struct tree *tree;
	struct node *node;
	bool result = false;
	int checksum = 0;
	os_error *error;

	ro_gui_menu_get_window_details(owner, &g, &bw, &c, &t, &tree);
	if (current_menu_open)
		checksum = ro_gui_menu_get_checksum();

	switch (action) {

		/* help actions */
		case HELP_LAUNCH_INTERACTIVE:
			result = ro_gui_interactive_help_available();
			ro_gui_menu_set_entry_shaded(current_menu, action, result);
			ro_gui_menu_set_entry_ticked(current_menu, action, result);
			break;

		/* history actions */
		case HISTORY_SHOW_LOCAL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				(!bw || (!bw->history) ||
				!(c || history_back_available(bw->history) ||
				history_forward_available(bw->history))));
			break;
		case HISTORY_SHOW_GLOBAL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!global_history_tree);
			break;

		/* hotlist actions */
		case HOTLIST_ADD_URL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				(!c || !hotlist_tree));
			break;
		case HOTLIST_SHOW:
			ro_gui_menu_set_entry_shaded(current_menu, action,
				!hotlist_tree);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_BOOKMARK, !hotlist_tree);
			break;

		/* page actions */
		case BROWSER_PAGE_INFO:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((windows) && (c))
				ro_gui_menu_prepare_pageinfo(g);
			break;
		case BROWSER_PRINT:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((t) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_PRINT, !c);
			if ((windows) && (c))
				ro_gui_print_prepare(g);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_PRINT, !c);
			break;
		case BROWSER_PAGE:
		case BROWSER_NEW_WINDOW:
		case BROWSER_VIEW_SOURCE:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			break;

		/* object actions */
		case BROWSER_OBJECT:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			break;
		case BROWSER_OBJECT_INFO:
			if ((windows) && (current_menu_object_box))
				ro_gui_menu_prepare_objectinfo(
						current_menu_object_box);
		case BROWSER_OBJECT_RELOAD:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!current_menu_object_box);
			break;

		/* save actions (browser, hotlist, history) */
		case BROWSER_OBJECT_SAVE:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_OBJECT_ORIG, c);
			break;
		case BROWSER_OBJECT_EXPORT_SPRITE:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_OBJECT_NATIVE, c);
			break;
		case BROWSER_SAVE:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_SOURCE, c);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_SAVE, !c);
			break;
		case BROWSER_SAVE_COMPLETE:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_COMPLETE, c);
			break;
		case BROWSER_EXPORT_DRAW:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_DRAW, c);
			break;
		case BROWSER_EXPORT_TEXT:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_TEXT, c);
			break;
		case BROWSER_OBJECT_SAVE_URL_URI:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
		case BROWSER_SAVE_URL_URI:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_URI, c);
			break;
		case BROWSER_OBJECT_SAVE_URL_URL:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
		case BROWSER_SAVE_URL_URL:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_URL, c);
			break;
		case BROWSER_OBJECT_SAVE_URL_TEXT:
			c = current_menu_object_box ?
				current_menu_object_box->object : NULL;
		case BROWSER_SAVE_URL_TEXT:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_save_prepare(GUI_SAVE_LINK_TEXT, c);
			break;
		case HOTLIST_EXPORT:
			if ((tree) && (windows))
				ro_gui_save_prepare(GUI_SAVE_HOTLIST_EXPORT_HTML,
						NULL);
			break;
		case HISTORY_EXPORT:
			if ((tree) && (windows))
				ro_gui_save_prepare(GUI_SAVE_HISTORY_EXPORT_HTML,
						NULL);
			break;

		/* navigation actions */
		case BROWSER_NAVIGATE_BACK:
			result = (!bw || !bw->history ||
					!history_back_available(bw->history));
			ro_gui_menu_set_entry_shaded(current_menu, action, result);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_BACK, result);
			break;
		case BROWSER_NAVIGATE_FORWARD:
			result = (!bw || !bw->history ||
					!history_forward_available(bw->history));
			ro_gui_menu_set_entry_shaded(current_menu, action, result);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_FORWARD, result);
			break;
		case BROWSER_NAVIGATE_RELOAD:
		case BROWSER_NAVIGATE_RELOAD_ALL:
			result = (bw->current_content && !bw->loading_content);
			ro_gui_menu_set_entry_shaded(current_menu, action, !result);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_RELOAD, !result);
			break;
		case BROWSER_NAVIGATE_STOP:
			result = (bw->loading_content || (bw->current_content &&
					(bw->current_content->status !=
						CONTENT_STATUS_DONE)));
			ro_gui_menu_set_entry_shaded(current_menu, action, !result);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_STOP, !result);
			break;
		case BROWSER_NAVIGATE_URL:
			if (windows)
				ro_gui_dialog_prepare_open_url();
			break;

		/* display actions */
		case BROWSER_SCALE_VIEW:
			ro_gui_menu_set_entry_shaded(current_menu, action, !c);
			if ((c) && (windows))
				ro_gui_dialog_prepare_zoom(g);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_SCALE, !c);
			break;
		case BROWSER_FIND_TEXT:
			if ((c) && (windows))
				ro_gui_search_prepare(g);
			if ((t) && (!t->editor) && (t->type == THEME_BROWSER_TOOLBAR))
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_SEARCH, !c);
			break;
		case BROWSER_IMAGES_FOREGROUND:
			ro_gui_menu_set_entry_shaded(current_menu, action, true);
			ro_gui_menu_set_entry_ticked(current_menu, action, true);
			break;
		case BROWSER_IMAGES_BACKGROUND:
			if (g)
				ro_gui_menu_set_entry_ticked(current_menu, action,
					g->option.background_images);
			break;
		case BROWSER_BUFFER_ANIMS:
			if (g) {
				ro_gui_menu_set_entry_shaded(current_menu, action,
					g->option.buffer_everything);
				ro_gui_menu_set_entry_ticked(current_menu, action,
					g->option.buffer_animations ||
					g->option.buffer_everything);
			}
			break;
		case BROWSER_BUFFER_ALL:
			if (g)
				ro_gui_menu_set_entry_ticked(current_menu, action,
					g->option.buffer_everything);
			break;
		case BROWSER_WINDOW_STAGGER:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					option_window_screen_width == 0);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					((option_window_screen_width == 0) ||
					option_window_stagger));
			break;
		case BROWSER_WINDOW_COPY:
			ro_gui_menu_set_entry_ticked(current_menu, action,
					option_window_size_clone);
			break;
		case BROWSER_WINDOW_RESET:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					option_window_screen_width == 0);
			break;

		/* tree actions */
		case TREE_NEW_FOLDER:
			ro_gui_hotlist_prepare_folder_dialog(NULL);
			break;
		case TREE_NEW_LINK:
			ro_gui_hotlist_prepare_entry_dialog(NULL);
			break;
		case TREE_EXPAND_ALL:
		case TREE_EXPAND_FOLDERS:
		case TREE_EXPAND_LINKS:
		case TREE_COLLAPSE_ALL:
		case TREE_COLLAPSE_FOLDERS:
		case TREE_COLLAPSE_LINKS:
			if ((tree) && (tree->root))
				ro_gui_menu_set_entry_shaded(current_menu, action,
						!tree->root->child);
			break;
		case TREE_SELECTION:
			if ((!tree) || (!tree->root))
				break;
			if (tree->root->child)
				result = tree_has_selection(tree->root->child);
			ro_gui_menu_set_entry_shaded(current_menu, action, !result);
			if ((t) && (!t->editor) && (t->type != THEME_BROWSER_TOOLBAR)) {
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_DELETE, !result);
				ro_gui_set_icon_shaded_state(t->toolbar_handle,
						ICON_TOOLBAR_LAUNCH, !result);
			}
			break;
		case TREE_SELECTION_EDIT:
			node = tree_get_selected_node(tree->root);
			entry = ro_gui_menu_find_entry(current_menu, action);
			if ((!node) || (!entry))
				break;
			if (node->folder) {
				entry->menu_entry->sub_menu =
						(wimp_menu *)dialog_folder;
				if (windows)
					ro_gui_hotlist_prepare_folder_dialog(node);
			} else {
				entry->menu_entry->sub_menu =
						(wimp_menu *)dialog_entry;
				if (windows)
					ro_gui_hotlist_prepare_entry_dialog(node);
			}
			break;
		case TREE_SELECTION_LAUNCH:
		case TREE_SELECTION_DELETE:
		case TREE_SELECTION_RESET:
			if ((!tree) || (!tree->root))
				break;
			if (tree->root->child)
				result = tree_has_selection(tree->root->child);
			ro_gui_menu_set_entry_shaded(current_menu, action, !result);
			break;
		case TREE_SELECT_ALL:
			ro_gui_menu_set_entry_shaded(current_menu, action,
					!tree->root->child);
			break;
		case TREE_CLEAR_SELECTION:
			if ((!tree) || (!tree->root))
				break;
			if (tree->root->child)
				result = tree_has_selection(tree->root->child);
			ro_gui_menu_set_entry_shaded(current_menu, action, !result);
			break;

		/* toolbar actions */
		case TOOLBAR_BUTTONS:
			ro_gui_menu_set_entry_shaded(current_menu, action, (!t ||
					(t->editor)));
			ro_gui_menu_set_entry_ticked(current_menu, action, (t &&
					((t->display_buttons) || (t->editor))));
			break;
		case TOOLBAR_ADDRESS_BAR:
			ro_gui_menu_set_entry_shaded(current_menu, action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->display_url));
			break;
		case TOOLBAR_THROBBER:
			ro_gui_menu_set_entry_shaded(current_menu, action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->display_throbber));
			break;
		case TOOLBAR_STATUS_BAR:
			ro_gui_menu_set_entry_shaded(current_menu, action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->display_status));
			break;
		case TOOLBAR_EDIT:
			ro_gui_menu_set_entry_shaded(current_menu, action, !t);
			ro_gui_menu_set_entry_ticked(current_menu, action,
					(t && t->editor));
			break;

		/* unknown action */
		default:
			return;
	}

	/* update open menus */
	if ((current_menu_open) &&
			(checksum != ro_gui_menu_get_checksum())) {
		error = xwimp_create_menu(current_menu, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
	}
}


/**
 * Gets various details relating to a window
 *
 * \param w  the window to complete information for
 */
void ro_gui_menu_get_window_details(wimp_w w, struct gui_window **g,
		struct browser_window **bw, struct content **content,
		struct toolbar **toolbar, struct tree **tree) {
	*g = ro_gui_window_lookup(w);
	if (*g) {
		*bw = (*g)->bw;
		*toolbar = (*g)->toolbar;
		if (*bw)
			*content = (*bw)->current_content;
		*tree = NULL;
	} else {
		*bw = NULL;
		*content = NULL;
		if ((hotlist_tree) && (w == (wimp_w)hotlist_tree->handle))
			*tree = hotlist_tree;
		else if ((global_history_tree) &&
				(w == (wimp_w)global_history_tree->handle))
			*tree = global_history_tree;
		else
			*tree = NULL;
		if (*tree)
			*toolbar = (*tree)->toolbar;
		else
			*toolbar = NULL;
	}
}


/**
 * Calculates a simple checksum for the current menu state
 */
int ro_gui_menu_get_checksum(void) {
	wimp_selection menu_tree;
	int i = 0, j, checksum = 0;
	os_error *error;
	wimp_menu *menu;

	if (!current_menu_open)
		return 0;

	error = xwimp_get_menu_state((wimp_menu_state_flags)0,
			&menu_tree, 0, 0);
	if (error) {
		LOG(("xwimp_get_menu_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return 0;
	}

	menu = current_menu;
	do {
		j = 0;
		do {
			if (menu->entries[j].icon_flags & wimp_ICON_SHADED)
				checksum ^= (1 << (i + j * 2));
			if (menu->entries[j].menu_flags & wimp_MENU_TICKED)
				checksum ^= (2 << (i + j * 2));
		} while (!(menu->entries[j++].menu_flags & wimp_MENU_LAST));
		j = menu_tree.items[i++];
		if (j != -1)
			menu = menu->entries[j].sub_menu;
	} while (j != -1);
	return checksum;
}
