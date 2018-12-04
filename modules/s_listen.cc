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

extern "C" std::list<net::listener> listeners;

extern "C" bool loaded_listener(const string_view &name);
static bool load_listener(const string_view &, const json::object &);
extern "C" bool unload_listener(const string_view &name);
extern "C" bool load_listener(const string_view &name);
static void init_listeners();
static void on_unload();
static void on_load();

mapi::header
IRCD_MODULE
{
	"Server listeners", on_load, on_unload
};

/// Active listener state
decltype(listeners)
listeners;

//
// On module load any existing listener descriptions are sought out
// of room state and instantiated (i.e on startup).
//

void
on_load()
{
	if(ircd::nolisten)
	{
		log::warning
		{
			"Not listening on any addresses because nolisten flag is set."
		};

		return;
	}

	init_listeners();
}

void
on_unload()
{
	listeners.clear();
}

void
init_listeners()
{
/*
	m::room::state{m::my_room}.for_each("ircd.listen", []
	(const m::event &event)
	{
		load_listener(event);
	});

	if(listeners.empty())
		log::warning
		{
			"No listening sockets configured; can't hear anyone."
		};
*/
}

//
// Upon processing of a new event which saved a listener description
// to room state in its content, we instantiate the listener here.
//

//
// Common
//

bool
load_listener(const string_view &name)
{
	bool ret{false};

	return ret;
}

bool
unload_listener(const string_view &name)
{
	if(!loaded_listener(name))
		return false;

	listeners.remove_if([&name]
	(const auto &listener)
	{
		return listener.name() == name;
	});

	return true;
}

static bool
_listener_proffer(const net::ipport &ipport)
{
	if(unlikely(ircd::runlevel != ircd::runlevel::RUN))
	{
		log::dwarning
		{
			"Refusing to add new client from %s in runlevel %s",
			string(ipport),
			reflect(ircd::runlevel)
		};

		return false;
	}

	if(unlikely(client::map.size() >= size_t(client::settings::max_client)))
	{
		log::warning
		{
			"Refusing to add new client from %s because maximum of %zu reached",
			string(ipport),
			size_t(client::settings::max_client)
		};

		return false;
	}

	if(client::count(ipport) >= size_t(client::settings::max_client_per_peer))
	{
		log::dwarning
		{
			"Refusing to add new client from %s: maximum of %zu connections for peer.",
			string(ipport),
			size_t(client::settings::max_client_per_peer)
		};

		return false;
	}

	return true;
}

bool
load_listener(const string_view &name,
              const json::object &opts)
try
{
	if(loaded_listener(name))
		throw error
		{
			"A listener with the name '%s' is already loaded", name
		};

	listeners.emplace_back(name, opts, client::create, _listener_proffer);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to init listener '%s' :%s",
		name,
		e.what()
	};

	return false;
}

bool
loaded_listener(const string_view &name)
{
	return end(listeners) != std::find_if(begin(listeners), end(listeners), [&name]
	(const auto &listener)
	{
		return listener.name() == name;
	});
}
