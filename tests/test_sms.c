/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2026
 *
 * Vladimir Serbinenko <phcoder@gmail.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*!
 * \file
 * \brief Unit Tests for smslib API
 *
 * \author Vladimir Serbinenko <phcoder@gmail.com>
 */

/*** MODULEINFO
	<depend>TEST_FRAMEWORK</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/test.h"
#include "asterisk/module.h"

#include "../apps/smslib.h"

AST_TEST_DEFINE(smslib_test)
{
	int res = AST_TEST_PASS;
	char oa[300];
	unsigned char tpdu_addr[] = { 0x0e, 0xd0, 0xd3, 0x7b, 0x7a, 0x3e, 0x1f, 0xbf, 0xdb };

	switch (cmd) {
	case TEST_INIT:
		info->name = "smslib_test";
		info->category = "/apps/sms/";
		info->summary = "decode a simple TPDU";
		info->description = "decode a simple TPDU";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	if (9 != unpackaddress(oa, tpdu_addr, sizeof(oa)))
	{
		ast_test_status_update(test, "error: 'unpackaddress' returned wrong number of bytes\n");
		res = AST_TEST_FAIL;
	}

	if (strcmp(oa, "Swisscom") != 0)
	{
		ast_test_status_update(test, "error: oa wrong. Actual '%s', expected 'Swisscom'\n", oa);
		res = AST_TEST_FAIL;
	}

	return res;
}

static int unload_module(void)
{
	AST_TEST_UNREGISTER(smslib_test);
	return 0;
}

static int load_module(void)
{
	AST_TEST_REGISTER(smslib_test);
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Smslib test module",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module
);
