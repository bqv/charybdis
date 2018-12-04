// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

extern "C" void default_conf(const string_view &prefix);
extern "C" void rehash_conf(const string_view &prefix, const bool &existing);
extern "C" void reload_conf();
extern "C" void refresh_conf();
static void init_conf_item(conf::item<> &);

// This module registers with conf::on_init to be called back
// when a conf item is initialized; when this module is unloaded
// we have to unregister that listener using this state.
decltype(conf::on_init)::const_iterator
conf_on_init_iter
{
	end(conf::on_init)
};

mapi::header
IRCD_MODULE
{
	"Server Configuration", []
	{
		conf_on_init_iter = conf::on_init.emplace(end(conf::on_init), init_conf_item);
		reload_conf();
	}, []
	{
		assert(conf_on_init_iter != end(conf::on_init));
		conf::on_init.erase(conf_on_init_iter);
	}
};

/// Set to false to quiet errors from a conf item failing to set
bool
item_error_log
{
	true
};

static void
on_run()
{
	// Suppress errors for this scope.
	const unwind uw{[] { item_error_log = true; }};
	item_error_log = false;
	rehash_conf({}, false);
}

/// Waits for the daemon to transition to the RUN state so we can gather all
/// of the registered conf items and save any new ones to the !conf room.
/// We can't do that on this module init for two reason:
/// - More conf items will load in other modules after this module.
/// - Events can't be safely sent to the !conf room until the RUN state.
const ircd::runlevel_changed
rehash_on_run{[]
(const auto &runlevel)
{
	if(runlevel == ircd::runlevel::RUN)
		ctx::context
		{
			"confhash", 256_KiB, on_run, ctx::context::POST
		};
}};

extern "C" void
get_conf_item(const string_view &key,
              const std::function<void (const string_view &)> &closure)
{

}

void
rehash_conf(const string_view &prefix,
            const bool &existing)
{

}

void
default_conf(const string_view &prefix)
{
	for(const auto &p : conf::items)
	{
		const auto &key{p.first};
		if(prefix && !startswith(key, prefix))
			continue;

		const auto &item{p.second}; assert(item);
		const auto value
		{
			unquote(item->feature["default"])
		};

		conf::set(key, value);
	}
}

void
reload_conf()
{

}

void
refresh_conf()
{
	ircd::conf::reset();
}
