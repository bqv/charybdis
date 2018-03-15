// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

ircd::conf::item<size_t>
ircd::m::event_max_size
{
	{ "name",     "m.event.max_size" },
	{ "default",   65507L            },
};

ircd::m::id::event
ircd::m::event_id(const event &event,
                  id::event::buf &buf)
{
	const crh::sha256::buf hash{event};
	return event_id(event, buf, hash);
}

ircd::m::id::event
ircd::m::event_id(const event &event,
                  id::event::buf &buf,
                  const const_buffer &hash)
{
	char readable[b58encode_size(sha256::digest_size)];
	return id::event
	{
		buf, b58encode(readable, hash), my_host()
	};
}

ircd::m::id::event
ircd::m::event_id(const event &event)
{
	return at<"event_id"_>(event);
}

void
ircd::m::check_size(const event &event)
{
	const size_t &event_size
	{
		serialized(event)
	};

	if(event_size > size_t(event_max_size))
		throw m::BAD_JSON
		{
			"Event is %zu bytes which is larger than the maximum %zu bytes",
			event_size,
			size_t(event_max_size)
		};
}

bool
ircd::m::check_size(std::nothrow_t,
                    const event &event)
{
	const size_t &event_size
	{
		serialized(event)
	};

	return event_size <= size_t(event_max_size);
}

ircd::string_view
ircd::m::membership(const m::event &event)
{
	return json::get<"membership"_>(event)?
		string_view{json::get<"membership"_>(event)}:
		unquote(json::get<"content"_>(event).get("membership"));
}

size_t
ircd::m::degree(const event &event)
{
	return degree(event::prev{event});
}

size_t
ircd::m::degree(const event::prev &prev)
{
	size_t ret{0};
	json::for_each(prev, [&ret]
	(const auto &, const json::array &prevs)
	{
		ret += prevs.count();
	});

	return ret;
}

bool
ircd::m::exists(const id::event &event_id)
{
	static constexpr auto idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(idx)
	};

	return has(column, event_id);
}

