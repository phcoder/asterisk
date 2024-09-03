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
#include "asterisk/res_pjsip_session.h"

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
static const pj_str_t STR_SUPPORTED_HDR = DEF_STR("Supported");
static const pj_str_t STR_SUPPORTED_VAL = DEF_STR("supported");
static const pj_str_t STR_PRECONDITION = DEF_STR("precondition");
static const pj_str_t STR_P_EARLY_MEDIA = DEF_STR("P-Early-Media");
static const pj_str_t STR_RECVONLY = DEF_STR("recvonly");
static const pj_str_t STR_BANDW_MODIFIER_AS = DEF_STR("AS");
static const pj_str_t STR_BANDW_MODIFIER_RS = DEF_STR("RS");
static const pj_str_t STR_BANDW_MODIFIER_RR = DEF_STR("RR");

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

pj_status_t volte_add_precondition(pjsip_tx_data *tdata, pj_bool_t supported)
{
	pj_bool_t created = PJ_FALSE;
	pj_status_t status;

	status = PJ_SUCCESS;
	if (supported) {
		/* "Supported: precondition" */
		pjsip_supported_hdr *hdr;
		hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_SUPPORTED_HDR, NULL);
		if (!hdr) {
			hdr = pjsip_supported_hdr_create(tdata->pool);
			if (hdr)
				created = PJ_TRUE;
			else
				status = PJ_ENOMEM;
		}
		if (hdr) {
			hdr->values[hdr->count++] = STR_PRECONDITION;
		}
		if (created)
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
	} else {
		/* "Require: precondition" */
		pjsip_require_hdr *hdr;
		hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_REQUIRE, NULL);
		if (!hdr) {
			hdr = pjsip_require_hdr_create(tdata->pool);
			if (hdr)
				created = PJ_TRUE;
			else
				status = PJ_ENOMEM;
		}
		if (hdr) {
			hdr->values[hdr->count++] = STR_PRECONDITION;
		}
		if (created)
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
	}

	return status;
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

