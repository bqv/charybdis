// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/util/params.h>

using namespace ircd;

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

static void init_cmds();

mapi::header
IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable self-reflecting command library.", []
	{
		init_cmds();
	}
};

conf::item<seconds>
default_synapse
{
	{ "name",     "ircd.console.timeout" },
	{ "default",  45L                    },
};

/// The first parameter for all commands. This aggregates general options
/// passed to commands as well as providing the output facility with an
/// ostream interface. Commands should only send output to this object. The
/// command's input line is not included here; it's the second param to a cmd.
struct opt
{
	std::ostream &out;
	bool html {false};
	seconds timeout {default_synapse};
	string_view special;

	operator std::ostream &()
	{
		return out;
	}

	template<class T> auto &operator<<(T&& t)
	{
		out << std::forward<T>(t);
		return out;
	}

	auto &operator<<(std::ostream &(*manip)(std::ostream &))
	{
		return manip(out);
	}
};

/// Instances of this object are generated when this module reads its
/// symbols to find commands. These instances are then stored in the
/// cmds set for lookup and iteration.
struct cmd
{
	using is_transparent = void;

	static constexpr const size_t &PATH_MAX
	{
		8
	};

	std::string name;
	std::string symbol;
	mods::sym_ptr ptr;

	bool operator()(const cmd &a, const cmd &b) const
	{
		return a.name < b.name;
	}

	bool operator()(const string_view &a, const cmd &b) const
	{
		return a < b.name;
	}

	bool operator()(const cmd &a, const string_view &b) const
	{
		return a.name < b;
	}

	cmd(std::string name, std::string symbol)
	:name{std::move(name)}
	,symbol{std::move(symbol)}
	,ptr{IRCD_MODULE, this->symbol}
	{}

	cmd() = default;
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
};

std::set<cmd, cmd>
cmds;

void
init_cmds()
{
	auto symbols
	{
		mods::symbols(mods::path(IRCD_MODULE))
	};

	for(std::string &symbol : symbols)
	{
		// elide lots of grief by informally finding this first
		if(!has(symbol, "console_cmd"))
			continue;

		thread_local char buf[1024];
		const string_view demangled
		{
			demangle(buf, symbol)
		};

		std::string command
		{
			replace(between(demangled, "__", "("), "__", " ")
		};

		const auto iit
		{
			cmds.emplace(std::move(command), std::move(symbol))
		};

		if(!iit.second)
			throw error
			{
				"Command '%s' already exists", command
			};
	}
}

const cmd *
find_cmd(const string_view &line)
{
	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		const auto it{cmds.lower_bound(name)};
		if(it == end(cmds) || it->name != name)
			continue;

		return &(*it);
	}

	return nullptr;
}

//
// Main command dispatch
//

int console_command_derived(opt &, const string_view &line);

static int
_console_command(opt &out,
                 const string_view &line)
{
	const cmd *const cmd
	{
		find_cmd(line)
	};

	if(!cmd)
		return console_command_derived(out, line);

	const auto args
	{
		lstrip(split(line, cmd->name).second, ' ')
	};

	const auto &ptr{cmd->ptr};
	using prototype = bool (struct opt &, const string_view &);
	return ptr.operator()<prototype>(out, args);
}

/// This function may be linked and called by those wishing to execute a
/// command. Output from the command will be appended to the provided ostream.
/// The input to the command is passed in `line`. Since `struct opt` is not
/// accessible outside of this module, all public options are passed via a
/// plaintext string which is parsed here.
extern "C" int
console_command(std::ostream &out,
                const string_view &line,
                const string_view &opts)
try
{
	opt opt
	{
		out,
		has(opts, "html")
	};

	return _console_command(opt, line);
}
catch(const params::error &e)
{
	out << e.what() << std::endl;
	return true;
}
catch(const bad_command &e)
{
	return -2;
}

//
// Help
//

bool
console_cmd__help(opt &out, const string_view &line)
{
	const auto cmd
	{
		find_cmd(line)
	};

	if(cmd)
	{
		out << "No help available for '" << cmd->name << "'."
		    << std::endl;

		//TODO: help string symbol map
	}

	out << "Commands available: \n"
	    << std::endl;

	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		string_view last;
		auto it{cmds.lower_bound(name)};
		if(it == end(cmds))
			continue;

		for(; it != end(cmds); ++it)
		{
			if(!startswith(it->name, name))
				break;

			const auto prefix
			{
				tokens_before(it->name, ' ', e)
			};

			if(last == prefix)
				continue;

			if(name && prefix != name && !startswith(lstrip(prefix, name), ' '))
				break;

			last = prefix;
			const auto suffix
			{
				e > 1? tokens_after(prefix, ' ', e - 2) : prefix
			};

			if(empty(suffix))
				continue;

			out << suffix << std::endl;
		}

		break;
	}

	return true;
}

//
// Test trigger stub
//

bool
console_cmd__test(opt &out, const string_view &line)
{
	return true;
}

//
// Time cmd prefix (like /usr/bin/time)
//

bool
console_cmd__time(opt &out, const string_view &line)
{
	ircd::timer timer;

	const auto ret
	{
		_console_command(out, line)
	};

	thread_local char buf[32];
	out << std::endl
	    << pretty(buf, timer.at<microseconds>())
	    << std::endl;

	return ret;
}

//
// Derived commands
//

int console_command_numeric(opt &, const string_view &line);
bool console_json(const json::object &);

int
console_command_derived(opt &out, const string_view &line)
{
	const string_view id
	{
		token(line, ' ', 0)
	};

	return -1;
}

//
// Command by JSON
//

bool
console_json(const json::object &object)
{
	if(!object.has("type"))
		return true;

	//return console_cmd__exec__event(object);
	return true;
}

//
// Command by ID
//

//
// misc
//

bool
console_cmd__exit(opt &out, const string_view &line)
{
	return false;
}

bool
console_cmd__debug(opt &out, const string_view &line)
{
	if(!RB_DEBUG_LEVEL)
	{
		out << "Debugging is not compiled in." << std::endl;
		return true;
	}

	if(log::console_enabled(log::DEBUG))
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
		return true;
	} else {
		out << "Turning on debuglog..." << std::endl;
		log::console_enable(log::DEBUG);
		return true;
	}
}

//
// main
//

bool
console_cmd__die(opt &out, const string_view &line)
{
	ircd::quit();
	return false;
}

[[noreturn]] bool
console_cmd__die__hard(opt &out, const string_view &line)
{
	ircd::terminate();
	__builtin_unreachable();
}

//
// log
//

bool
console_cmd__log(opt &out, const string_view &line)
{
	for(const auto *const &log : log::log::list)
		out << (log->snote? log->snote : '-')
		    << " " << std::setw(8) << std::left << log->name
		    << " "
		    << (log->fmasked? " FILE" : "")
		    << (log->cmasked? " CONSOLE" : "")
		    << std::endl;

	return true;
}

bool
console_cmd__log__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"level",
	}};

	if(!param.count())
	{
		for(int i(0); i < num_of<log::facility>(); ++i)
			if(i > RB_LOG_LEVEL)
				out << "[\033[1;40m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
			else if(console_enabled(log::facility(i)))
				out << "[\033[1;42m+\033[0m]: " << reflect(log::facility(i)) << std::endl;
			else
				out << "[\033[1;41m-\033[0m]: " << reflect(log::facility(i)) << std::endl;

		return true;
	}

	const int level
	{
		param.at<int>(0)
	};

	for(int i(0); i < num_of<log::facility>(); ++i)
		if(i > RB_LOG_LEVEL)
		{
			out << "[\033[1;40m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
		}
		else if(i <= level)
		{
			console_enable(log::facility(i));
			out << "[\033[1;42m+\033[0m]: " << reflect(log::facility(i)) << std::endl;
		} else {
			console_disable(log::facility(i));
			out << "[\033[1;41m-\033[0m]: " << reflect(log::facility(i)) << std::endl;
		}

	return true;
}

bool
console_cmd__log__mask(opt &out, const string_view &line)
{
	thread_local string_view list[64];
	const auto &count
	{
		tokens(line, ' ', list)
	};

	log::console_mask({list, count});
	return true;
}

bool
console_cmd__log__unmask(opt &out, const string_view &line)
{
	thread_local string_view list[64];
	const auto &count
	{
		tokens(line, ' ', list)
	};

	log::console_unmask({list, count});
	return true;
}

bool
console_cmd__log__mark(opt &out, const string_view &line)
{
	const string_view &msg
	{
		empty(line)?
			"marked by console":
			line
	};

	log::mark
	{
		msg
	};

	out << "The log files were marked with '" << msg
	    << "'"
	    << std::endl;

	return true;
}