size_t
ircd::m::count(const event::prev &prev)
{
	size_t ret{0};
	m::for_each(prev, [&ret](const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::for_each(const event::prev &prev,
                  const std::function<void (const event::id &)> &closure)
{
	json::for_each(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const json::array &prev : prevs)
		{
			const event::id &id{unquote(prev[0])};
			closure(id);
		}
	});
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << ": " << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
		out("auth_event", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
		out("prev_state", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
		out("prev_event", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	s << "S[ ";
	for(const json::array prev_state : prev_states)
		s << unquote(prev_state[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << std::setw(16) << std::right << key << ": " << val << std::endl;
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"type",
		"depth",
		"state_key",
		"membership",
		"redacts",
	};

	json::for_each(event, top_keys, out);

	const auto &ts{json::get<"origin_server_ts"_>(event)};
	{
		thread_local char buf[128];
		s << std::setw(16) << std::right << "origin_server_ts" << ": "
		  << timef(buf, ts / 1000L, localtime)
		  << " (" << ts << ")"
		  << std::endl;
	}

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << ": "
		  << hash.first
		  << " "
		  << unquote(hash.second)
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << ": "
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << std::setw(16) << std::right << "[content]" << ": ";
		for(const auto &content : contents)
			s << content.first << ", ";
		s << std::endl;
	}

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
		out("[auth_event]", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
		out("[prev_state]", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
		out("[prev_event]", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event &event,
                        const bool &content_keys)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(val))
			s << val << " ";
		else
			s << "* ";
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
	};

	s << json::get<"depth"_>(event) << " :";
	json::for_each(event, top_keys, out);

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "A:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "S:" << prev_states.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "E:" << prev_events.count() << " ";

	const auto &hashes{json::get<"hashes"_>(event)};
	s << "[ ";
	for(const auto &hash : hashes)
		s << hash.first << " ";
	s << "] ";

	const auto &signatures{json::get<"signatures"_>(event)};
	s << "[ ";
	for(const auto &signature : signatures)
	{
		s << signature.first << "[ ";
		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << "] ";
	}
	s << "] ";

	out("type", json::get<"type"_>(event));

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	out("membership", json::get<"membership"_>(event));
	out("redacts", json::get<"redacts"_>(event));

	const json::object &contents
	{
		content_keys?
			json::get<"content"_>(event):
			json::object{}
	};

	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	resizebuf(s, ret);
	return ret;
}

bool
ircd::m::my(const event &event)
{
	const auto &origin(json::get<"origin"_>(event));
	const auto &eid(json::get<"event_id"_>(event));
	return
		origin?
			my_host(origin):
		eid?
			my(event::id(eid)):
		false;
}

bool
ircd::m::my(const id::event &event_id)
{
	return self::host(event_id.host());
}

//
// event
//

ircd::m::event::event(const id &id,
                      const mutable_buffer &buf)
{
	assert(bool(dbs::events));

	db::gopts opts;
	opts.snapshot = database::snapshot{*dbs::events};
	for(size_t i(0); i < dbs::event_column.size(); ++i)
	{
		const db::cell cell
		{
			dbs::event_column[i], id, opts
		};

		db::assign(*this, cell, id);
	}

	const json::object obj
	{
		string_view{data(buf), json::print(buf, *this)}
	};

	new (this) m::event(obj);
}

ircd::string_view
ircd::m::event::hashes(const mutable_buffer &out,
                       json::iov &event,
                       const string_view &content)
{
	const sha256::buf hash_
	{
		hash(event, content)
	};

	static const size_t hashb64sz
	{
		size_t(sizeof(hash_) * 1.34) + 1
	};

	thread_local char hashb64buf[hashb64sz];
	const json::members hashes
	{
		{ "sha256", b64encode_unpadded(hashb64buf, hash_) }
	};

	return json::stringify(mutable_buffer{out}, hashes);
}

ircd::sha256::buf
ircd::m::event::hash(json::iov &event,
                     const string_view &content)
{
	const json::iov::push _content
	{
		event, { "content", content }
	};

	return hash(event);
}

ircd::sha256::buf
ircd::m::event::hash(const m::event &event)
{
	thread_local char buf[64_KiB];
	const string_view preimage
	{
		stringify(buf, event)
	};

	const sha256::buf hash
	{
		sha256{preimage}
	};

	return hash;
}

ircd::string_view
ircd::m::event::signatures(const mutable_buffer &out,
                           json::iov &event,
                           const json::iov &content)
{
	const ed25519::sig sig
	{
		sign(event, content)
	};

	static const size_t sigb64sz
	{
		size_t(sizeof(sig) * 1.34) + 1
	};

	thread_local char sigb64buf[sigb64sz];
	const json::members sigb64
	{
		{ self::public_key_id, b64encode_unpadded(sigb64buf, sig) }
	};

	const json::members sigs
	{
		{ my_host(), sigb64 }
    };

	return json::stringify(mutable_buffer{out}, sigs);
}

ircd::ed25519::sig
ircd::m::event::sign(json::iov &event,
                     const json::iov &contents)
{
	const auto &type
	{
		event.at("type")
	};

	if(type == "m.room.aliases")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "aliases", contents.at("aliases") }
			}
		}};

		return sign(event);
	}
	else if(type == "m.room.create")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "creator", contents.at("creator") }
			}
		}};

		return sign(event);
	}
	else if(type == "m.room.history_visibility")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "history_visibility", contents.at("history_visibility") }
			}
		}};

		return sign(event);
	}
	else if(type == "m.room.join_rules")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "join_rule", contents.at("join_rule") }
			}
		}};

		return sign(event);
	}
	else if(type == "m.room.member")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "membership", contents.at("membership") }
			}
		}};

		return sign(event);
	}
	else if(type == "m.room.power_levels")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "ban", contents.at("ban")                        },
				{ "events", contents.at("events")                  },
				{ "events_default", contents.at("events_default")  },
				{ "kick", contents.at("kick")                      },
				{ "redact", contents.at("redact")                  },
				{ "state_default", contents.at("state_default")    },
				{ "users", contents.at("users")                    },
				{ "users_default", contents.at("users_default")    },
			}
		}};

		return sign(event);
	}
	else
	{
		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		return sign(event);
	}
}

ircd::ed25519::sig
ircd::m::event::sign(const m::event &event)
{
	thread_local char buf[64_KiB];
	const string_view preimage
	{
		stringify(buf, event)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(preimage)
	};

	assert(self::public_key.verify(preimage, sig));
	return sig;
}

//
// event::prev
//

