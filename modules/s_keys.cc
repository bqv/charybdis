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

static void init_my_ed25519();
static void init_my_tls_crt();
extern "C" void init_my_keys();

mapi::header
IRCD_MODULE
{
	"Server keys"
};

void
init_my_keys()
{
	init_my_tls_crt();
}

conf::item<std::string>
tls_key_dir
{
	{ "name",     "ircd.keys.tls_key_dir"  },
	{ "default",  fs::cwd()                }
};

void
init_my_tls_crt()
{
/*
	if(!m::self::origin)
		throw error
		{
			"The m::self::origin must be set to init my ed25519 key."
		};

	const std::string private_key_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.key",
	};

	const std::string public_key_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.key.pub",
	};

	const std::string dhparam_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.dh",
	};

	const std::string certificate_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt",
	};

	const std::string private_key_file
	{
		fs::make_path(private_key_path_parts)
	};

	const std::string public_key_file
	{
		fs::make_path(public_key_path_parts)
	};

	const std::string cert_file
	{
		fs::make_path(certificate_path_parts)
	};

	if(!fs::exists(private_key_file))
	{
		log::warning
		{
			"Failed to find certificate private key @ `%s'; creating...",
			private_key_file
		};

		openssl::genrsa(private_key_file, public_key_file);
	}
*/

/*
	const std::string dhparam_file
	{
		fs::make_path(dhparam_path_parts)
	};

	if(!fs::exists(dhparam_file))
	{
		log::warning
		{
			"Failed to find dhparam file @ `%s'; creating; "
			"this will take about 2 to 5 minutes...",
			dhparam_file
		};

		openssl::gendh(dhparam_file);
	}
*/

/*
	const json::object config{};
	if(!fs::exists(cert_file))
	{
		std::string subject
		{
			config.get({"certificate", m::self::origin, "subject"})
		};

		if(!subject)
			subject = json::strung{json::members
			{
				{ "CN", m::self::origin }
			}};

		log::warning
		{
			"Failed to find SSL certificate @ `%s'; creating for '%s'...",
			cert_file,
			m::self::origin
		};

		const unique_buffer<mutable_buffer> buf
		{
			1_MiB
		};

		const json::strung opts{json::members
		{
			{ "private_key_pem_path",  private_key_file  },
			{ "public_key_pem_path",   public_key_file   },
			{ "subject",               subject           },
		}};

		const auto cert
		{
			openssl::genX509_rsa(buf, opts)
		};

		fs::overwrite(cert_file, cert);
	}

	const auto cert_pem
	{
		fs::read(cert_file)
	};

	const unique_buffer<mutable_buffer> der_buf
	{
		8_KiB
	};

	const auto cert_der
	{
		openssl::cert2d(der_buf, cert_pem)
	};

	const fixed_buffer<const_buffer, crh::sha256::digest_size> hash
	{
		sha256{cert_der}
	};

	m::self::tls_cert_der_sha256_b64 =
	{
		b64encode_unpadded(hash)
	};

	log::info
	{
		m::log, "Certificate `%s' :PEM %zu bytes; DER %zu bytes; sha256b64 %s",
		cert_file,
		cert_pem.size(),
		ircd::size(cert_der),
		m::self::tls_cert_der_sha256_b64
	};

	const unique_buffer<mutable_buffer> print_buf
	{
		8_KiB
	};

	log::info
	{
		m::log, "Certificate `%s' :%s",
		cert_file,
		openssl::print_subject(print_buf, cert_pem)
	};
*/
}
