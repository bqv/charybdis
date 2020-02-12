// The Construct
//
// Copyright (C) The Construct Developers
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <llvm/Object/Wasm.h>

namespace ircd::llvm
{
	static void init(), fini();
}

ircd::mapi::header
IRCD_MODULE
{
	"LLVM Compiler Infrastructure",
	ircd::llvm::init,
	ircd::llvm::fini,
};

void
ircd::llvm::init()
{

}

void
ircd::llvm::fini()
{

}