pj_status_t volte_add_p_early_media_supported(pjsip_tx_data *tdata)
{
	pj_status_t status;

	/* "P-Early-Media" */
	status = add_value_string_hdr(tdata, &STR_P_EARLY_MEDIA, &STR_SUPPORTED_VAL);
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

/* Sender and receiver of INVITE will start with this status table. */
void volte_init_sdp_qos(struct ast_sip_session_qos_status *local_status)
{
	memset(local_status, 0, sizeof(*local_status));
	local_status->local_send_curr = PJ_FALSE;
	local_status->local_recv_curr = PJ_FALSE;
	local_status->remote_send_curr = PJ_FALSE;
	local_status->remote_recv_curr = PJ_FALSE;
	local_status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
	local_status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
	local_status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
	local_status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
	local_status->remote_conf_set = PJ_FALSE;
	local_status->remote_send_conf = PJ_FALSE;
	local_status->remote_recv_conf = PJ_FALSE;
};

static const char *send_recv_str(pj_bool_t send, pj_bool_t recv)
{
	if (!send && !recv)
		return "none";
	if (send && recv)
		return "sendrecv";
	if (send)
		return "send";
	else
		return "recv";
}

static const char *strength_str(enum ast_sip_session_qos_status_strength strength)
{
	switch (strength) {
	case AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE:
		return "none";
	case AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL:
		return "optional";
	case AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY:
		return "mandatory";
	}

	return "invalid";
}

/* Parse remote QOS attributes from SDP. */
pj_status_t volte_parse_sdp_qos(const pjmedia_sdp_media *media, struct ast_sip_session_qos_status *status)
{
	pj_bool_t got_local_curr = PJ_FALSE;
	pj_bool_t got_remote_curr = PJ_FALSE;
	pj_bool_t got_local_send_des = PJ_FALSE;
	pj_bool_t got_local_recv_des = PJ_FALSE;
	pj_bool_t got_remote_send_des = PJ_FALSE;
	pj_bool_t got_remote_recv_des = PJ_FALSE;
	int i;

	memset(status, 0, sizeof(*status));

	for (i = 0; i < media->attr_count; i++) {
		if (!pj_strcmp2(&media->attr[i]->name, "curr")) {
			if (!pj_strcmp2(&media->attr[i]->value, "qos local none")) {
				status->local_send_curr = PJ_FALSE;
				status->local_recv_curr = PJ_FALSE;
				got_local_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos local sendrecv")) {
				status->local_send_curr = PJ_TRUE;
				status->local_recv_curr = PJ_TRUE;
				got_local_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos local send")) {
				status->local_send_curr = PJ_TRUE;
				status->local_recv_curr = PJ_FALSE;
				got_local_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos local recv")) {
				status->local_send_curr = PJ_FALSE;
				status->local_recv_curr = PJ_TRUE;
				got_local_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote none")) {
				status->remote_send_curr = PJ_FALSE;
				status->remote_recv_curr = PJ_FALSE;
				got_remote_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote sendrecv")) {
				status->remote_send_curr = PJ_TRUE;
				status->remote_recv_curr = PJ_TRUE;
				got_remote_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote send")) {
				status->remote_send_curr = PJ_TRUE;
				status->remote_recv_curr = PJ_FALSE;
				got_remote_curr = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote recv")) {
				status->remote_send_curr = PJ_FALSE;
				status->remote_recv_curr = PJ_TRUE;
				got_remote_curr = PJ_TRUE;
			}
		}
		if (!pj_strcmp2(&media->attr[i]->name, "des")) {
			if (!pj_strcmp2(&media->attr[i]->value, "qos none local send")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_local_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional local send")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_local_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory local send")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_local_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos none local recv")) {
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional local recv")) {
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory local recv")) {
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos none local sendrecv")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_local_send_des = PJ_TRUE;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional local sendrecv")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_local_send_des = PJ_TRUE;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory local sendrecv")) {
				status->local_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				status->local_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_local_send_des = PJ_TRUE;
				got_local_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos none remote send")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_remote_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional remote send")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_remote_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory remote send")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_remote_send_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos none remote recv")) {
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_remote_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional remote recv")) {
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_remote_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory remote recv")) {
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_remote_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos none remote sendrecv")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_NONE;
				got_remote_send_des = PJ_TRUE;
				got_remote_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos optional remote sendrecv")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_OPTIONAL;
				got_remote_send_des = PJ_TRUE;
				got_remote_recv_des = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos mandatory remote sendrecv")) {
				status->remote_send_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				status->remote_recv_des = AST_SIP_SESSION_QOS_STATUS_STRENGTH_MANDATORY;
				got_remote_send_des = PJ_TRUE;
				got_remote_recv_des = PJ_TRUE;
			}
		}
		if (!pj_strcmp2(&media->attr[i]->name, "conf")) {
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote none")) {
				status->remote_send_conf = PJ_FALSE;
				status->remote_recv_conf = PJ_FALSE;
				status->remote_conf_set = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote sendrecv")) {
				status->remote_send_conf = PJ_TRUE;
				status->remote_recv_conf = PJ_TRUE;
				status->remote_conf_set = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote send")) {
				status->remote_send_conf = PJ_TRUE;
				status->remote_recv_conf = PJ_FALSE;
				status->remote_conf_set = PJ_TRUE;
			}
			if (!pj_strcmp2(&media->attr[i]->value, "qos remote recv")) {
				status->remote_send_conf = PJ_FALSE;
				status->remote_recv_conf = PJ_TRUE;
				status->remote_conf_set = PJ_TRUE;
			}
		}
	}

	if (!got_local_curr || !got_remote_curr) {
		ast_log(LOG_ERROR, "Failed to parse SDP attributes, missing or incomplete 'curr:qos' attributes.");
		return PJ_EINVAL;
	}
	if (!got_local_send_des || !got_local_recv_des || !got_remote_send_des || !got_remote_recv_des) {
		ast_log(LOG_ERROR, "Failed to parse SDP attributes, missing or incomplete 'des:qos' attributes.");
		return PJ_EINVAL;
	}

	return PJ_SUCCESS;
}

