/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <windows.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/corestrings.h"
#include "utils/url.h"
#include "utils/file.h"
#include "desktop/browser.h"
#include "desktop/gui_clipboard.h"

#include "windows/schedule.h"
#include "windows/window.h"
#include "windows/filetype.h"
#include "windows/gui.h"

static bool win32_quit = false;

HINSTANCE hInstance; /** win32 application instance handle. */


void win32_set_quit(bool q)
{
	win32_quit = q;
}

/* exported interface documented in gui.h */
void win32_run(void)
{
	MSG Msg; /* message from system */
	BOOL bRet; /* message fetch result */
	int timeout; /* timeout in miliseconds */
	UINT timer_id = 0;

	while (!win32_quit) {
		/* run the scheduler and discover how long to wait for
		 * the next event.
		 */
		timeout = schedule_run();

		if (timeout == 0) {
			bRet = PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);
		} else {
			if (timeout > 0) {
				/* set up a timer to ensure we get woken */
				timer_id = SetTimer(NULL, 0, timeout, NULL);
			}

			/* wait for a message */
			bRet = GetMessage(&Msg, NULL, 0, 0);

			/* if a timer was sucessfully created remove it */
			if (timer_id != 0) {
				KillTimer(NULL, timer_id);
				timer_id = 0;
			}
		}

		if (bRet > 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}
}






/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	/* TODO: Implement this */
	HANDLE clipboard_handle;
	char *content;

	clipboard_handle = GetClipboardData(CF_TEXT);
	if (clipboard_handle != NULL) {
		content = GlobalLock(clipboard_handle);
		LOG(("pasting %s", content));
		GlobalUnlock(clipboard_handle);
	}
}


/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	/* TODO: Implement this */
	HANDLE hnew;
	char *new, *original;
	HANDLE h = GetClipboardData(CF_TEXT);
	if (h == NULL)
		original = (char *)"";
	else
		original = GlobalLock(h);

	size_t len = strlen(original) + 1;
	hnew = GlobalAlloc(GHND, length + len);
	new = (char *)GlobalLock(hnew);
	snprintf(new, length + len, "%s%s", original, buffer);

	if (h != NULL) {
		GlobalUnlock(h);
		EmptyClipboard();
	}
	GlobalUnlock(hnew);
	SetClipboardData(CF_TEXT, hnew);
}


/**
 * Generate a windows path from one or more component elemnts.
 *
 * If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] nelm The number of elements.
 * @param[in] ap The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror windows_mkpath(char **str, size_t *size, size_t nelm, va_list ap)
{
	return vsnstrjoin(str, size, '\\', nelm, ap);
}


/**
 * Get the basename of a file using windows path handling.
 *
 * This gets the last element of a path and returns it.
 *
 * @param[in] path The path to extract the name from.
 * @param[in,out] str Pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the path element.
 * @param[in,out] size The size of the space available if \a
 *                     str not NULL on input and set to the total
 *                     output length on output.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror windows_basename(const char *path, char **str, size_t *size)
{
	const char *leafname;
	char *fname;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	leafname = strrchr(path, '\\');
	if (!leafname) {
		leafname = path;
	} else {
		leafname += 1;
	}

	fname = strdup(leafname);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}

	*str = fname;
	if (size != NULL) {
		*size = strlen(fname);
	}
	return NSERROR_OK;
}


/**
 * Create a path from a nsurl using windows file handling.
 *
 * @param[in] url The url to encode.
 * @param[out] path_out A string containing the result path which should
 *                      be freed by the caller.
 * @return NSERROR_OK and the path is written to \a path or error code
 *         on faliure.
 */