ircd::m::event::id
ircd::m::event::prev::auth_event(const uint &idx)
const
{
	return std::get<0>(auth_events(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_state(const uint &idx)
const
{
	return std::get<0>(prev_states(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_event(const uint &idx)
const
{
	return std::get<0>(prev_events(idx));
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::auth_events(const uint &idx)
const
{
	const json::array &auth_event
	{
		at<"auth_events"_>(*this).at(idx)
	};

	return
	{
		unquote(auth_event.at(0)), unquote(auth_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_states(const uint &idx)
const
{
	const json::array &state_event
	{
		at<"prev_state"_>(*this).at(idx)
	};

	return
	{
		unquote(state_event.at(0)), unquote(state_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_events(const uint &idx)
const
{
	const json::array &prev_event
	{
		at<"prev_events"_>(*this).at(idx)
	};

	return
	{
		unquote(prev_event.at(0)), unquote(prev_event[1])
	};
}

//
// event::fetch
//

void
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id)
{
	if(!seek(fetch, event_id, std::nothrow))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id,
              std::nothrow_t)
{
	db::seek(fetch.row, string_view{event_id});
	if(!fetch.row.valid(event_id))
		return false;

	auto &event{static_cast<m::event &>(fetch)};
	assign(event, fetch.row, event_id);
	return true;
}

// db::row finds the layout of an event tuple because we pass this as a
// reference argument to its constructor, rather than making db::row into
// a template type.
const ircd::m::event
_dummy_event_;

/// Seekless constructor.
ircd::m::event::fetch::fetch()
:row
{
	*dbs::events, string_view{}, _dummy_event_, cell
}
{
}

/// Seek to event_id and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::id &event_id)
:row
{
	*dbs::events, event_id, _dummy_event_, cell
}
{
	if(!row.valid(event_id))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};

	assign(*this, row, event_id);
}

/// Seek to event_id and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             std::nothrow_t)
:row
{
	*dbs::events, event_id, _dummy_event_, cell
}
{
	if(row.valid(event_id))
		assign(*this, row, event_id);
}

bool
ircd::m::event::fetch::valid(const event::id &event_id)
const
{
	return row.valid(event_id);
}

//
// event::conforms
//

namespace ircd::m
{
	const size_t event_conforms_num{num_of<event::conforms::code>()};
	extern const std::array<string_view, event_conforms_num> event_conforms_reflects;
}

decltype(ircd::m::event_conforms_reflects)
ircd::m::event_conforms_reflects
{
	"INVALID_OR_MISSING_EVENT_ID",
	"INVALID_OR_MISSING_ROOM_ID",
	"INVALID_OR_MISSING_SENDER_ID",
	"MISSING_TYPE",
	"MISSING_ORIGIN",
	"INVALID_ORIGIN",
	"INVALID_OR_MISSING_REDACTS_ID",
	"MISSING_MEMBERSHIP",
	"INVALID_MEMBERSHIP",
	"MISSING_CONTENT_MEMBERSHIP",
	"INVALID_CONTENT_MEMBERSHIP",
	"MISSING_PREV_EVENTS",
	"MISSING_PREV_STATE",
	"DEPTH_NEGATIVE",
	"DEPTH_ZERO",
	"MISSING_SIGNATURES",
	"MISSING_ORIGIN_SIGNATURE",
	"MISMATCH_ORIGIN_SENDER",
	"MISMATCH_ORIGIN_EVENT_ID",
	"SELF_REDACTS",
	"SELF_PREV_EVENT",
	"SELF_PREV_STATE",
	"DUP_PREV_EVENT",
	"DUP_PREV_STATE",
};

std::ostream &
ircd::m::operator<<(std::ostream &s, const event::conforms &conforms)
{
	thread_local char buf[1024];
	s << conforms.string(buf);
	return s;
}

ircd::string_view
ircd::m::reflect(const event::conforms::code &code)
try
{
	return event_conforms_reflects.at(code);
}
catch(const std::out_of_range &e)
{
	return "??????"_sv;
}

ircd::m::event::conforms::code
ircd::m::event::conforms::reflect(const string_view &name)
{
	const auto it
	{
		std::find(begin(event_conforms_reflects), end(event_conforms_reflects), name)
	};

	if(it == end(event_conforms_reflects))
		throw std::out_of_range
		{
			"There is no event::conforms code by that name."
		};

	return code(std::distance(begin(event_conforms_reflects), it));
}

ircd::m::event::conforms::conforms(const event &e,
                                   const uint64_t &skip)
:conforms{e}
{
	report &= ~skip;
}

ircd::m::event::conforms::conforms(const event &e)
:report{0}
{
	if(!valid(m::id::EVENT, json::get<"event_id"_>(e)))
		set(INVALID_OR_MISSING_EVENT_ID);

	if(!valid(m::id::ROOM, json::get<"room_id"_>(e)))
		set(INVALID_OR_MISSING_ROOM_ID);

	if(!valid(m::id::USER, json::get<"sender"_>(e)))
		set(INVALID_OR_MISSING_SENDER_ID);

	if(empty(json::get<"type"_>(e)))
		set(MISSING_TYPE);

	if(empty(json::get<"origin"_>(e)))
		set(MISSING_ORIGIN);

	//TODO: XXX
	if(false)
		set(INVALID_ORIGIN);

	if(empty(json::get<"signatures"_>(e)))
		set(MISSING_SIGNATURES);

	if(empty(json::object{json::get<"signatures"_>(e).get(json::get<"origin"_>(e))}))
		set(MISSING_ORIGIN_SIGNATURE);

	if(!has(INVALID_OR_MISSING_SENDER_ID))
		if(json::get<"origin"_>(e) != m::id::user{json::get<"sender"_>(e)}.host())
			set(MISMATCH_ORIGIN_SENDER);

	if(!has(INVALID_OR_MISSING_EVENT_ID))
		if(json::get<"origin"_>(e) != m::id::event{json::get<"event_id"_>(e)}.host())
			set(MISMATCH_ORIGIN_EVENT_ID);

	if(json::get<"type"_>(e) == "m.room.redaction")
		if(!valid(m::id::EVENT, json::get<"redacts"_>(e)))
			set(INVALID_OR_MISSING_REDACTS_ID);

	if(json::get<"redacts"_>(e))
		if(json::get<"redacts"_>(e) == json::get<"event_id"_>(e))
			set(SELF_REDACTS);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(json::get<"membership"_>(e)))
			set(MISSING_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(json::get<"membership"_>(e)))
			set(INVALID_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(unquote(json::get<"content"_>(e).get("membership"))))
			set(MISSING_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(unquote(json::get<"content"_>(e).get("membership"))))
			set(INVALID_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"prev_events"_>(e)))
			set(MISSING_PREV_EVENTS);

	if(json::get<"type"_>(e) != "m.room.create")
		if(!empty(json::get<"state_key"_>(e)))
			if(empty(json::get<"prev_state"_>(e)))
				set(MISSING_PREV_STATE);

	if(json::get<"depth"_>(e) < 0)
		set(DEPTH_NEGATIVE);

	if(json::get<"type"_>(e) != "m.room.create")
		if(json::get<"depth"_>(e) == 0)
			set(DEPTH_ZERO);

	const prev p{e};
	size_t i{0}, j{0};
	for(const json::array &pe : json::get<"prev_events"_>(p))
	{
		if(unquote(pe.at(0)) == json::get<"event_id"_>(e))
			set(SELF_PREV_EVENT);

		j = 0;
		for(const json::array &pe_ : json::get<"prev_events"_>(p))
			if(i != j++)
				if(pe_.at(0) == pe.at(0))
					set(DUP_PREV_EVENT);

		++i;
	}

	i = 0;
	for(const json::array &ps : json::get<"prev_state"_>(p))
	{
		if(unquote(ps.at(0)) == json::get<"event_id"_>(e))
			set(SELF_PREV_STATE);

		j = 0;
		for(const json::array &ps_ : json::get<"prev_state"_>(p))
			if(i != j++)
				if(ps_.at(0) == ps.at(0))
					set(DUP_PREV_STATE);

		++i;
	}
}

void
ircd::m::event::conforms::operator|=(const code &code)
&
{
	set(code);
}

void
ircd::m::event::conforms::del(const code &code)
{
	report &= ~(1UL << code);
}

void
ircd::m::event::conforms::set(const code &code)
{
	report |= (1UL << code);
}

ircd::string_view
ircd::m::event::conforms::string(const mutable_buffer &out)
const
{
	mutable_buffer buf{out};
	for(uint64_t i(0); i < num_of<code>(); ++i)
	{
		if(!has(code(i)))
			continue;

		if(begin(buf) != begin(out))
			consume(buf, copy(buf, " "_sv));

		consume(buf, copy(buf, m::reflect(code(i))));
	}

	return { data(out), begin(buf) };
}

bool
ircd::m::event::conforms::has(const code &code)
const
{
	return report & (1UL << code);
}

bool
ircd::m::event::conforms::has(const uint &code)
const
{
	return (report & (1UL << code)) == code;
}

bool
ircd::m::event::conforms::operator!()
const
{
	return clean();
}

ircd::m::event::conforms::operator bool()
const
{
	return !clean();
}

bool
ircd::m::event::conforms::clean()
const
{
	return report == 0;
}