/* Generate SDP from local QOS attributes. */
pj_status_t volte_add_sdp_qos(pj_pool_t *pool, pjmedia_sdp_media *media, struct ast_sip_session_qos_status *status)
{
	const char *str_dir, *str_strength;
	char tmp[64];
	pj_str_t str_tmp;
	pjmedia_sdp_attr *attr;

	/* Set current status table. */
	str_dir = send_recv_str(status->local_send_curr, status->local_recv_curr);
	sprintf(tmp, "qos local %s", str_dir);
	pj_strdup2(pool, &str_tmp, tmp);
	attr = pjmedia_sdp_attr_create(pool, "curr", &str_tmp);
	if (attr)
		pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
	else
		goto fail;

	str_dir = send_recv_str(status->remote_send_curr, status->remote_recv_curr);
	sprintf(tmp, "qos remote %s", str_dir);
	pj_strdup2(pool, &str_tmp, tmp);
	attr = pjmedia_sdp_attr_create(pool, "curr", &str_tmp);
	if (attr)
		pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
	else
		goto fail;

	str_strength = strength_str(status->local_send_des);
	str_dir = send_recv_str(PJ_TRUE, (status->local_send_des == status->local_recv_des));
	sprintf(tmp, "qos %s local %s", str_strength, str_dir);
	pj_strdup2(pool, &str_tmp, tmp);
	attr = pjmedia_sdp_attr_create(pool, "des", &str_tmp);
	if (attr)
		pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
	else
		goto fail;
	if (status->local_send_des != status->local_recv_des) {
		/* Add second line, if both stregths differ (not sendrecv) */
		str_strength = strength_str(status->local_recv_des);
		str_dir = send_recv_str(PJ_FALSE, PJ_TRUE);
		sprintf(tmp, "qos %s local %s", str_strength, str_dir);
		pj_strdup2(pool, &str_tmp, tmp);
		attr = pjmedia_sdp_attr_create(pool, "des", &str_tmp);
		if (attr)
			pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
		else
			goto fail;
	}
	str_strength = strength_str(status->remote_send_des);
	str_dir = send_recv_str(PJ_TRUE, (status->remote_send_des == status->remote_recv_des));
	sprintf(tmp, "qos %s remote %s", str_strength, str_dir);
	pj_strdup2(pool, &str_tmp, tmp);
	attr = pjmedia_sdp_attr_create(pool, "des", &str_tmp);
	if (attr)
		pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
	else
		goto fail;
	if (status->remote_send_des != status->remote_recv_des) {
		/* Add second line, if both stregths differ (not sendrecv) */
		str_strength = strength_str(status->remote_recv_des);
		str_dir = send_recv_str(PJ_FALSE, PJ_TRUE);
		sprintf(tmp, "qos %s remote %s", str_strength, str_dir);
		pj_strdup2(pool, &str_tmp, tmp);
		attr = pjmedia_sdp_attr_create(pool, "des", &str_tmp);
		if (attr)
			pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
		else
			goto fail;
	}

	if (status->remote_conf_set) {
		str_dir = send_recv_str(status->remote_send_conf, status->remote_recv_conf);
		sprintf(tmp, "qos remote %s", str_dir);
		pj_strdup2(pool, &str_tmp, tmp);
		attr = pjmedia_sdp_attr_create(pool, "conf", &str_tmp);
		if (attr)
			pjmedia_sdp_attr_add(&media->attr_count, media->attr, attr);
		else
			goto fail;
	}

	return PJ_SUCCESS;

fail:
	ast_log(LOG_ERROR, "Failed to create SDP attributes.");
	return PJ_ENOMEM;
}

