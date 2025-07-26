/**************************************************************************/
/*  file_access_windows.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

// file_access_windows.cpp (refatorado parcialmente para clareza e modularidade)

#ifdef WINDOWS_ENABLED

#include "file_access_windows.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#include <share.h> // _SH_DENYNO
#include <shlwapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tchar.h>
#include <cerrno>
#include <cwchar>

#ifdef _MSC_VER
#define S_ISREG(m) ((m) & _S_IFREG)
#endif

// Helpers para clareza
const WCHAR *FileAccessWindows::_mode_string_from_flags(int flags) const {
	static const WCHAR *modes[] = {L"rb", L"wb", L"rb+", L"wb+"};
	if (flags >= 0 && flags <= WRITE_READ) {
		return modes[flags];
	}
	return nullptr;
}

int FileAccessWindows::_share_mode_from_flags(int flags) const {
	if (!is_backup_save_enabled()) return _SH_DENYNO;
	if (flags == READ) return _SH_DENYWR;
	return _SH_DENYRW;
}

bool FileAccessWindows::_is_directory(const String &p_path) {
	DWORD file_attr = GetFileAttributesW((LPCWSTR)(p_path.utf16().get_data()));
	return (file_attr != INVALID_FILE_ATTRIBUTES && (file_attr & FILE_ATTRIBUTE_DIRECTORY));
}

String FileAccessWindows::_create_temp_file_in_place(String &r_save_path) {
	r_save_path = path;
	uint64_t id = OS::get_singleton()->get_ticks_usec();
	String tmpfile;
	while (true) {
		tmpfile = path + itos(id++) + ".tmp";
		HANDLE handle = CreateFileW((LPCWSTR)tmpfile.utf16().get_data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle != INVALID_HANDLE_VALUE) {
			CloseHandle(handle);
			return tmpfile;
		}
		if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_SHARING_VIOLATION) {
			return String(); // erro
		}
	}
}

Error FileAccessWindows::open_internal(const String &p_path, int p_mode_flags) {
	if (is_path_invalid(p_path)) {
#ifdef DEBUG_ENABLED
		if (p_mode_flags != READ) {
			WARN_PRINT("Path is reserved Windows system pipe: " + p_path);
		}
#endif
		return ERR_INVALID_PARAMETER;
	}

	_close();
	path_src = p_path;
	path = fix_path(p_path);

	if (path.ends_with(":") || path.ends_with(":\\") || _is_directory(path)) {
		return ERR_FILE_CANT_OPEN;
	}

	if (is_backup_save_enabled() && p_mode_flags == WRITE) {
		String temp_path = _create_temp_file_in_place(save_path);
		if (temp_path.is_empty()) {
			last_error = ERR_FILE_CANT_WRITE;
			return last_error;
		}
		path = temp_path;
	}

	const WCHAR *mode_string = _mode_string_from_flags(p_mode_flags);
	ERR_FAIL_COND_V_MSG(!mode_string, ERR_INVALID_PARAMETER, "Invalid mode flags for file open.");

	f = _wfsopen((LPCWSTR)(path.utf16().get_data()), mode_string, _share_mode_from_flags(p_mode_flags));

	if (!f) {
		switch (errno) {
			case ENOENT: last_error = ERR_FILE_NOT_FOUND; break;
			default: last_error = ERR_FILE_CANT_OPEN; break;
		}
		return last_error;
	}

	last_error = OK;
	flags = p_mode_flags;
	return OK;
}

#endif // WINDOWS_ENABLED

	invalid_files.clear();
}

#endif // WINDOWS_ENABLED