static nserror windows_nsurl_to_path(struct nsurl *url, char **path_out)
{
	lwc_string *urlpath;
	char *path;
	bool match;
	lwc_string *scheme;
	nserror res;

	if ((url == NULL) || (path_out == NULL)) {
		return NSERROR_BAD_PARAMETER;
	}

	scheme = nsurl_get_component(url, NSURL_SCHEME);

	if (lwc_string_caseless_isequal(scheme, corestring_lwc_file,
					&match) != lwc_error_ok)
	{
		return NSERROR_BAD_PARAMETER;
	}
	lwc_string_unref(scheme);
	if (match == false) {
		return NSERROR_BAD_PARAMETER;
	}

	urlpath = nsurl_get_component(url, NSURL_PATH);
	if (urlpath == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	res = url_unescape(lwc_string_data(urlpath), &path);
	lwc_string_unref(urlpath);
	if (res != NSERROR_OK) {
		return res;
	}

	/* if there is a drive: prefix treat path as DOS filename */
	if ((path[2] == ':') ||  (path[2] == '|')) {
		char *sidx; /* slash index */

		/* move the string down to remove leading / note the
		 * strlen is *not* copying too much data as we are
		 * moving the null too!
		 */
		memmove(path, path + 1, strlen(path));

		/* swap / for \ */
		sidx = strrchr(path, '/');
		while (sidx != NULL) {
			*sidx = '\\';
			sidx = strrchr(path, '/');
		}
	}
	/* if the path does not have a drive letter we return the
	 * complete path.
	 */
	/** @todo Need to check returning the unaltered path in this
	 * case is correct
	 */

	*path_out = path;

	return NSERROR_OK;
}


/**
 * Create a nsurl from a path using windows file handling.
 *
 * Perform the necessary operations on a path to generate a nsurl.
 *
 * @param[in] path The path to convert.
 * @param[out] url_out pointer to recive the nsurl, The returned url
 *                     should be unreferenced by the caller.
 * @return NSERROR_OK and the url is placed in \a url or error code on
 *         faliure.
 */
static nserror windows_path_to_nsurl(const char *path, struct nsurl **url_out)
{
	nserror ret;
	int urllen;
	char *urlstr;
	char *sidx; /* slash index */

	if ((path == NULL) || (url_out == NULL) || (*path == 0)) {
		return NSERROR_BAD_PARAMETER;
	}

	/* build url as a string for nsurl constructor */
	urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 5;
	urlstr = malloc(urllen);
	if (urlstr == NULL) {
		return NSERROR_NOMEM;
	}

	/** @todo check if this should be url escaping the path. */
	if (*path == '/') {
		/* unix style path start, so try wine Z: */
		snprintf(urlstr, urllen, "%sZ%%3A%s", FILE_SCHEME_PREFIX, path);
	} else {
		snprintf(urlstr, urllen, "%s%s", FILE_SCHEME_PREFIX, path);
	}

	sidx = strrchr(urlstr, '\\');
	while (sidx != NULL) {
		*sidx = '/';
		sidx = strrchr(urlstr, '\\');
	}

	ret = nsurl_create(urlstr, url_out);
	free(urlstr);

	return ret;
}


/**
 * Ensure that all directory elements needed to store a filename exist.
 *
 * @param fname The filename to ensure the path to exists.
 * @return NSERROR_OK on success or error code on failure.
 */
static nserror windows_mkdir_all(const char *fname)
{
	char *dname;
	char *sep;
	struct stat sb;

	dname = strdup(fname);

	sep = strrchr(dname, '\\');
	if (sep == NULL) {
		/* no directory separator path is just filename so its ok */
		free(dname);
		return NSERROR_OK;
	}

	*sep = 0; /* null terminate directory path */

	if (stat(dname, &sb) == 0) {
		free(dname);
		if (S_ISDIR(sb.st_mode)) {
			/* path to file exists and is a directory */
			return NSERROR_OK;
		}
		return NSERROR_NOT_DIRECTORY;
	}
	*sep = '\\'; /* restore separator */

	sep = dname;
	while (*sep == '\\') {
		sep++;
	}
	while ((sep = strchr(sep, '\\')) != NULL) {
		*sep = 0;
		if (stat(dname, &sb) != 0) {
			if (nsmkdir(dname, S_IRWXU) != 0) {
				/* could not create path element */
				free(dname);
				return NSERROR_NOT_FOUND;
			}
		} else {
			if (! S_ISDIR(sb.st_mode)) {
				/* path element not a directory */
				free(dname);
				return NSERROR_NOT_DIRECTORY;
			}
		}
		*sep = '\\'; /* restore separator */
		/* skip directory separators */
		while (*sep == '\\') {
			sep++;
		}
	}

	free(dname);
	return NSERROR_OK;
}

/* windows file handling */
static struct gui_file_table file_table = {
	.mkpath = windows_mkpath,
	.basename = windows_basename,
	.nsurl_to_path = windows_nsurl_to_path,
	.path_to_nsurl = windows_path_to_nsurl,
	.mkdir_all = windows_mkdir_all,
};

struct gui_file_table *win32_file_table = &file_table;


static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *win32_clipboard_table = &clipboard_table;


