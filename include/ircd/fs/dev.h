// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_FS_DEV_H

namespace ircd::fs::dev
{
	using major_minor = std::pair<ulong, ulong>;

	// Convert device ID's with the major(3) / minor(3) / makedev(3)
	ulong id(const major_minor &);
	major_minor id(const ulong &id);

	// Convert device id integers into a string used as the directory name in sysfs(5)
	string_view sysfs_id(const mutable_buffer &out, const major_minor &);
	string_view sysfs_id(const mutable_buffer &out, const ulong &);

	// Read data for a device from sysfs; path is relative to /sys/dev/block/$id/...
	string_view sysfs(const mutable_buffer &out, const ulong &id, const string_view &path);
	template<class T = size_t, size_t bufmax = 32> T sysfs(const ulong &id, const string_view &path);
}

/// Return a lex_cast'able (an integer) from a sysfs target.
template<class T,
         size_t bufmax>
T
ircd::fs::dev::sysfs(const ulong &id,
                     const string_view &path)
{
	char buf[bufmax];
	const auto value(sysfs(buf, id, path));
	return lex_cast<T>(rstrip(value, '\n'));
}