bool
console_cmd__mark(opt &out, const string_view &line)
{
	return console_cmd__log__mark(out, line);
}

//
// info
//

bool
console_cmd__info(opt &out, const string_view &line)
{
	info::dump();

	out << "Daemon information was written to the log."
	    << std::endl;

	return true;
}

bool
console_cmd__uptime(opt &out, const string_view &line)
{
	const seconds uptime
	{
		ircd::uptime()
	};

	const hours uptime_h
	{
		uptime.count() / (60L * 60L)
	};

	const minutes uptime_m
	{
		(uptime.count() / 60L) % 60L
	};

	const minutes uptime_s
	{
		uptime.count() % 60L
	};

	out << "Running for ";

	if(uptime_h.count())
		out << uptime_h.count() << " hours ";

	if(uptime_m.count())
		out << uptime_m.count() << " minutes ";

	out << uptime_s.count() << " seconds." << std::endl;
	return true;
}

bool
console_cmd__date(opt &out, const string_view &line)
{
	out << ircd::time() << std::endl;

	thread_local char buf[128];
	const auto now{ircd::now<system_point>()};
	out << timef(buf, now, ircd::localtime) << std::endl;
	out << timef(buf, now) << " (UTC)" << std::endl;

	return true;
}

//
// mem
//

bool
console_cmd__mem(opt &out, const string_view &line)
{
	auto &this_thread
	{
		ircd::allocator::profile::this_thread
	};

	out << "IRCd thread allocations:" << std::endl
	    << "alloc count:  " << this_thread.alloc_count << std::endl
	    << "freed count:  " << this_thread.free_count << std::endl
	    << "alloc bytes:  " << pretty(iec(this_thread.alloc_bytes)) << std::endl
	    << "freed bytes:  " << pretty(iec(this_thread.free_bytes)) << std::endl
	    << std::endl;

	thread_local char buf[1024];
	out << "malloc() information:" << std::endl
	    << allocator::info(buf) << std::endl
	    ;

	return true;
}

bool
console_cmd__mem__trim(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"pad"
	}};

	const size_t &pad
	{
		param.at<size_t>("pad", 0UL)
	};

	const auto ret
	{
		ircd::allocator::trim(pad)
	};

	out << "malloc trim "
	    << (ret? "was able to release some memory." : "did not release any memory.")
	    << std::endl;

	return true;
}

//
// env
//

bool
console_cmd__env(opt &out, const string_view &line)
{
	if(!::environ)
		throw error
		{
			"Env variable list not available."
		};

	const params param{line, " ",
	{
		"key"
	}};

	if(param["key"] == "*")
	{
		for(const char *const *e(::environ); *e; ++e)
			out << *e << std::endl;

		return true;
	}

	if(param["key"])
	{
		out << util::getenv(param["key"]) << std::endl;
		return true;
	}

	for(const char *const *e(::environ); *e; ++e)
	{
		string_view kv[2];
		tokens(*e, '=', kv);
		if(!startswith(kv[0], "IRCD_") && !startswith(kv[0], "ircd_"))
			continue;

		out << std::setw(64) << std::left << kv[0]
		    << " :" << kv[1]
		    << std::endl;
	}

	return true;
}

//
// aio
//

bool
console_cmd__aio(opt &out, const string_view &line)
{
	if(!fs::aio::context)
		throw error
		{
			"AIO is not available."
		};

	const auto &s
	{
		fs::aio::stats
	};

	out << std::setw(12) << std::left << "requests"
	    << std::setw(9) << std::right << s.requests
	    << "   " << pretty(iec(s.bytes_requests))
	    << std::endl;

	out << std::setw(12) << std::left << "requests cur"
	    << std::setw(9) << std::right << (s.requests - s.complete)
	    << "   " << pretty(iec(s.bytes_requests - s.bytes_complete))
	    << std::endl;

	out << std::setw(12) << std::left << "requests avg"
	    << std::setw(9) << std::right << " "
	    << "   " << pretty(iec(s.bytes_requests / s.requests))
	    << std::endl;

	out << std::setw(12) << std::left << "requests max"
	    << std::setw(9) << std::right << s.max_requests
	    << std::endl;

	out << std::setw(12) << std::left << "reads"
	    << std::setw(9) << std::right << s.reads
	    << "   " << pretty(iec(s.bytes_read))
	    << std::endl;

	out << std::setw(12) << std::left << "reads cur"
	    << std::setw(9) << std::right << s.cur_reads
	    << std::endl;

	out << std::setw(12) << std::left << "reads avg"
	    << std::setw(9) << std::right << " "
	    << "   " << pretty(iec(s.bytes_read / s.reads))
	    << std::endl;

	out << std::setw(12) << std::left << "reads max"
	    << std::setw(9) << std::right << s.max_reads
	    << std::endl;

	out << std::setw(12) << std::left << "writes"
	    << std::setw(9) << std::right << s.writes
	    << "   " << pretty(iec(s.bytes_write))
	    << std::endl;

	out << std::setw(12) << std::left << "writes cur"
	    << std::setw(9) << std::right << s.cur_writes
	    << "   " << pretty(iec(s.cur_bytes_write))
	    << std::endl;

	out << std::setw(12) << std::left << "writes avg"
	    << std::setw(9) << std::right << " "
	    << "   " << pretty(iec(s.bytes_write / s.writes))
	    << std::endl;

	out << std::setw(12) << std::left << "writes max"
	    << std::setw(9) << std::right << s.max_writes
	    << std::endl;

	out << std::setw(12) << std::left << "errors"
	    << std::setw(9) << std::right << s.errors
	    << "   " << pretty(iec(s.bytes_errors))
	    << std::endl;

	out << std::setw(12) << std::left << "cancel"
	    << std::setw(9) << std::right << s.cancel
	    << "   " << pretty(iec(s.bytes_cancel))
	    << std::endl;

	out << std::setw(12) << std::left << "handles"
	    << std::setw(9) << std::right << s.handles
	    << std::endl;

	out << std::setw(12) << std::left << "events"
	    << std::setw(9) << std::right << s.events
	    << std::endl;

	return true;
}

//
// conf
//

bool
console_cmd__conf__list(opt &out, const string_view &line)
{
	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
		out
		<< std::setw(48) << std::left << std::setfill('_') << item.first
		<< " " << item.second->get(val)
		<< std::endl;

	return true;
}

bool
console_cmd__conf(opt &out, const string_view &line)
{
	return console_cmd__conf__list(out, line);
}

bool
console_cmd__conf__set(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key", "value"
	}};

	const auto &key
	{
		param.at(0)
	};

	const auto &val
	{
		param.at(1)
	};

	return true;
}

bool
console_cmd__conf__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"key"
	}};

	const auto &key
	{
		param.at(0)
	};

	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
	{
		if(item.first != key)
			continue;

		out << std::setw(48) << std::right << item.first
		    << " = " << item.second->get(val)
		    << std::endl;

		return true;
	}

	throw http::error
	{
		http::NOT_FOUND, "Conf item '%s' not found", key
	};
}

bool
console_cmd__conf__rehash(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix", "force"
	}};

	using prototype = void (const string_view &, const bool &);
	static mods::import<prototype> rehash_conf
	{
		"s_conf", "rehash_conf"
	};

	string_view prefix
	{
		param.at("prefix", "*"_sv)
	};

	if(prefix == "*")
		prefix = string_view{};

	const bool force
	{
		param["force"] == "force"
	};

	rehash_conf(prefix, force);
/*
	out << "Saved runtime conf items"
	    << (prefix? " with the prefix "_sv : string_view{})
	    << (prefix? prefix : string_view{})
	    << " from the current state into !conf:" << my_host()
	    << std::endl;
*/
	return true;
}

bool
console_cmd__conf__default(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"prefix"
	}};

	using prototype = void (const string_view &);
	static mods::import<prototype> default_conf
	{
		"s_conf", "default_conf"
	};

	string_view prefix
	{
		param.at("prefix", "*"_sv)
	};

	if(prefix == "*")
		prefix = string_view{};

	default_conf(prefix);

	out << "Set runtime conf items"
	    << (prefix? " with the prefix "_sv : string_view{})
	    << (prefix? prefix : string_view{})
	    << " to their default value."
	    << std::endl
	    << "Note: These values must be saved with the rehash command"
	    << " to survive a restart/reload."
	    << std::endl;

	return true;
}

bool
console_cmd__conf__reload(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> reload_conf
	{
		"s_conf", "reload_conf"
	};

	reload_conf();
/*
	out << "Updated any runtime conf items"
	    << " from the current state in !conf:" << my_host()
	    << std::endl;
*/
	return true;
}

