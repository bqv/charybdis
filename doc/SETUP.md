## SETUP

This guide will help you execute Construct for the first time. If you are
building from source code and have not already done so please follow the
instructions in [BUILD](BUILD.md) before continuing here.

#### NOTES

- We will refer to your server as `host.tld`. For those familiar with matrix:
this is your _origin_ and mxid `@user:host.tld` hostpart. If you delegate
your server's location to something like `matrix.host.tld:1234` we refer to
this as your _servername_.

> Construct clusters all share the same _origin_ but each individual instance
of the daemon has a unique _servername_.

- If you built construct yourself as a standalone build you will need to add
the included library directories before executing:
`export LD_LIBRARY_PATH=/path/to/src/deps/boost/lib:$LD_LIBRARY_PATH`
`export LD_LIBRARY_PATH=/path/to/src/deps/rocksdb:$LD_LIBRARY_PATH`

### PROCEDURE

1. Execute

	There are two arguments: `<origin> [servername]`. If the _servername_
	argument is missing, the _origin_ will be used for it instead.

	```
	bin/construct host.tld
	````
	> There is no configuration file.

	> Log messages will appear in terminal concluding with notice `IRCd RUN`.


2. Strike ctrl-c on keyboard
	> The command-line console will appear.


3. Create a general listener socket by entering the following command:

	```
	net listen matrix * 8448 privkey.pem cert.pem chain.pem
	```
	- `matrix` is your name for this listener; you can use any name.
	- `*` and `8448` is the local address and port to bind.
	- `privkey.pem` and `cert.pem` and `chain.pem` are paths (ideally
	absolute paths) to PEM-format files for the listener's TLS.

	> The Matrix Federation Tester should now pass. Browse to
	https://matrix.org/federationtester/api/report?server_name=host.tld and
	verify `"AllChecksOK": true`

4. To use a web-based client like Riot, configure the "web root" directory
to point at Riot's `webapp/` directory by entering the following:
	```
	conf set ircd.web.root.path /path/to/riot-web/webapp/
	mod reload web_root
	```

6. Browse to `https://host.tld:8448/` and register a user.


### ADDENDUM

* If you are employing a reverse-proxy you must review the apropos section in
the [TROUBLESHOOTING](TROUBLESHOOTING.md#trouble-with-reverse-proxies-and-middlewares)
guide or the server may not operate correctly.

* Logging to files is only enabled by default for CRITICAL, ERROR, and WARNING.
It is not enabled by default for the INFO level. To enable, use `conf set
ircd.log.info.file.enable true`.
