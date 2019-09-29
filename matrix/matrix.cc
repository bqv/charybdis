// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void on_load(), on_unload() noexcept;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Chat Protocol",
	ircd::m::on_load,
	ircd::m::on_unload,
};

IRCD_MODULE_EXPORT_DATA
decltype(ircd::m::log)
ircd::m::log
{
	"m", 'm'
};

//
// init
//

/// --- tmp ---

extern "C" void
reload_conf();

namespace ircd::m
{
	std::unique_ptr<dbs::init> _dbs;
	std::unique_ptr<fetch::init> _fetch;
	std::unique_ptr<init::modules> _modules;
}

/// --- /tmp ---

void
ircd::m::on_load()
try
{
	assert(ircd::run::level == run::level::START);
	m::self::init::keys();
	_dbs = std::make_unique<dbs::init>(ircd::server_name, std::string{});
	reload_conf();
	_fetch = std::make_unique<fetch::init>();
	_modules = std::make_unique<init::modules>();
	self::signon();
}
catch(const m::error &e)
{
	log::error
	{
		log, "Failed to start matrix (%u) %s :%s :%s",
		uint(e.code),
		http::status(e.code),
		e.errcode(),
		e.errstr(),
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to start matrix :%s", e.what()
	};

	throw;
}

void
ircd::m::on_unload()
noexcept try
{
	mods::imports.erase("m_listen"s);

	if(m::sync::pool.size())
		m::sync::pool.join();

	self::signoff();
	_fetch.reset(nullptr);
	_modules.reset(nullptr);
	_dbs.reset(nullptr);

	//TODO: remove this for non-interfering shutdown
	server::interrupt_all();
	client::terminate_all();
	client::close_all();
	server::close_all();
	server::wait_all();
	client::wait_all();
}
catch(const m::error &e)
{
	log::critical
	{
		log, "%s %s", e.what(), e.content
	};

	ircd::terminate();
}

//
// init::modules
//

namespace ircd::m
{
	extern const std::vector<string_view> module_names;
	extern const std::vector<string_view> module_names_optional;
}

ircd::m::init::modules::modules()
{
	const unwind::exceptional unload{[this]
	{
		this->fini_imports();
	}};

	init_imports();
}

ircd::m::init::modules::~modules()
noexcept
{
	fini_imports();
}

void
ircd::m::init::modules::init_imports()
{
	if(!bool(ircd::mods::autoload))
	{
		log::warning
		{
			"Not loading modules because noautomod flag is set. "
			"You may still load modules manually."
		};

		return;
	}

	for(const auto &name : module_names) try
	{
		mods::imports.emplace(name, name);
	}
	catch(...)
	{
		const auto &optional(module_names_optional);
		if(std::count(begin(optional), end(optional), name))
			continue;

		throw;
	}

	if(vm::sequence::retired == 0)
	{
		log::notice
		{
			log, "This appears to be your first time running IRCd because the events "
			"database is empty. I will be bootstrapping it with initial events now..."
		};

		m::init::bootstrap{};
	}
}

void
ircd::m::init::modules::fini_imports()
noexcept
{
	for(auto it(module_names.rbegin()); it != module_names.rend(); ++it)
		mods::imports.erase(*it);
}

/// This is an ordered list for loading and unloading modules. This is not the
/// solution I really want at all so consider it temporary. Modules are loaded
/// in the order of the lines and unloaded in reverse order.
decltype(ircd::m::module_names)
ircd::m::module_names
{
	"m_noop",
	"m_breadcrumb_rooms",
	"m_command",
	"m_control",
	"m_device",
	"m_device_list_update",
	"m_direct",
	"m_direct_to_device",
	"m_ignored_user_list",
	"m_listen",
	"m_presence",
	"m_profile",
	"m_receipt",
	"m_room_aliases",
	"m_room_canonical_alias",
	"m_room_create",
	"m_room_history_visibility",
	"m_room_join_rules",
	"m_room_member",
	"m_room_message",
	"m_room_name",
	"m_room_power_levels",
	"m_room_redaction",
	"m_room_server_acl",
	"m_room_third_party_invite",
	"m_user_highlight_auth",

	"net_dns",
	"key_query",
	"key_server",
	"identity_pubkey",
	"identity_v1",
	"media_media",

	"federation_backfill_ids",
	"federation_backfill",
	"federation_event_auth",
	"federation_event",
	"federation_get_groups_publicised",
	"federation_get_missing_events",
	"federation_invite",
	"federation_invite2",
	"federation_make_join",
	"federation_make_leave",
	"federation_publicrooms",
	"federation_query_auth",
	"federation_query",
	"federation_sender",
	"federation_send_join",
	"federation_send_leave",
	"federation_send",
	"federation_state_ids",
	"federation_state",
	"federation_user_devices",
	"federation_user_keys_claim",
	"federation_user_keys_query",
	"federation_version",

	"client_user",
	"client_rooms",
	"client_createroom",
	"client_join",
	"client_account",
	"client_profile",
	"client_notifications",
	"client_devices",
	"client_delete_devices",
	"client_send_to_device",
	"client_keys_changes",
	"client_keys_upload",
	"client_keys_claim",
	"client_keys_query",
	"client_presence",
	"client_joined_groups",
	"client_publicised_groups",
	"client_login",
	"client_logout",
	"client_register_available",
	"client_register_email",
	"client_register",
	"client_directory_list_appservice",
	"client_directory_list_room",
	"client_directory_room",
	"client_directory_user",
	"client_publicrooms",
	"client_search",
	"client_pushers",
	"client_pushrules",
	"client_events",
	"client_initialsync",
	"client_sync",
	"client_sync_account_data",
	"client_sync_device_lists",
	"client_sync_device_one_time_keys_count",
	"client_sync_presence",
	"client_sync_to_device",
	"client_sync_rooms_account_data",
	"client_sync_rooms_ephemeral_receipt",
	"client_sync_rooms_ephemeral",
	"client_sync_rooms_ephemeral_typing",
	"client_sync_rooms",
	"client_sync_rooms_state",
	"client_sync_rooms_timeline",
	"client_sync_rooms_unread_notifications",
	"client_sync_rooms_summary",
	"client_voip_turnserver",
	"client_thirdparty_protocols",
	"client_versions",
	"client_capabilities",

	"well_known",
	"web_root",
	"web_hook",
	"stats",
	"m_vm_fetch",
	"m_vm",
	"m_listen",
};

/// This is a list of modules that are considered "optional" and any loading
/// error for them will not propagate and interrupt m::init.
decltype(ircd::m::module_names_optional)
ircd::m::module_names_optional
{
	"web_hook",
};