bool
console_cmd__conf__reset(opt &out, const string_view &line)
{
	using prototype = void ();
	static mods::import<prototype> refresh_conf
	{
		"s_conf", "refresh_conf"
	};

	refresh_conf();
	out << "All conf items which execute code upon a change"
	    << " have done so with the latest runtime value."
	    << std::endl;

	return true;
}

//
// hook
//

bool
console_cmd__hook__list(opt &out, const string_view &line)
{

	return true;
}

bool
console_cmd__hook(opt &out, const string_view &line)
{
	return console_cmd__hook__list(out, line);
}

//
// mod
//

bool
console_cmd__mod(opt &out, const string_view &line)
{
	auto avflist(mods::available());
	const auto b(std::make_move_iterator(begin(avflist)));
	const auto e(std::make_move_iterator(end(avflist)));
	std::vector<std::string> available(b, e);
	std::sort(begin(available), end(available));

	for(const auto &mod : available)
	{
		const auto loadstr
		{
			mods::loaded(mod)? "\033[1;32;42m+\033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod__path(opt &out, const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod__syms(opt &out, const string_view &line)
{
	const std::string path
	{
		token(line, ' ', 0)
	};

	const std::vector<std::string> symbols
	{
		mods::symbols(path)
	};

	for(const auto &sym : symbols)
		out << sym << std::endl;

	out << " -- " << symbols.size() << " symbols in " << path << std::endl;
	return true;
}

bool
console_cmd__mod__reload(opt &out, const string_view &line)
{
	const auto names
	{
		tokens<std::vector>(line, ' ')
	};

	for(auto it(names.begin()); it != names.end(); ++it)
	{
		const auto &name{*it};
		if(mods::imports.erase(std::string{name}))
			out << name << " unloaded." << std::endl;
		else
			out << name << " is not loaded." << std::endl;
	}

	for(auto it(names.rbegin()); it != names.rend(); ++it)
	{
		const auto &name{*it};
		if(mods::imports.emplace(std::string{name}, name).second)
			out << name << " loaded." << std::endl;
		else
			out << name << " is already loaded." << std::endl;
	}

	return true;
}

bool
console_cmd__mod__load(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(mods::imports.find(name) != end(mods::imports))
		{
			out << name << " is already loaded." << std::endl;
			return;
		}

		mods::imports.emplace(std::string{name}, name);
		out << name << " loaded." << std::endl;
	});

	return true;
}

bool
console_cmd__mod__unload(opt &out, const string_view &line)
{
	tokens(line, ' ', [&out]
	(const string_view &name)
	{
		if(!mods::imports.erase(std::string{name}))
		{
			out << name << " is not loaded." << std::endl;
			return;
		}

		out << "unloaded " << name << std::endl;
	});

	return true;
}

//
// ctx
//

bool
console_cmd__ctx__interrupt(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	for(size_t i(0); i < param.count(); ++i)
		for(auto *const &ctx : ctx::ctxs)
			if(id(*ctx) == param.at<uint64_t>(i))
			{
				interrupt(*ctx);
				break;
			}

	return true;
}

bool
console_cmd__ctx__term(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"id", "[id]..."
	}};

	for(size_t i(0); i < param.count(); ++i)
		for(auto *const &ctx : ctx::ctxs)
			if(id(*ctx) == param.at<uint64_t>(i))
			{
				terminate(*ctx);
				break;
			}

	return true;
}

