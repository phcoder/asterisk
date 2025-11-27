/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Andreas Eversberg <aeversberg@sysmocom.de>
 * Based on code by Joshua Colp <jcolp@digium.com>
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

#include "asterisk.h"

#include <pjsip.h>
#include <pjlib.h>

#include "asterisk/res_pjsip.h"
#include "include/res_pjsip_private.h"
#include "asterisk/logger.h"
#include "asterisk/sorcery.h"

static void resolve_destroy(void *obj)
{
	struct ast_sip_resolve *resolve = obj;

	ast_string_field_free_memory(resolve);
}

static void *resolve_alloc(const char *name)
{
	struct ast_sip_resolve *resolve;

	resolve = ast_sorcery_generic_alloc(sizeof(*resolve), resolve_destroy);
	if (!resolve) {
		return NULL;
	}

	if (ast_string_field_init(resolve, 256)) {
		ao2_cleanup(resolve);
		return NULL;
	}

	return resolve;
}

/*! \brief Apply handler for resolve type */
static int resolve_apply(const struct ast_sorcery *sorcery, void *obj)
{
	struct ast_sip_resolve *resolve = obj;

	if (ast_strlen_zero(resolve->ip)) {
		ast_log(LOG_ERROR, "%s '%s' missing required IP being resolved.\n",
			SIP_SORCERY_RESOLVE_TYPE, ast_sorcery_object_get_id(resolve));
		return -1;
	}
	return 0;
}

/*! \brief Initialize sorcery with domain alias support */
int ast_sip_initialize_sorcery_resolve(void)
{
	struct ast_sorcery *sorcery = ast_sip_get_sorcery();

	ast_sorcery_apply_default(sorcery, SIP_SORCERY_RESOLVE_TYPE, "config", "pjsip.conf,criteria=type=resolve");

	puts("a");
	if (ast_sorcery_object_register(sorcery, SIP_SORCERY_RESOLVE_TYPE,
		resolve_alloc, NULL, resolve_apply)) {
	puts("b");
		return -1;
	}

	ast_sorcery_object_field_register(sorcery, SIP_SORCERY_RESOLVE_TYPE, "type", "",
			OPT_NOOP_T, 0, 0);
	ast_sorcery_object_field_register(sorcery, SIP_SORCERY_RESOLVE_TYPE, "ip",
			"", OPT_STRINGFIELD_T, 0, STRFLDSET(struct ast_sip_resolve, ip));
	ast_sorcery_object_field_register(sorcery, SIP_SORCERY_RESOLVE_TYPE, "transport",
			"", OPT_STRINGFIELD_T, 0, STRFLDSET(struct ast_sip_resolve, transport));

	return 0;
}
