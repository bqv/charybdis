################################################################################
#
# IRCd Modules
#

AM_CXXFLAGS = \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.pic.h \
	-include $(top_srcdir)/include/ircd/mods/mapi.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-avoid-version \
	-module \
	-Wl,-fuse-ld=gold \
	-Wl,--shared \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--gc-sections \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-L$(top_srcdir)/ircd \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	# -Wl,--no-undefined \
	# -export-symbols-regex IRCD_MODULE* \
	# -export-symbols-regex * \
	###

###############################################################################
#
# /
#

moduledir = @moduledir@

webroot_la_SOURCES = webroot.cc
console_la_SOURCES = console.cc

module_LTLIBRARIES = \
	webroot.la \
	console.la \
	###

###############################################################################
#
# Server modules
#

s_moduledir = @moduledir@

s_conf_la_SOURCES = s_conf.cc
s_dns_la_SOURCES =  s_dns.cc s_dns_cache.cc s_dns_resolver.cc
s_listen_la_SOURCES = s_listen.cc
s_keys_la_SOURCES = s_keys.cc

s_module_LTLIBRARIES = \
	s_conf.la \
	s_dns.la \
	s_listen.la \
	s_keys.la \
	###

###############################################################################
#
# Messages / protocol
#

m_moduledir = @moduledir@

m_noop_la_SOURCES = m_noop.cc

m_module_LTLIBRARIES = \
	m_noop.la \
	###

###############################################################################
#
# /_matrix/media/
#

media_moduledir = @moduledir@

media_media_media_la_SOURCES = \
	media/download.cc \
	media/upload.cc \
	media/media.cc \
	###

media_module_LTLIBRARIES = \
	media/media_media.la \
	###

###############################################################################
#
# JavaScript
#

if JS

js_moduledir = @moduledir@

js_js_console_la_SOURCES = js/console.cc
js_js_listener_la_SOURCES = js/listener.cc
js_js_kernel_la_SOURCES = js/kernel.cc

js_module_LTLIBRARIES = \
	js/js_console.la \
	js/js_listener.la \
	js/js_kernel.la \
	###

endif