bool
console_cmd__ctx__list(opt &out, const string_view &line)
{
	out << "   "
	    << "ID"
	    << "    "
	    << "STATE"
	    << "   "
	    << "YIELDS"
	    << "      "
	    << "CYCLE COUNT"
	    << "     "
	    << "PCT"
	    << "     "
	    << "STACK "
	    << "   "
	    << "LIMIT"
	    << "     "
	    << "PCT"
	    << "   "
	    << ":NAME"
	    << std::endl;

	for(const auto *const &ctxp : ctx::ctxs)
	{
		const auto &ctx{*ctxp};
		out << std::setw(5) << std::right << id(ctx);
		out << "  "
		    << (started(ctx)? 'S' : '-')
		    << (running(ctx)? 'R' : '-')
		    << (waiting(ctx)? 'W' : '-')
		    << (finished(ctx)? 'F' : '-')
		    << (interruptible(ctx)? '-' : 'N')
		    << (interruption(ctx)? 'I' : '-')
		    << (termination(ctx)? 'T' : '-')
		    ;

		out << " "
		    << std::setw(8) << std::right << yields(ctx)
		    << " ";

		out << " "
		    << std::setw(15) << std::right << cycles(ctx)
		    << " ";

		const long double total_cyc(ctx::prof::total_slice_cycles());
		const auto tsc_pct
		{
			total_cyc > 0.0? (cycles(ctx) / total_cyc) : 0.0L
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (tsc_pct * 100)
		    << "% ";

		out << "  "
		    << std::setw(7) << std::right << stack_at(ctx)
		    << " ";

		out << " "
		    << std::setw(7) << std::right << stack_max(ctx)
		    << " ";

		const auto stack_pct
		{
			stack_at(ctx) / (long double)stack_max(ctx)
		};

		out << " "
		    << std::setw(5) << std::right << std::fixed << std::setprecision(2) << (stack_pct * 100)
		    << "% ";

		out << "  :"
		    << name(ctx);

		out << std::endl;
	}

	return true;
}

bool
console_cmd__ctx(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__ctx__list(out, line);

	return true;
}

//
// db
//

bool
console_cmd__db__compressions(opt &out, const string_view &line)
{
	out << "Available compressions:"
	    << std::endl
	    << std::endl;

	for(const auto &name : db::compressions)
		if(!name.empty())
			out << name << std::endl;

	return true;
}

bool
console_cmd__db__sync(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	sync(database);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__flush(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[sync]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto sync
	{
		param.at(1, false)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	flush(database, sync);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__sort(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[blocking]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto blocking
	{
		param.at(1, false)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	sort(database, blocking);
	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__compact(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[colname]", "[begin]", "[end]", "[level]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const auto level
	{
		param.at(2, -1)
	};

	const auto begin
	{
		param[3]
	};

	const auto end
	{
		param[4]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(!colname)
	{
		compact(database);
		out << "done" << std::endl;
		return true;
	}

	db::column column
	{
		database, colname
	};

	const bool integer
	{
		begin? try_lex_cast<uint64_t>(begin) : false
	};

	const uint64_t integers[2]
	{
		integer? lex_cast<uint64_t>(begin) : 0,
		integer && end? lex_cast<uint64_t>(end) : 0
	};

	const std::pair<string_view, string_view> range
	{
		integer? byte_view<string_view>(integers[0]) : begin,
		integer && end? byte_view<string_view>(integers[1]) : end,
	};

	compact(column, range, level);

	if(level > -2)
		compact(column, level);

	out << "done" << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__resume(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	resume(database);
	out << "resumed database " << dbname << std::endl;
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__errors(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname",
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto &errors
	{
		db::errors(database)
	};

	size_t i(0);
	for(const auto &error : errors)
		out << std::setw(2) << std::left << (i++) << ':' << error << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__ticker(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[ticker]"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto ticker
	{
		param[1]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(ticker && ticker != "-a")
	{
		out << ticker << ": " << db::ticker(database, ticker) << std::endl;
		return true;
	}

	for(uint32_t i(0); i < db::ticker_max; ++i)
	{
		const string_view &name
		{
			db::ticker_id(i)
		};

		if(!name)
			continue;

		const auto &val
		{
			db::ticker(database, i)
		};

		if(val == 0 && ticker != "-a")
			continue;

		out << std::left << std::setw(48) << std::setfill('_') << name
		    << " " << val
		    << std::endl;
	}

	for(uint32_t i(0); i < db::histogram_max; ++i)
	{
		const string_view &name
		{
			db::histogram_id(i)
		};

		if(!name)
			continue;

		const auto &val
		{
			db::histogram(database, i)
		};

		if(!(val.max > 0.0) && ticker != "-a")
			continue;

		out << std::left << std::setw(48) << std::setfill('_') << name
		    << std::setfill(' ') << std::right
		    << " " << std::setw(9) << val.hits << " hit "
		    << " " << std::setw(13) << val.time << " tot "
		    << " " << std::setw(12) << uint64_t(val.max) << " max "
		    << " " << std::setw(10) << uint64_t(val.median) << " med "
		    << " " << std::setw(9) << uint64_t(val.avg) << " avg "
		    << " " << std::setw(10) << val.stddev << " dev "
		    << " " << std::setw(10) << val.pct95 << " p95 "
		    << " " << std::setw(10) << val.pct99 << " p99 "
		    << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__io(opt &out, const string_view &line)
{
	const auto &ic
	{
		db::iostats_current()
	};

	out << db::string(ic) << std::endl;
	return true;
}

bool
console_cmd__db__perf(opt &out, const string_view &line)
{
	const auto &pc
	{
		db::perf_current()
	};

	out << db::string(pc) << std::endl;
	return true;
}

bool
console_cmd__db__perf__level(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"[level]"
	}};

	if(!param.count())
	{
		const auto &level
		{
			db::perf_level()
		};

		out << "Current level is: " << level << std::endl;
		return true;
	}

	const auto &level
	{
		param.at<uint>(0)
	};

	db::perf_level(level);
	out << "Set level to: " << level << std::endl;
	return true;
}

bool
console_cmd__db__prop(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "property"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto property
	{
		param.at(2)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for integer properties that RocksDB aggregates.
	if(colname == "*")
	{
		const uint64_t value
		{
			db::property(database, property)
		};

		out << value << std::endl;
		return true;
	}

	const auto query{[&out, &database, &property]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::property<db::prop_map>(column, property)
		};

		for(const auto &p : value)
			out << p.first << " : " << p.second << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		query(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &column : database.columns)
	{
		out << std::setw(16) << std::right << name(*column) << " : ";
		query(name(*column));
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column",
	}};

	const auto dbname
	{
		param.at(0)
	};

	auto colname
	{
		param[1]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	struct stats
	{
		size_t usage;
		size_t pinned;
		size_t capacity;
		size_t hits;
		size_t misses;
		size_t inserts;
		size_t inserts_bytes;

		stats &operator+=(const stats &b)
		{
			usage += b.usage;
			pinned += b.pinned;
			capacity += b.capacity;
			hits += b.hits;
			misses += b.misses;
			inserts += b.inserts;
			inserts_bytes += b.inserts_bytes;
			return *this;
		}
	};

	if(!colname)
	{
		const auto usage(db::usage(cache(database)));
		const auto pinned(db::pinned(cache(database)));
		const auto capacity(db::capacity(cache(database)));
		const auto usage_pct
		{
			capacity > 0.0? (double(usage) / double(capacity)) : 0.0L
		};

		const auto hits(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.hit")));
		const auto misses(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.miss")));
		const auto inserts(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.add")));
		const auto inserts_bytes(db::ticker(cache(database), db::ticker_id("rocksdb.block.cache.data.bytes.insert")));

		out << std::left
		    << std::setw(16) << "ROW"
		    << std::right
		    << " "
		    << std::setw(7) << "PCT"
		    << " "
		    << std::setw(9) << "HITS"
		    << " "
		    << std::setw(9) << "MISSES"
		    << " "
		    << std::setw(9) << "INSERT"
		    << " "
		    << std::setw(26) << "CACHED"
		    << " "
		    << std::setw(26) << "CAPACITY"
		    << " "
		    << std::setw(26) << "INSERT TOTAL"
		    << " "
		    << std::setw(20) << "LOCKED"
		    << " "
		    << std::endl;

		out << std::left
		    << std::setw(16) << "*"
		    << std::right
		    << " "
		    << std::setw(6) << std::right << std::fixed << std::setprecision(2) << (usage_pct * 100)
		    << "%"
		    << " "
		    << std::setw(9) << hits
		    << " "
		    << std::setw(9) << misses
		    << " "
		    << std::setw(9) << inserts
		    << " "
		    << std::setw(26) << std::right << pretty(iec(usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(capacity))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(inserts_bytes))
		    << " "
		    << std::setw(20) << std::right << pretty(iec(pinned))
		    << " "
		    << std::endl
		    << std::endl;

		// Now set the colname to * so the column total branch is taken
		// below and we output that line too.
		colname = "*";
	}

	out << std::left
	    << std::setw(16) << "COLUMN"
	    << std::right
	    << " "
	    << std::setw(7) << "PCT"
	    << " "
	    << std::setw(9) << "HITS"
	    << " "
	    << std::setw(9) << "MISSES"
	    << " "
	    << std::setw(9) << "INSERT"
	    << " "
	    << std::setw(26) << "CACHED"
	    << " "
	    << std::setw(26) << "CAPACITY"
	    << " "
	    << std::setw(26) << "INSERT TOTAL"
	    << " "
	    << std::setw(20) << "LOCKED"
	    << " "
	    << std::endl;

		//TODO: compressed cache reenable
		/*
	    << "|"
	    << std::setw(9) << "HITS"
	    << " "
	    << std::setw(9) << "INSERTS"
	    << " "
	    << std::setw(26) << "COMPRESSED CACHED"
	    << " "
	    << std::setw(26) << "COMPRESSED CAPACITY"
	    << " "
	    << std::setw(7) << "PCT"
	    << " "
	    << std::endl;
		*/

	const auto output{[&out]
	(const string_view &column_name, const stats &s, const stats &comp)
	{
		const auto pct
		{
			s.capacity > 0.0? (double(s.usage) / double(s.capacity)) : 0.0L
		};

		const auto pct_comp
		{
			comp.capacity > 0.0? (double(comp.usage) / double(comp.capacity)) : 0.0L
		};

		out << std::setw(16) << std::left << column_name
		    << std::right
		    << " "
		    << std::setw(6) << std::right << std::fixed << std::setprecision(2) << (pct * 100)
		    << '%'
		    << " "
		    << std::setw(9) << s.hits
		    << " "
		    << std::setw(9) << s.misses
		    << " "
		    << std::setw(9) << s.inserts
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(s.capacity))
		    << " "
		    << std::setw(26) << pretty(iec(s.inserts_bytes))
		    << " "
		    << std::setw(20) << std::right << pretty(iec(s.pinned))
		    << " ";

		//TODO: compressed cache reenable
		if(1)
		{
			out << std::endl;
			return;
		}

		out << "|"
		    << std::setw(9) << comp.hits
		    << " "
		    << std::setw(9) << comp.inserts
		    << " "
		    << std::setw(26) << std::right << pretty(iec(comp.usage))
		    << " "
		    << std::setw(26) << std::right << pretty(iec(comp.capacity))
		    << " "
		    << std::setw(6) << std::right << std::fixed << std::setprecision(2) << (pct_comp * 100)
		    << '%'
		    << " "
		    << std::endl;
	}};

	const auto query{[&database]
	(const string_view &colname, const auto &output)
	{
		const db::column column
		{
			database, colname
		};

		const stats s
		{
			db::usage(cache(column)),
			db::pinned(cache(column)),
			db::capacity(cache(column)),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.hit")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.miss")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.add")),
			db::ticker(cache(column), db::ticker_id("rocksdb.block.cache.data.bytes.insert")),
		};

		const stats comp
		{
			db::usage(cache_compressed(column)),
			0,
			db::capacity(cache_compressed(column)),
			db::ticker(cache_compressed(column), db::ticker_id("rocksdb.block.cache.hit")),
			0,
			db::ticker(cache_compressed(column), db::ticker_id("rocksdb.block.cache.add")),
			0
		};

		output(colname, s, comp);
	}};

	// Querying the totals for all caches for all columns in a loop
	if(colname == "*")
	{
		stats s_total{0}, comp_total{0};
		for(const auto &column : database.columns)
			query(name(*column), [&](const auto &column, const auto &s, const auto &comp)
			{
				s_total += s;
				comp_total += comp;
			});

		output("*", s_total, comp_total);
		return true;
	}

	// Query the cache for a single column
	if(colname != "**")
	{
		query(colname, output);
		return true;
	}

	// Querying the cache for all columns in a loop
	for(const auto &column : database.columns)
		query(name(*column), output);

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache__clear(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "[key]"
	}};

	const auto &dbname
	{
		param.at(0)
	};

	const auto &colname
	{
		param[1]
	};

	const auto &key
	{
		param[2]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto clear{[&out, &database]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		db::clear(cache(column));
		db::clear(cache_compressed(column));
		out << "Cleared caches for '" << name(database) << "' '" << colname << "'"
		    << std::endl;
	}};

	const auto remove{[&out, &database]
	(const string_view &colname, const string_view &key)
	{
		db::column column
		{
			database, colname
		};

		const bool removed[]
		{
			db::remove(cache(column), key),
			db::remove(cache_compressed(column), key)
		};

		out << "Removed key from";
		if(removed[0])
			out << " [uncompressed cache]";

		if(removed[1])
			out << " [compressed cache]";

		out << std::endl;
	}};

	if(!colname || colname == "**")
	{
		for(const auto &column : database.columns)
			clear(name(*column));

		return true;
	}

	if(!key)
	{
		clear(colname);
		return true;
	}

	remove(colname, key);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__cache__fetch(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "key"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param[1]
	};

	const auto key
	{
		param[2]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::has(column, key);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__stats(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	return console_cmd__db__prop(out, fmt::snstringf
	{
		1024, "%s %s rocksdb.stats",
		param.at(0),
		param.at(1)
	});
}

bool
console_cmd__db__set(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "option", "value"
	}};

	const auto dbname
	{
		param.at(0)
	};

	const auto colname
	{
		param.at(1, "*"_sv)
	};

	const auto option
	{
		param.at(2)
	};

	const auto value
	{
		param.at(3)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	// Special branch for DBOptions
	if(colname == "*")
	{
		db::setopt(database, option, value);
		out << "done" << std::endl;
		return true;
	}

	const auto setopt{[&out, &database, &option, &value]
	(const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		db::setopt(column, option, value);
		out << colname << " :done" << std::endl;
	}};

	// Branch for querying the property for a single column
	if(colname != "**")
	{
		setopt(colname);
		return true;
	}

	// Querying the property for all columns in a loop
	for(const auto &column : database.columns)
		setopt(name(*column));

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__ingest(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column", "path"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column")
	};

	const auto path
	{
		param.at("path")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::ingest(column, path);
	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

static void
_print_sst_info_header(opt &out)
{
	out << std::left
	    << std::setw(12) << "name"
	    << std::right
	    << "  " << std::setw(6) << "idxs"
	    << "  " << std::setw(9) << "blocks"
	    << "  " << std::setw(9) << "entries"
	    << "  " << std::setw(25) << "file size"
	    << "  " << std::setw(23) << "key range"
	    << "  " << std::setw(25) << "sequence number"
	    << "  " << std::setw(32) << "creation"
	    << "  " << std::setw(3) << "lev"
	    << "  " << std::setw(2) << "ID"
	    << std::left
	    << "  " << std::setw(20) << "column"
	    << std::endl;
}

static void
_print_sst_info(opt &out,
                const db::database::sst::info &f)
{
	const uint64_t &min_key
	{
		f.min_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.min_key)):
			0UL
	};

	const uint64_t &max_key
	{
		f.max_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.max_key)):
			0UL
	};

	out << std::left
	    << std::setw(12) << f.name
	    << "  " << std::setw(6) << std::right << f.index_parts
	    << "  " << std::setw(9) << std::right << f.data_blocks
	    << "  " << std::setw(9) << std::right << f.entries
	    << "  " << std::setw(25) << std::right << pretty(iec(f.size))
	    << "  " << std::setw(10) << std::right << min_key << " : " << std::setw(10) << std::left << max_key
	    << "  " << std::setw(11) << std::right << f.min_seq << " : " << std::setw(11) << std::left << f.max_seq
	    << "  " << std::setw(32) << std::right << timestr(f.created, ircd::localtime)
	    << "  " << std::setw(3) << std::right << f.level
	    << "  " << std::setw(2) << std::right << f.cfid
	    << "  " << std::setw(20) << std::left << f.column
	    << std::endl;
}

static void
_print_sst_info_full(opt &out,
                     const db::database::sst::info &f)
{
	const uint64_t &min_key
	{
		f.min_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.min_key)):
			0UL
	};

	const uint64_t &max_key
	{
		f.max_key.size() == 8?
			uint64_t(byte_view<uint64_t>(f.max_key)):
			0UL
	};

	const auto closeout{[&out]
	(const string_view &name, const auto &closure)
	{
		out << std::left << std::setw(40) << std::setfill('_') << name << " ";
		closure(out);
		out << std::endl;
	}};

	const auto close_auto{[&closeout]
	(const string_view &name, const auto &value)
	{
		closeout(name, [&value](opt &out)
		{
			out << value;
		});
	}};

	const auto close_size{[&closeout]
	(const string_view &name, const size_t &value)
	{
		closeout(name, [&value](opt &out)
		{
			out << pretty(iec(value));
		});
	}};

	close_auto("name", f.name);
	close_auto("directory", f.path);
	close_auto("column ID", f.cfid);
	close_auto("column", f.column);
	close_auto("format", f.format);
	close_auto("version", f.version);
	close_auto("comparator", f.comparator);
	close_auto("merge operator", f.merge_operator);
	close_auto("prefix extractor", f.prefix_extractor);
	close_size("file size", f.size);
	close_auto("creation", timestr(f.created, ircd::localtime));
	close_auto("level", f.level);
	close_auto("lowest sequence", f.min_seq);
	close_auto("highest sequence", f.max_seq);
	close_auto("lowest key", min_key);
	close_auto("highest key", max_key);
	close_auto("fixed key length", f.fixed_key_len);
	close_auto("range deletes", f.range_deletes);
	close_auto("compacting", f.compacting? "yes"_sv : "no"_sv);
	close_auto("", "");

	close_size("index root size", f.top_index_size);
	close_auto("index data blocks", f.index_parts);
	close_size("index data size", f.index_size);
	close_size("index data block average size", f.index_size / double(f.index_parts));
	close_size("index data average per-key", f.index_size / double(f.entries));
	close_size("index data average per-block", f.index_size / double(f.data_blocks));
	close_auto("index root percent of index", 100.0 * (f.top_index_size / double(f.index_size)));
	close_auto("index data percent of keys", 100.0 * (f.index_size / double(f.keys_size)));
	close_auto("index data percent of values", 100.0 * (f.index_size / double(f.values_size)));
	close_auto("index data percent of data", 100.0 * (f.index_size / double(f.data_size)));
	close_auto("", "");

	close_auto("filter", f.filter);
	close_size("filter size", f.filter_size);
	close_auto("filter average per-key", f.filter_size / double(f.entries));
	close_auto("filter average per-block", f.filter_size / double(f.data_blocks));
	close_auto("filter percent of index", 100.0 * (f.filter_size / double(f.index_size)));
	close_auto("filter percent of data", 100.0 * (f.filter_size / double(f.data_size)));
	close_auto("filter percent of keys", 100.0 * (f.filter_size / double(f.keys_size)));
	close_auto("filter percent of values", 100.0 * (f.filter_size / double(f.values_size)));
	close_auto("", "");

	close_auto("keys", f.entries);
	close_size("keys size", f.keys_size);
	close_size("keys average size", f.keys_size / double(f.entries));
	close_auto("keys percent of values", 100.0 * (f.keys_size / double(f.values_size)));
	close_auto("keys average per-block", f.entries / double(f.data_blocks));
	close_auto("keys average per-index", f.entries / double(f.index_parts));
	close_auto("", "");

	close_auto("values", f.entries);
	close_size("values size", f.values_size);
	close_size("values average size", f.values_size / double(f.entries));
	close_size("values average size per-block", f.values_size / double(f.data_blocks));
	close_auto("", "");

	const auto blocks_size{f.keys_size + f.values_size};
	close_auto("blocks", f.data_blocks);
	close_size("blocks size", blocks_size);
	close_size("blocks average size", blocks_size / double(f.data_blocks));
	close_auto("blocks percent of keys", 100.0 * (f.data_blocks / double(f.entries)));
	close_auto("", "");

	close_auto("data compression", f.compression);
	close_size("data size", f.data_size);
	close_size("data blocks average size", f.data_size / double(f.data_blocks));
	close_auto("data compression percent", 100 - 100.0L * (f.data_size / double(blocks_size)));
	close_auto("", "");
}

bool
console_cmd__db__sst(opt &out, const string_view &line)
{
	string_view buf[16];
	const vector_view<const string_view> args
	{
		buf, tokens(line, " ", buf)
	};

	db::database::sst::tool(args);
	return true;
}

bool
console_cmd__db__sst__dump(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"dbname", "column", "begin", "end", "path"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column", "*"_sv)
	};

	const auto begin
	{
		param["begin"]
	};

	const auto end
	{
		param["end"]
	};

	const auto path
	{
		param["path"]
	};

	auto &database
	{
		db::database::get(dbname)
	};

	_print_sst_info_header(out);

	const auto do_dump{[&](const string_view &colname)
	{
		db::column column
		{
			database, colname
		};

		const db::database::sst::dump dump
		{
			column, {begin, end}, path
		};

		_print_sst_info(out, dump.info);
	}};

	if(colname != "*")
	{
		do_dump(colname);
		return true;
	}

	for(const auto &column : database.columns)
		do_dump(name(*column));

	return true;
}

bool
console_cmd__db__files(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column", "*"_sv)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	if(colname == "*")
	{
		const auto fileinfos
		{
			db::database::sst::info::vector(database)
		};

		_print_sst_info_header(out);
		for(const auto &fileinfo : fileinfos)
			_print_sst_info(out, fileinfo);

		out << "-- " << fileinfos.size() << " files"
		    << std::endl;

		return true;
	}

	if(startswith(colname, "/"))
	{
		const db::database::sst::info info{database, colname};
		_print_sst_info_full(out, info);
		return true;
	}

	const db::column column
	{
		database, colname
	};

	const db::database::sst::info::vector vector{column};
	_print_sst_info_header(out);
	for(const auto &info : vector)
		_print_sst_info(out, info);

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__bytes(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	auto &database
	{
		db::database::get(param.at(0))
	};

	if(!param[1] || param[1] == "*")
	{
		const auto bytes
		{
			db::bytes(database)
		};

		out << bytes << std::endl;
		return true;
	}

	const auto query{[&out, &database]
	(const string_view &colname)
	{
		const db::column column
		{
			database, colname
		};

		const auto value
		{
			db::bytes(column)
		};

		out << std::setw(16) << std::right << colname
		    << " : " << value
		    << std::endl;
	}};

	if(param[1] != "**")
	{
		query(param.at(1));
		return true;
	}

	for(const auto &column : database.columns)
		query(name(*column));

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txns(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "seqnum", "limit"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto cur_seq
	{
		db::sequence(database)
	};

	const auto seqnum
	{
		param.at<int64_t>("seqnum", cur_seq)
	};

	const auto limit
	{
		param.at<int64_t>("limit", 32L)
	};

	// note that we decrement the sequence number here optimistically
	// based on the number of possible entries in a txn. There are likely
	// fewer entries in a txn thus we will be missing the latest txns or
	// outputting more txns than the limit. We choose the latter here.
	const auto start
	{
		std::max(seqnum - limit * ssize_t(database.columns.size()), 0L)
	};

	for_each(database, start, db::seq_closure_bool{[&out, &seqnum]
	(db::txn &txn, const int64_t &_seqnum) -> bool
	{
		txn.get(db::op::SET, "event_id", []
		(const db::delta &delta)
		{

		});

		return _seqnum <= seqnum;
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txn(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto &database
	{
		db::database::get(dbname)
	};

	get(database, seqnum, db::seq_closure{[&out]
	(db::txn &txn, const uint64_t &seqnum)
	{
		for_each(txn, [&out, &seqnum]
		(const db::delta &delta)
		{
			const string_view &dkey
			{
				std::get<delta.KEY>(delta)
			};

			// !!! Assumption based on the events database schema. If the
			// key is 8 bytes we assume it's an event::idx in binary. No
			// other columns have 8 byte keys; instead they have plaintext
			// event_id amalgams with some binary characters which are simply
			// not displayed by the ostream. We could have a switch here to
			// use m::dbs's key parsers based on the column name but that is
			// not done here yet.
			const string_view &key
			{
				dkey.size() == 8?
					lex_cast(uint64_t(byte_view<uint64_t>(dkey))):
					dkey
			};

			out << std::setw(12)  << std::right  << seqnum << " : "
			    << std::setw(8)   << std::left   << reflect(std::get<delta.OP>(delta)) << " "
			    << std::setw(18)  << std::right  << std::get<delta.COL>(delta) << " "
			    << key
			    << std::endl;
		});
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__checkpoint(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	const auto seqnum
	{
		checkpoint(database)
	};

	out << "Checkpoint " << name(database)
	    << " at sequence " << seqnum << " complete."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__check(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	auto &database
	{
		db::database::get(dbname)
	};

	check(database);
	out << "Check of " << dbname << " completed without error."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__DROP__DROP__DROP(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "column"
	}};

	const auto dbname
	{
		param.at("dbname")
	};

	const auto colname
	{
		param.at("column")
	};

	auto &database
	{
		db::database::get(dbname)
	};

	db::column column
	{
		database, colname
	};

	db::drop(column);

	out << "DROPPED COLUMN " << colname
	    << " FROM DATABASE " << dbname
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__list(opt &out, const string_view &line)
{
	const auto available
	{
		db::available()
	};

	for(const auto &path : available)
	{
		const auto name
		{
			replace(lstrip(lstrip(path, fs::get(fs::DB)), '/'), "/", ":")
		};

		const auto &d
		{
			db::database::get(std::nothrow, name)
		};

		const auto &light
		{
			d? "\033[1;42m \033[0m" : " "
		};

		out << "[" << light << "]"
		    << " " << name
		    << " `" << path << "'"
		    << std::endl;
	}

	return true;
}

bool
console_cmd__db__columns(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname"
	}};

	auto &d
	{
		db::database::get(param.at("dbname"))
	};

	for(const auto &c : d.columns)
	{
		const db::column &column(*c);
		out << '[' << std::setw(3) << std::right << db::id(column) << ']'
		    << " " << std::setw(18) << std::left << db::name(column)
		    << " " << std::setw(25) << std::right << pretty(iec(bytes(column)))
		    << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__info(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"dbname", "[column]"
	}};

	auto &d
	{
		db::database::get(param.at("dbname"))
	};

	const db::column c
	{
		param.at("[column]", string_view{})?
			db::column{d, param.at("[column]")}:
			db::column{}
	};

	const auto closeout{[&out]
	(const string_view &name, const auto &closure)
	{
		out << std::left << std::setw(40) << std::setfill('_') << name << " ";
		closure();
		out << std::endl;
	}};

	const auto property{[&out, &d, &c, &closeout]
	(const string_view &prop)
	{
		const auto name(lstrip(prop, "rocksdb."));
		closeout(name, [&out, &d, &c, &prop]
		{
			if(c)
				out << db::property(c, prop);
			else
				out << db::property(d, prop);
		});
	}};

	const auto sizeprop{[&out, &d, &c, &closeout]
	(const string_view &prop)
	{
		const auto name(lstrip(prop, "rocksdb."));
		closeout(name, [&out, &d, &c, &prop]
		{
			if(c)
				out << pretty(iec(db::property<db::prop_int>(c, prop)));
			else
				out << pretty(iec(db::property(d, prop)));
		});
	}};

	if(c)
	{
		out << db::describe(c).explain
		    << std::endl;

		closeout("SIZE", [&] { out << pretty(iec(bytes(c))); });
		closeout("FILES", [&] { out << file_count(c); });
	} else {
		closeout("uuid", [&] { out << uuid(d); });
		closeout("size", [&] { out << pretty(iec(bytes(d))); });
		closeout("columns", [&] { out << d.columns.size(); });
		closeout("files", [&] { out << file_count(d); });
		closeout("sequence", [&] { out << sequence(d); });
	}

	property("rocksdb.estimate-num-keys");
	property("rocksdb.background-errors");
	property("rocksdb.base-level");
	property("rocksdb.num-live-versions");
	property("rocksdb.current-super-version-number");
	property("rocksdb.min-log-number-to-keep");
	property("rocksdb.is-file-deletions-enabled");
	property("rocksdb.is-write-stopped");
	property("rocksdb.actual-delayed-write-rate");
	property("rocksdb.num-entries-active-mem-table");
	property("rocksdb.num-deletes-active-mem-table");
	property("rocksdb.mem-table-flush-pending");
	property("rocksdb.num-running-flushes");
	property("rocksdb.compaction-pending");
	property("rocksdb.num-running-compactions");
	sizeprop("rocksdb.estimate-pending-compaction-bytes");
	property("rocksdb.num-snapshots");
	property("rocksdb.oldest-snapshot-time");
	sizeprop("rocksdb.size-all-mem-tables");
	sizeprop("rocksdb.cur-size-all-mem-tables");
	sizeprop("rocksdb.cur-size-active-mem-table");
	sizeprop("rocksdb.estimate-table-readers-mem");
	sizeprop("rocksdb.block-cache-capacity");
	sizeprop("rocksdb.block-cache-usage");
	sizeprop("rocksdb.block-cache-pinned-usage");
	if(!c)
		closeout("row cache size", [&] { out << pretty(iec(db::usage(cache(d)))); });

	sizeprop("rocksdb.estimate-live-data-size");
	sizeprop("rocksdb.live-sst-files-size");
	sizeprop("rocksdb.total-sst-files-size");

	if(c)
	{
		const db::database::sst::info::vector v{c};
		for(const auto &info : v)
		{
			out << std::endl;
			_print_sst_info_full(out, info);
		}
	}
	else
	{
		out << std::endl;
		for(const auto &column : d.columns)
		{
			const auto explain
			{
				split(db::describe(*column).explain, '\n').first
			};

			out << std::left << std::setfill (' ') << std::setw(3) << db::id(*column)
			    << " " << std::setw(20) << db::name(*column)
			    << " " << std::setw(24) << pretty(iec(db::bytes(*column)))
			    << " :" << explain << std::endl;
		}
	}

	if(!c && !errors(d).empty())
	{
		size_t i(0);
		out << std::endl;
		out << "ERRORS (" << errors(d).size() << "): " << std::endl;
		for(const auto &error : errors(d))
			out << std::setw(2) << (i++) << ':' << error << std::endl;
	}

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__db__list(out, line);

	return console_cmd__db__info(out, line);
}

//
// peer
//

static bool
html__peer(opt &out, const string_view &line)
{
	out << "<table>";

	out << "<tr>";
	out << "<td> HOST </td>";
	out << "<td> ADDR </td>";
	out << "<td> LINKS </td>";
	out << "<td> REQS </td>";
	out << "<td> ▲ BYTES Q</td>";
	out << "<td> ▼ BYTES Q</td>";
	out << "<td> ▲ BYTES</td>";
	out << "<td> ▼ BYTES</td>";
	out << "<td> ERROR </td>";
	out << "</tr>";

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << "<tr>";

		out << "<td>" << host << "</td>";
		out << "<td>" << ipp << "</td>";

		out << "<td>" << peer.link_count() << "</td>";
		out << "<td>" << peer.tag_count() << "</td>";
		out << "<td>" << peer.write_size() << "</td>";
		out << "<td>" << peer.read_size() << "</td>";
		out << "<td>" << peer.write_total() << "</td>";
		out << "<td>" << peer.read_total() << "</td>";

		out << "<td>";
		if(peer.err_has() && peer.err_msg())
			out << peer.err_msg();
		else if(peer.err_has())
			out << "<unknown error>"_sv;
		out << "</td>";

		out << "</tr>";
	}

	out << "</table>";
	return true;
}

bool
console_cmd__peer(opt &out, const string_view &line)
{
	if(out.html)
		return html__peer(out, line);

	const auto print{[&out]
	(const auto &host, const auto &peer)
	{
		using std::setw;
		using std::left;
		using std::right;

		const net::ipport &ipp{peer.remote};
		out << setw(40) << left << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << " " << setw(2) << right << peer.link_count()     << " L"
		    << " " << setw(2) << right << peer.tag_count()      << " T"
		    << " " << setw(2) << right << peer.tag_committed()  << " TC"
		    << " " << setw(9) << right << peer.write_size()     << " UP Q"
		    << " " << setw(9) << right << peer.read_size()      << " DN Q"
		    << " " << setw(9) << right << peer.write_total()    << " UP"
		    << " " << setw(9) << right << peer.read_total()     << " DN"
		    ;

		if(peer.err_has() && peer.err_msg())
			out << "  :" << peer.err_msg();
		else if(peer.err_has())
			out << "  <unknown error>"_sv;

		out << std::endl;
	}};

	const params param{line, " ",
	{
		"[hostport]", "[all]"
	}};

	const auto &hostport
	{
		param[0]
	};

	const bool all
	{
		has(line, "all")
	};

	if(hostport && hostport != "all")
	{
		auto &peer
		{
			server::find(hostport)
		};

		print(peer.hostname, peer);
		return true;
	}

	for(const auto &p : server::peers)
	{
		const auto &host{p.first};
		const auto &peer{*p.second};
		if(peer.err_has() && !all)
			continue;

		print(host, peer);
	}

	return true;
}

bool
console_cmd__peer__count(opt &out, const string_view &line)
{
	size_t i{0};
	for(const auto &pair : ircd::server::peers)
	{
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(!peer.err_has())
			++i;
	}

	out << i << std::endl;
	return true;
}

bool
console_cmd__peer__error(opt &out, const string_view &line)
{
	for(const auto &pair : ircd::server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{pair.first};
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(!peer.err_has())
			continue;

		const net::ipport &ipp{peer.remote};
		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << peer.e->etime;

		if(peer.err_msg())
			out << "  :" << peer.err_msg();
		else
			out << "  <unknown error>"_sv;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__peer__error__count(opt &out, const string_view &line)
{
	size_t i{0};
	for(const auto &pair : ircd::server::peers)
	{
		assert(bool(pair.second));
		const auto &peer{*pair.second};
		if(peer.err_has())
			++i;
	}

	out << i << std::endl;
	return true;
}

bool
console_cmd__peer__error__clear__all(opt &out, const string_view &line)
{
	size_t cleared(0);
	for(auto &pair : ircd::server::peers)
	{
		const auto &name{pair.first};
		assert(bool(pair.second));
		auto &peer{*pair.second};
		cleared += peer.err_clear();
	}

	out << "cleared " << cleared
	    << " of " << ircd::server::peers.size()
	    << std::endl;

	return true;
}

bool
console_cmd__peer__error__clear(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__peer__error__clear__all(out, line);

	const net::hostport hp
	{
		token(line, ' ', 0)
	};

	const auto cleared
	{
		server::errclear(hp)
	};

	out << std::boolalpha << cleared << std::endl;
	return true;
}

bool
console_cmd__peer__version(opt &out, const string_view &line)
{
	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		if(!empty(peer.server_name))
			out << " :" << peer.server_name;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__peer__find(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"ip:port"
	}};

	const auto &ip{rsplit(param.at(0), ':').first};
	const auto &port{rsplit(param.at(0), ':').second};
	const net::ipport ipp{ip, port? port : "0"};

	for(const auto &p : server::peers)
	{
		const auto &hostname{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp_
		{
			peer.remote
		};

		if(is_v6(ipp) && (!is_v6(ipp_) || host6(ipp) != host6(ipp_)))
			continue;

		if(is_v4(ipp) && (!is_v4(ipp_) || host4(ipp) != host4(ipp_)))
			continue;

		if(net::port(ipp) && net::port(ipp) != net::port(ipp_))
			continue;

		out << hostname << std::endl;
		break;
	}

	return true;
}

bool
console_cmd__peer__cancel(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"hostport"
	}};

	const auto &hostport
	{
		param.at(0)
	};

	auto &peer
	{
		server::find(hostport)
	};

	peer.cancel();
	return true;
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"Peer not found"
	};
}

bool
console_cmd__peer__close(opt &out, const string_view &line)
try
{
	const params param{line, " ",
	{
		"hostport", "[dc]"
	}};

	const auto &hostport
	{
		param.at(0)
	};

	const auto &dc
	{
		param.at(1, "SSL_NOTIFY"_sv)
	};

	auto &peer
	{
		server::find(hostport)
	};

	const net::close_opts opts
	{
		dc == "RST"?
			net::dc::RST:
		dc == "SSL_NOTIFY"?
			net::dc::SSL_NOTIFY:
			net::dc::SSL_NOTIFY
	};

	peer.close(opts);
	peer.err_clear();
	return true;
}
catch(const std::out_of_range &e)
{
	throw error
	{
		"Peer not found"
	};
}

//
// net
//

bool
console_cmd__net__host(opt &out, const string_view &line)
{
	const params token
	{
		line, " ", {"host", "service"}
	};

	const auto &host{token.at(0)};
	const auto &service
	{
		token.count() > 1? token.at(1) : string_view{}
	};

	const net::hostport hostport
	{
		host, service
	};

	ctx::dock dock;
	bool done{false};
	net::ipport ipport;
	std::exception_ptr eptr;
	net::dns::resolve(hostport, [&done, &dock, &eptr, &ipport]
	(std::exception_ptr eptr_, const net::hostport &, const net::ipport &ipport_)
	{
		eptr = std::move(eptr_);
		ipport = ipport_;
		done = true;
		dock.notify_one();
	});

	while(!done)
		dock.wait();

	if(eptr)
		std::rethrow_exception(eptr);
	else
		out << ipport << std::endl;

	return true;
}

bool
console_cmd__host(opt &out, const string_view &line)
{
	return console_cmd__net__host(out, line);
}

bool
console_cmd__net__host__cache__A(opt &out, const string_view &line)
{
	net::dns::cache::for_each("A", [&]
	(const auto &host, const auto &r)
	{
		const auto &record
		{
			dynamic_cast<const rfc1035::record::A &>(r)
		};

		const net::ipport ipp{record.ip4, 0};
		out << std::setw(48) << std::right << host
		    << "  =>  " << std::setw(21) << std::left << ipp
		    << "  expires " << timestr(record.ttl, ircd::localtime)
		    << " (" << record.ttl << ")"
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__net__host__cache__A__count(opt &out, const string_view &line)
{
	size_t count[2] {0};
	net::dns::cache::for_each("A", [&]
	(const auto &host, const auto &r)
	{
		const auto &record
		{
			dynamic_cast<const rfc1035::record::A &>(r)
		};

		++count[bool(record.ip4)];
		return true;
	});

	out << "resolved:  " << count[1] << std::endl;
	out << "error:     " << count[0] << std::endl;
	return true;
}

bool
console_cmd__net__host__cache__A__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"hostport"
	}};

	if(!param.count())
	{
		out << "NOT IMPLEMENTED" << std::endl;
		return true;
	}

	const net::hostport hostport
	{
		param.at("hostport")
	};

	out << "NOT IMPLEMENTED" << std::endl;
	return true;
}

bool
console_cmd__net__host__cache__SRV(opt &out, const string_view &line)
{
	net::dns::cache::for_each("SRV", [&]
	(const auto &key, const auto &r)
	{
		const auto &record
		{
			dynamic_cast<const rfc1035::record::SRV &>(r)
		};

		thread_local char buf[256];
		const string_view remote{fmt::sprintf
		{
			buf, "%s:%u",
			rstrip(record.tgt, '.'),
			record.port
		}};

		out << std::setw(48) << std::right << key
		    << "  =>  " << std::setw(48) << std::left << remote
		    <<  " expires " << timestr(record.ttl, ircd::localtime)
		    << " (" << record.ttl << ")"
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__net__host__cache__SRV__count(opt &out, const string_view &line)
{
	size_t count[2] {0};
	net::dns::cache::for_each("SRV", [&]
	(const auto &host, const auto &r)
	{
		const auto &record
		{
			dynamic_cast<const rfc1035::record::SRV &>(r)
		};

		++count[bool(record.tgt)];
		return true;
	});

	out << "resolved:  " << count[1] << std::endl;
	out << "error:     " << count[0] << std::endl;
	return true;
}

bool
console_cmd__net__host__cache__SRV__clear(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"hostport", "[service]"
	}};

	if(!param.count())
	{
		out << "NOT IMPLEMENTED" << std::endl;
		return true;
	}

	const net::hostport hostport
	{
		param.at("hostport")
	};

	net::dns::opts opts;
	opts.srv = param.at("[service]", "_matrix._tcp."_sv);

	thread_local char srv_key_buf[128];
	const auto srv_key
	{
		net::dns::make_SRV_key(srv_key_buf, hostport, opts)
	};

	out << "NOT IMPLEMENTED" << std::endl;
	return true;
}

bool
console_cmd__net__listen__list(opt &out, const string_view &line)
{
	using list = std::list<net::listener>;

	static mods::import<list> listeners
	{
		"s_listen", "listeners"
	};

	const list &l(listeners);
	for(const auto &listener : l)
	{
		const json::object opts(listener);
		out << listener << ": " << opts << std::endl;
	}

	return true;
}

bool
console_cmd__net__listen(opt &out, const string_view &line)
{
	if(empty(line))
		return console_cmd__net__listen__list(out, line);

	const params token{line, " ",
	{
		"name",
		"host",
		"port",
		"certificate_pem_path",
		"private_key_pem_path",
		"tmp_dh_path",
		"backlog",
		"max_connections",
	}};

	const json::members opts
	{
		{ "host",                      token.at("host", "0.0.0.0"_sv)           },
		{ "port",                      token.at("port", 8448L)                  },
		{ "certificate_pem_path",      token.at("certificate_pem_path")         },
		{ "private_key_pem_path",      token.at("private_key_pem_path")         },
		{ "tmp_dh_path",               token.at("tmp_dh_path", ""_sv)           },
		{ "backlog",                   token.at("backlog", -1L)                 },
		{ "max_connections",           token.at("max_connections", -1L)         },
	};
/*
	const auto eid
	{
		m::send(m::my_room, m::me, "ircd.listen", token.at("name"), opts)
	};

	out << eid << std::endl;
*/
	return true;
}

bool
console_cmd__net__listen__load(opt &out, const string_view &line)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> load_listener
	{
		"s_listen", "load_listener"
	};

	const params params{line, " ",
	{
		"name",
	}};

	if(load_listener(params.at("name")))
		out << "loaded listener '" << params.at("name") << "'" << std::endl;
	else
		out << "failed to load listener '" << params.at("name") << "'" << std::endl;

	return true;
}

bool
console_cmd__net__listen__unload(opt &out, const string_view &line)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> unload_listener
	{
		"s_listen", "unload_listener"
	};

	const params params{line, " ",
	{
		"name",
	}};

	if(unload_listener(params.at("name")))
		out << "unloaded listener '" << params.at("name") << "'" << std::endl;
	else
		out << "failed to unload listener '" << params.at("name") << "'" << std::endl;

	return true;
}

//
// client
//

bool
console_cmd__client(opt &out, const string_view &line)
{
	using std::setw;
	using std::left;
	using std::right;

	const params param{line, " ",
	{
		"[reqs|id]",
	}};

	const bool &reqs
	{
		param[0] == "reqs"
	};

	const auto &idnum
	{
		!reqs?
			param.at<ulong>(0, 0):
			0
	};

	std::vector<client *> clients(client::map.size());
	static const values<decltype(client::map)> values;
	std::transform(begin(client::map), end(client::map), begin(clients), values);
	std::sort(begin(clients), end(clients), []
	(const auto &a, const auto &b)
	{
		return a->id < b->id;
	});

	for(const auto &client : clients)
	{
		if(idnum && client->id < idnum)
			continue;
		else if(idnum && client->id > idnum)
			break;
		else if(reqs && !client->reqctx)
			continue;

		out << setw(8) << left << client->id
		    << "  " << right << setw(22) << local(*client)
		    << "  " << left << setw(22) << remote(*client)
		    ;

		out << " | RDY " << right << setw(4) << client->ready_count
		    << " | REQ " << right << setw(4) << client->request_count
		    ;

		if(bool(client->sock))
		{
			const auto stat
			{
				net::bytes(*client->sock)
			};

			out << " | UP " << setw(8) << right << stat.second
			    << " | DN " << setw(8) << right << stat.first
			    << " |";
		}

		if(client->reqctx)
			out << " CTX " << setw(4) << id(*client->reqctx);
		else
			out << " ASYNC";

		if(client->request.head.method)
			out << " " << client->request.head.method << ""
			    << " " << client->request.head.path;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__client__clear(opt &out, const string_view &line)
{
	client::terminate_all();
	client::close_all();
	client::wait_all();
	return true;
}

bool
console_cmd__client__spawn(opt &out, const string_view &line)
{
	client::spawn();
	return true;
}

//
// resource
//

bool
console_cmd__resource(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"path", "method"
	}};

	const auto path
	{
		param["path"]
	};

	const auto method
	{
		param["method"]
	};

	if(path && method)
	{
		const auto &r
		{
			resource::find(path)
		};

		const auto &m
		{
			r[method]
		};

		out << method << " "
		    << path
		    << std::endl;

		out << (m.opts->flags & resource::method::REQUIRES_AUTH? " REQUIRES_AUTH" : "")
		    << (m.opts->flags & resource::method::RATE_LIMITED? " RATE_LIMITED" : "")
		    << (m.opts->flags & resource::method::VERIFY_ORIGIN? " VERIFY_ORIGIN" : "")
		    << (m.opts->flags & resource::method::CONTENT_DISCRETION? " CONTENT_DISCRETION" : "")
		    << std::endl;

		return true;
	}

	for(const auto &p : resource::resources)
	{
		const auto &r(*p.second);
		for(const auto &mp : p.second->methods)
		{
			assert(mp.second);
			const auto &m{*mp.second};
			out << std::setw(56) << std::left << p.first
			    << " "
			    << std::setw(7) << mp.first
			    << std::right
			    << " | REQ " << std::setw(8) << m.stats->requests
			    << " | RET " << std::setw(8) << m.stats->completions
			    << " | TIM " << std::setw(8) << m.stats->timeouts
			    << " | ERR " << std::setw(8) << m.stats->internal_errors
			    << std::endl;
		}
	}

	return true;
}

//
// key
//

bool
console_cmd__key(opt &out, const string_view &line)
{

	return true;
}

bool
console_cmd__key__get(opt &out, const string_view &line)
{
	const params param{line, " ",
	{
		"server_name", "[query_server]"
	}};

	const auto server_name
	{
		param.at(0)
	};

	const auto query_server
	{
		param[1]
	};

	if(!query_server)
	{

	}
	else
	{
		const std::pair<string_view, string_view> queries[1]
		{
			{ server_name, {} }
		};

	}

	return true;
}