static pj_status_t add_bandwidth_session(pj_pool_t *pool, pjmedia_sdp_session *session, const pj_str_t *modifier,
					 pj_uint32_t value)
{
	pjmedia_sdp_bandw *bandw;
	if (session->bandw_count == PJMEDIA_MAX_SDP_BANDW) {
		ast_log(LOG_ERROR, "Too many bandwidth entries? Please fix!");
		return PJ_EINVAL;
	}

	bandw = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
	bandw->modifier = *modifier;
	bandw->value = value;
	session->bandw[session->bandw_count++] = bandw;

	return PJ_SUCCESS;
}

static pj_status_t add_bandwidth_media(pj_pool_t *pool, pjmedia_sdp_media *media, const pj_str_t *modifier,
				       pj_uint32_t value)
{
	pjmedia_sdp_bandw *bandw;
	if (media->bandw_count == PJMEDIA_MAX_SDP_BANDW) {
		ast_log(LOG_ERROR, "Too many bandwidth entries? Please fix!");
		return PJ_EINVAL;
	}

	bandw = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
	bandw->modifier = *modifier;
	bandw->value = value;
	media->bandw[media->bandw_count++] = bandw;

	return PJ_SUCCESS;
}

/* Add bandwidth line to SDP */
pj_status_t volte_add_sdp_bandwidth_session(pj_pool_t *pool, pjmedia_sdp_session *session, pj_uint32_t bw_value)
{
	add_bandwidth_session(pool, session, &STR_BANDW_MODIFIER_AS, bw_value);
	add_bandwidth_session(pool, session, &STR_BANDW_MODIFIER_RS, 600);
	add_bandwidth_session(pool, session, &STR_BANDW_MODIFIER_RR, 2000);

	return PJ_SUCCESS;
}

/* Add bandwidth line to media of SDP */
pj_status_t volte_add_sdp_bandwidth_media(pj_pool_t *pool, pjmedia_sdp_media *media, pj_uint32_t bw_value)
{
	add_bandwidth_media(pool, media, &STR_BANDW_MODIFIER_AS, bw_value);
	add_bandwidth_media(pool, media, &STR_BANDW_MODIFIER_RS, 600);
	add_bandwidth_media(pool, media, &STR_BANDW_MODIFIER_RR, 2000);

	return PJ_SUCCESS;
}

//#define DEBUG_NEGOTIATION

