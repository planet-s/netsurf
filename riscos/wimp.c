/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * General RISC OS WIMP/OS library functions (implementation).
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "netsurf/riscos/wimp.h"


/*	Wimp_Extend,11 block
*/
static wimpextend_furniture_sizes furniture_sizes;

/**
 * Gets the default horzontal scrollbar height
 */
int ro_get_hscroll_height(wimp_w w) {
  	furniture_sizes.w = w;
	furniture_sizes.border_widths.y0 = 38;
	xwimpextend_get_furniture_sizes(&furniture_sizes);
	return furniture_sizes.border_widths.y0;
}

/**
 * Reads a modes EIG factors.
 *
 * \param  mode  mode to read EIG factors for, or -1 for current
 */
struct eig_factors ro_read_eig_factors(os_mode mode) {
	bits psr;
	struct eig_factors factors;
	xos_read_mode_variable(mode, os_MODEVAR_XEIG_FACTOR, &factors.xeig, &psr);
	xos_read_mode_variable(mode, os_MODEVAR_YEIG_FACTOR, &factors.yeig, &psr);
	return factors;
}


/**
 * Converts the supplied os_coord from OS units to pixels.
 *
 * \param  os_units  values to convert
 * \param  mode      mode to use EIG factors for, or -1 for current
 */
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode) {
	struct eig_factors factors = ro_read_eig_factors(mode);
	os_units->x = (os_units->x >> factors.xeig);
	os_units->y = (os_units->y >> factors.yeig);
}


/**
 * Converts the supplied os_coord from pixels to OS units.
 *
 * \param  pixels  values to convert
 * \param  mode    mode to use EIG factors for, or -1 for current
 */
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode) {
	struct eig_factors factors = ro_read_eig_factors(mode);
	pixels->x = (pixels->x << factors.xeig);
	pixels->y = (pixels->y << factors.yeig);
}


/**
 * Redraws an icon
 *
 * \param  w  window handle
 * \param  i  icon handle
 */
#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)


/**
 * Read the contents of an icon.
 *
 * \param  w  window handle
 * \param  i  icon handle
 * \return string in icon
 */
char *ro_gui_get_icon_string(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	if (xwimp_get_icon_state(&ic)) return NULL;
	return ic.icon.data.indirected_text.text;
}


/**
 * Set the contents of an icon to a string.
 *
 * \param  w     window handle
 * \param  i     icon handle
 * \param  text  string (copied)
 */
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text) {
  	wimp_caret caret;
	wimp_icon_state ic;
	int old_len, len;
	
	/*	Get the icon data
	*/
	ic.w = w;
	ic.i = i;
	if (xwimp_get_icon_state(&ic)) return;
	
	/*	Check that the existing text is not the same as the updated text
		to stop flicker
	*/
	if (!strncmp(ic.icon.data.indirected_text.text, text,
			(unsigned int)ic.icon.data.indirected_text.size)) return;
	
	/*	Copy the text across
	*/
	old_len = strlen(ic.icon.data.indirected_text.text);
	strncpy(ic.icon.data.indirected_text.text, text,
			(unsigned int)ic.icon.data.indirected_text.size);
					
	/*	Handle the caret being in the icon
	*/
	if (!xwimp_get_caret_position(&caret)) {
		if ((caret.w == w) && (caret.i == i)) {
		  	len = strlen(text);
		  	if ((caret.index > len) || (caret.index == old_len)) caret.index = len;
			xwimp_set_caret_position(w, i, caret.pos.x, caret.pos.y, -1, caret.index);
		}
	}
	
	/*	Redraw the icon
	*/
	ro_gui_redraw_icon(w, i);
}


/**
 * Set the contents of an icon to a number.
 *
 * \param  w      window handle
 * \param  i      icon handle
 * \param  value  value
 */
void ro_gui_set_icon_integer(wimp_w w, wimp_i i, int value) {
	char buffer[20]; // Big enough for 64-bit int
	sprintf(buffer, "%d", value);
	ro_gui_set_icon_string(w, i, buffer);
}


/**
 * Set the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 * \param  state selected state
 */
#define ro_gui_set_icon_selected_state(w, i, state) xwimp_set_icon_state(w, i, (state ? wimp_ICON_SELECTED : 0), wimp_ICON_SELECTED)


/**
 * Gets the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 */
int ro_gui_get_icon_selected_state(wimp_w w, wimp_i i) {
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SELECTED) != 0;
}


/**
 * Set the selected state of an icon.
 *
 * \param  w     window handle
 * \param  i     icon handle
 * \param  state selected state
 */
#define ro_gui_set_icon_shaded_state(w, i, state) xwimp_set_icon_state(w, i, (state ? wimp_ICON_SHADED : 0), wimp_ICON_SHADED)
