/*
 * Asterisk -- An open source telephony toolkit.
 *
 * (C) 2024 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * Author: Andreas Eversberg
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
#include "asterisk/utils.h"
#include "asterisk/manager.h"
#include "asterisk/res_pjsip.h"

#include "volte.h"

#define DEF_STR(str) { str, sizeof(str) - 1 }

static const pj_str_t STR_ACCEPT = DEF_STR("Accept");
static const pj_str_t STR_ACCEPT_CONTACT = DEF_STR("Accept-Contact");
static const pj_str_t STR_MIME_APP_SDP = DEF_STR("application/sdp");
static const pj_str_t STR_MIME_APP_3GPP_IMS_XML = DEF_STR("application/3gpp-ims+xml");
static const pj_str_t STR_SECURITY_VERIFY = DEF_STR("Security-Verify");
static const pj_str_t STR_REQUIRE = DEF_STR("Require");
static const pj_str_t STR_P_PREFERRED_SERVICE = DEF_STR("P-Preferred-Service");
static const pj_str_t STR_PROXY_REQUIRE = DEF_STR("Proxy-Require");
static const pj_str_t STR_SEC_AGREE = DEF_STR("sec-agree");
static const pj_str_t STR_P_ACCESS_NETWORK_INFO = DEF_STR("P-Access-Network-Info");
static const pj_str_t STR_P_EARLY_MEDIA = DEF_STR("P-Early-Media");
static const pj_str_t STR_RECVONLY = DEF_STR("recvonly");

/* Create string header and add given value. */
static pj_status_t add_value_string_hdr(pjsip_tx_data *tdata, const pj_str_t *name, const pj_str_t *value)
{
	pjsip_generic_string_hdr *hdr;

	/* Add header. */
	hdr = pjsip_generic_string_hdr_create(tdata->pool, name, value);
	if (!hdr) {
		ast_log(LOG_ERROR, "Failed to create string header.");
		return -ENOMEM;
	}

	/* Append header */
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *)hdr);

	return PJ_SUCCESS;
}

/* 3GPP TS 24.229 5.1.3.1: Add "Accept: application/sdp,application/3gpp-ims+xml" to header. */
pj_status_t volte_add_accept(pjsip_tx_data *tdata)
{
	pj_bool_t created = PJ_FALSE;
	pjsip_accept_hdr *hdr;
	pj_status_t status;

	status = PJ_SUCCESS;
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_ACCEPT, NULL);
	if (!hdr) {
		hdr = pjsip_accept_hdr_create(tdata->pool);
		if (hdr)
			created = PJ_TRUE;
		else
			status = PJ_ENOMEM;
	}
	if (hdr) {
		hdr->values[hdr->count++] = STR_MIME_APP_SDP;
		hdr->values[hdr->count++] = STR_MIME_APP_3GPP_IMS_XML;
	}
	if (created)
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);

	return status;
}

pj_status_t volte_add_accept_contact(pjsip_tx_data *tdata, char *info)
{
	const pj_str_t contact = { info, strlen(info) };
	pj_status_t status;

	status = add_value_string_hdr(tdata, &STR_ACCEPT_CONTACT, &contact);
	if (status)
		return status;

	return PJ_SUCCESS;
}

pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata)
{
	const pj_str_t security = { transport_state->volte.security_server,
				    strlen(transport_state->volte.security_server)};
	pj_status_t status;

	status = add_value_string_hdr(tdata, &STR_SECURITY_VERIFY, &security);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add Sec-Agree to header. */
pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata)
{
	pj_bool_t created = PJ_FALSE;
	pjsip_require_hdr *hdr;
	pj_status_t status;

	/* "Require: sec-agree" */
	status = PJ_SUCCESS;
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_REQUIRE, NULL);
	if (!hdr) {
		hdr = pjsip_require_hdr_create(tdata->pool);
		if (hdr)
			created = PJ_TRUE;
		else
			status = PJ_ENOMEM;
	}
	if (hdr) {
		hdr->values[hdr->count++] = STR_SEC_AGREE;
	}
	if (created)
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
	if (status)
		return status;

	/* "Proxy-Require: sec-agree" */
	status = add_value_string_hdr(tdata, &STR_PROXY_REQUIRE, &STR_SEC_AGREE);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add P-Access-Network-Info to header. */
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info)
{
	pj_status_t status;

	const pj_str_t info_str = { info, strlen(info) };

	/* "P-Access-Network-Info" */
	status = add_value_string_hdr(tdata, &STR_P_ACCESS_NETWORK_INFO, &info_str);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add P-Preferred-Service to header. */
pj_status_t volte_add_p_preferred_service(pjsip_tx_data *tdata, char *info)
{
	pj_status_t status;

	const pj_str_t info_str = { info, strlen(info) };

	/* "P-Access-Network-Info" */
	status = add_value_string_hdr(tdata, &STR_P_PREFERRED_SERVICE, &info_str);
	if (status)
		return status;

	return PJ_SUCCESS;
}

pj_status_t volte_add_p_early_media_recvonly(pjsip_tx_data *tdata)
{
	pj_status_t status;

	/* "P-Early-Media" */
	status = add_value_string_hdr(tdata, &STR_P_EARLY_MEDIA, &STR_RECVONLY);
	if (status)
		return status;

	return PJ_SUCCESS;
}

void volte_add_contact_params(pjsip_tx_data *tdata, const char **params)
{
	pjsip_contact_hdr *contact;
	pjsip_sip_uri *uri;
	pjsip_param *p;
	pj_str_t name, value;
	char uuid_buf[AST_UUID_STR_LEN];

	contact = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
	if (!contact) {
		return;
	}

	uri = pjsip_uri_get_uri(contact->uri);
	if (uri) {
		ast_pbx_uuid_get(uuid_buf, sizeof(uuid_buf));
		pj_strdup2(tdata->pool, &uri->user, uuid_buf);
	}

	while (*params) {
		p = PJ_POOL_ALLOC_T(tdata->pool, pjsip_param);
		if (!p) {
			ast_log(LOG_ERROR, "No memory\n");
			return;
		}
		pj_cstr(&name, *params++);
		pj_cstr(&value, *params++);
		p->name = name;
		p->value = value;
		pj_list_insert_before(&contact->other_param, p);
	}
}