/* Use remote status table to update local status table. */
pj_status_t volte_negotiate_sdp_qos(struct ast_sip_session_qos_status *local_status,
				    struct ast_sip_session_qos_status *remote_status,
				    const char *reason)
{
	/* Note that the remote_status is reverse: send <-> receive, local <-> remote */

#ifdef DEBUG_NEGOTIATION
	printf("Reason for negotiation: %s\n", reason);
	printf("Local table:\n");
	printf("  local send state   = %d, local recv state   = %d\n",
	       local_status->local_send_curr, local_status->local_recv_curr);
	printf("  remote send state  = %d, remote recv state  = %d\n",
	       local_status->remote_send_curr, local_status->remote_recv_curr);
	printf("  local send desire  = %d, local recv desire  = %d\n",
	       local_status->local_send_des, local_status->local_recv_des);
	printf("  remote send desire = %d, remote recv desire = %d\n",
	       local_status->remote_send_des, local_status->remote_recv_des);
	printf("Remote table:\n");
	printf("  local send state   = %d, local recv state   = %d\n",
	       remote_status->local_send_curr, remote_status->local_recv_curr);
	printf("  remote send state  = %d, remote recv state  = %d\n",
	       remote_status->remote_send_curr, remote_status->remote_recv_curr);
	printf("  local send desire  = %d, local recv desire  = %d\n",
	       remote_status->local_send_des, remote_status->local_recv_des);
	printf("  remote send desire = %d, remote recv desire = %d\n",
	       remote_status->remote_send_des, remote_status->remote_recv_des);
#endif

	/* Get current remote status. */
	if (local_status->remote_recv_curr != remote_status->local_send_curr) {
#ifdef DEBUG_NEGOTIATION
		printf("Remote receive state changes from %d to %d\n",
		       local_status->remote_recv_curr, remote_status->local_send_curr);
#endif
		local_status->remote_recv_curr = remote_status->local_send_curr;
	}
	if (local_status->remote_send_curr != remote_status->local_recv_curr) {
#ifdef DEBUG_NEGOTIATION
		printf("Remote send state changes from %d to %d\n",
		       local_status->remote_send_curr, remote_status->local_recv_curr);
#endif
		local_status->remote_send_curr = remote_status->local_recv_curr;
	}

	/* Upgrade desired status. */
	if (remote_status->remote_recv_des > local_status->local_send_des) {
#ifdef DEBUG_NEGOTIATION
		printf("Local receive desired is upgraded from %d to %d\n",
		       local_status->local_recv_des, remote_status->remote_send_des);
#endif
		local_status->local_recv_des = remote_status->remote_send_des;
	}
	if (remote_status->remote_send_des > local_status->local_recv_des) {
#ifdef DEBUG_NEGOTIATION
		printf("Local send desired is upgraded from %d to %d\n",
		       local_status->local_send_des, remote_status->remote_recv_des);
#endif
		local_status->local_send_des = remote_status->remote_recv_des;
	}
	if (remote_status->local_recv_des > local_status->remote_send_des) {
#ifdef DEBUG_NEGOTIATION
		printf("Remote send desired is upgraded from %d to %d\n",
		       local_status->remote_send_des, remote_status->local_recv_des);
#endif
		local_status->remote_send_des = remote_status->local_recv_des;
	}
	if (remote_status->local_send_des > local_status->remote_recv_des) {
#ifdef DEBUG_NEGOTIATION
		printf("Remote receive desired is upgraded from %d to %d\n",
		       local_status->remote_recv_des, remote_status->local_send_des);
#endif
		local_status->remote_recv_des = remote_status->local_send_des;
	}

	return PJ_SUCCESS;
}

/* Request confirm on precondition change. This will be done by UPDATE. */
pj_status_t volte_confirm_sdp_qos(struct ast_sip_session_qos_status *local_status)
{
#ifdef DEBUG_NEGOTIATION
	printf("Remote send confirm is set\n");
	printf("Remote receive confirm is set\n");
#endif
	local_status->remote_send_conf = PJ_TRUE;
	local_status->remote_recv_conf = PJ_TRUE;
	local_status->remote_conf_set = PJ_TRUE;

	return PJ_SUCCESS;
}

/* React on precondition up and prepare for UPDATE. */
pj_status_t volte_update_sdp_qos(struct ast_sip_session_qos_status *local_status)
{
	/* Set precondition to up. */
	local_status->local_send_curr = PJ_TRUE;
	local_status->local_recv_curr = PJ_TRUE;

	/* This is the confirmation, so we don't send it in the update. */
	local_status->remote_send_conf = PJ_FALSE;
	local_status->remote_recv_conf = PJ_FALSE;
	local_status->remote_conf_set = PJ_FALSE;

	return PJ_SUCCESS;
}

pj_bool_t volte_is_supported_precondition(pjsip_rx_data *rdata)
{
	pjsip_supported_hdr *sup_hdr = pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_SUPPORTED, NULL);
	int i;

	if (!sup_hdr)
		return PJ_FALSE;

	for (i = 0; i < sup_hdr->count; i++) {
		if (!pj_stricmp2(&sup_hdr->values[i], "precondition"))
			return PJ_TRUE;
	}

	return PJ_FALSE;
}
