/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Kevin Harwell <kharwell@digium.com>
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

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<depend>res_pjsip_session</depend>
	<support_level>core</support_level>
 ***/

/*** DOCUMENTATION
	<info name="MessageDestinationInfo" language="en_US" tech="PJSIP">
		<para>The <literal>destination</literal> parameter is used to construct
		the Request URI for an outgoing message.  It can be in one of the following
		formats, all prefixed with the <literal>pjsip:</literal> message tech.</para>
		<para>
		</para>
		<enumlist>
			<enum name="endpoint">
				<para>Request URI comes from the endpoint's default aor and contact.</para>
			</enum>
			<enum name="endpoint/aor">
				<para>Request URI comes from the specific aor/contact.</para>
			</enum>
			<enum name="endpoint@domain">
				<para>Request URI from the endpoint's default aor and contact.  The domain is discarded.</para>
			</enum>
		</enumlist>
		<para>
		</para>
		<para>These all use the endpoint to send the message with the specified URI:</para>
		<para>
		</para>
		<enumlist>
 			<enum name="endpoint/&lt;sip[s]:host&gt;>"/>
 			<enum name="endpoint/&lt;sip[s]:user@host&gt;"/>
 			<enum name="endpoint/&quot;display name&quot; &lt;sip[s]:host&gt;"/>
 			<enum name="endpoint/&quot;display name&quot; &lt;sip[s]:user@host&gt;"/>
 			<enum name="endpoint/sip[s]:host"/>
 			<enum name="endpoint/sip[s]:user@host"/>
 			<enum name="endpoint/host"/>
 			<enum name="endpoint/user@host"/>
 		</enumlist>
		<para>
		</para>
		<para>These all use the default endpoint to send the message with the specified URI:</para>
		<para>
		</para>
 	 	<enumlist>
 			<enum name="&lt;sip[s]:host&gt;"/>
 			<enum name="&lt;sip[s]:user@host&gt;"/>
 			<enum name="&quot;display name&quot; &lt;sip[s]:host&gt;"/>
 			<enum name="&quot;display name&quot; &lt;sip[s]:user@host&gt;"/>
 			<enum name="sip[s]:host"/>
 			<enum name="sip[s]:user@host"/>
 	 	</enumlist>
		<para>
		</para>
 	 	<para>These use the default endpoint to send the message with the specified host:</para>
		<para>
		</para>
 	 	<enumlist>
 			<enum name="host"/>
 			<enum name="user@host"/>
 	 	</enumlist>
 		<para>
 		</para>
 		<para>This form is similar to a dialstring:</para>
		<para>
		</para>
		<enumlist>
			<enum name="PJSIP/user@endpoint"/>
		</enumlist>
		<para>
		</para>
		<para>You still need to prefix the destination with
		the <literal>pjsip:</literal> message technology prefix.  For example:
		<literal>pjsip:PJSIP/8005551212@myprovider</literal>.
		The endpoint contact's URI will have the <literal>user</literal> inserted
		into it and will become the Request URI.  If the contact URI already has
		a user specified, it will be replaced.
		</para>
		<para>
		</para>
	</info>
	<info name="MessageFromInfo" language="en_US" tech="PJSIP">
		<para>The <literal>from</literal> parameter is used to specity the <literal>From:</literal>
		header in the outgoing SIP MESSAGE.  It will override the value specified in
		MESSAGE(from) which itself will override any <literal>from</literal> value from
		an incoming SIP MESSAGE.
		</para>
 		<para>
 		</para>
	</info>
	<info name="MessageToInfo" language="en_US" tech="PJSIP">
		<para>The <literal>to</literal> parameter is used to specity the <literal>To:</literal>
		header in the outgoing SIP MESSAGE.  It will override the value specified in
		MESSAGE(to) which itself will override any <literal>to</literal> value from
		an incoming SIP MESSAGE.
		</para>
 		<para>
 		</para>
	</info>
 ***/
#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_ua.h>

#include "asterisk/message.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_session.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/test.h"
#include "asterisk/uri.h"
#include "asterisk/file.h"
#include "../apps/smslib.h"

const pjsip_method pjsip_message_method = {PJSIP_OTHER_METHOD, {"MESSAGE", 7} };

#define MAX_HDR_SIZE 512
#define MAX_BODY_SIZE 1024
#define MAX_USER_SIZE 128

static struct ast_taskprocessor *message_serializer;

static const char *volte_msg_contact_params[] = {
	NULL
};

static void volte_add_contact_params(pjsip_tx_data *tdata, pj_bool_t set_user, const char *contact_user, const char **params)
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

	if (set_user == PJ_TRUE) {
		uri = pjsip_uri_get_uri(contact->uri);
		if (uri) {
			if (contact_user && contact_user[0]) {
				pj_strdup2(tdata->pool, &uri->user, contact_user);
			} else {
				ast_pbx_uuid_get(uuid_buf, sizeof(uuid_buf));
				pj_strdup2(tdata->pool, &uri->user, uuid_buf);
			}
		}
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

/*!
 * \internal
 * \brief Checks to make sure the request has the correct content type.
 *
 * \details This module supports the following media types: "text/plain".
 * Return unsupported otherwise.
 *
 * \param rdata The SIP request
 */
static enum pjsip_status_code check_content_type(const pjsip_rx_data *rdata, pj_bool_t *is_sms_out)
{
	int is_plain = 0, is_sms = 0;
	if (rdata->msg_info.msg->body && rdata->msg_info.msg->body->len) {
		is_plain = ast_sip_is_content_type(
			&rdata->msg_info.msg->body->content_type, "text", "plain");
		is_sms = ast_sip_is_content_type(
			&rdata->msg_info.msg->body->content_type, "application", "vnd.3gpp.sms");
	} else if (rdata->msg_info.ctype) {
		 is_plain = ast_sip_is_content_type(
			 &rdata->msg_info.ctype->media, "text", "plain");
		 is_sms = ast_sip_is_content_type(
			 &rdata->msg_info.ctype->media, "application", "vnd.3gpp.sms");
	}

	*is_sms_out = is_sms;

	return is_plain || is_sms ? PJSIP_SC_OK : PJSIP_SC_UNSUPPORTED_MEDIA_TYPE;
}

/*!
 * \internal
 * \brief Checks to make sure the request has the correct content type.
 *
 * \details This module supports the following media types: "text/\*", "application/\*".
 * Return unsupported otherwise.
 *
 * \param rdata The SIP request
 */
static enum pjsip_status_code check_content_type_in_dialog(const pjsip_rx_data *rdata)
{
	int res = PJSIP_SC_UNSUPPORTED_MEDIA_TYPE;
	static const pj_str_t text = { "text", 4};
	static const pj_str_t application = { "application", 11};

	if (!(rdata->msg_info.msg->body && rdata->msg_info.msg->body->len > 0)) {
		return res;
	}

	/* We'll accept any text/ or application/ content type */
	if (pj_stricmp(&rdata->msg_info.msg->body->content_type.type, &text) == 0
			|| pj_stricmp(&rdata->msg_info.msg->body->content_type.type, &application) == 0) {
		res = PJSIP_SC_OK;
	} else if (rdata->msg_info.ctype
		&& (pj_stricmp(&rdata->msg_info.ctype->media.type, &text) == 0
		|| pj_stricmp(&rdata->msg_info.ctype->media.type, &application) == 0)) {
		res = PJSIP_SC_OK;
	}

	return res;
}

/*!
 * \internal
 * \brief Update the display name in the To uri in the tdata with the one from the supplied uri
 *
 * \param tdata the outbound message data structure
 * \param to uri containing the display name to replace in the the To uri
 *
 * \return 0: success, -1: failure
 */
static int update_to_display_name(pjsip_tx_data *tdata, char *to)
{
	pjsip_name_addr *parsed_name_addr;

	parsed_name_addr = (pjsip_name_addr *) pjsip_parse_uri(tdata->pool, to, strlen(to),
		PJSIP_PARSE_URI_AS_NAMEADDR);

	if (parsed_name_addr) {
		if (pj_strlen(&parsed_name_addr->display)) {
			pjsip_name_addr *name_addr =
				(pjsip_name_addr *) PJSIP_MSG_TO_HDR(tdata->msg)->uri;

			pj_strdup(tdata->pool, &name_addr->display, &parsed_name_addr->display);

		}
		return 0;
	}

	return -1;
}

/*!
 * \internal
 * \brief Checks if the given msg var name should be blocked.
 *
 * \details Some headers are not allowed to be overriden by the user.
 *  Determine if the given var header name from the user is blocked for
 *  an outgoing MESSAGE.
 *
 * \param name name of header to see if it is blocked.
 *
 * \retval TRUE if the given header is blocked.
 */
static int is_msg_var_blocked(const char *name)
{
	int i;

	/* Don't block the Max-Forwards header because the user can override it */
	static const char *hdr[] = {
		"To",
		"From",
		"Via",
		"Route",
		"Contact",
		"Call-ID",
		"CSeq",
		"Allow",
		"Content-Length",
		"Content-Type",
		"Request-URI",
	};

	for (i = 0; i < ARRAY_LEN(hdr); ++i) {
		if (!strcasecmp(name, hdr[i])) {
			/* Block addition of this header. */
			return 1;
		}
	}
	return 0;
}

/*!
 * \internal
 * \brief Copies any other msg vars over to the request headers.
 *
 * \param msg The msg structure to copy headers from
 * \param tdata The SIP transmission data
 */
static enum pjsip_status_code vars_to_headers(const struct ast_msg *msg, pjsip_tx_data *tdata)
{
	const char *name;
	const char *value;
	int max_forwards;
	struct ast_msg_var_iterator *iter;

	for (iter = ast_msg_var_iterator_init(msg);
		ast_msg_var_iterator_next(msg, iter, &name, &value);
		ast_msg_var_unref_current(iter)) {
		if (!strcasecmp(name, "Max-Forwards")) {
			/* Decrement Max-Forwards for SIP loop prevention. */
			if (sscanf(value, "%30d", &max_forwards) != 1 || --max_forwards == 0) {
				ast_msg_var_iterator_destroy(iter);
				ast_log(LOG_NOTICE, "MESSAGE(Max-Forwards) reached zero.  MESSAGE not sent.\n");
				return -1;
			}
			sprintf((char *) value, "%d", max_forwards);
			ast_sip_add_header(tdata, name, value);
		} else if (!is_msg_var_blocked(name)) {
			ast_sip_add_header(tdata, name, value);
		}
	}
	ast_msg_var_iterator_destroy(iter);

	return PJSIP_SC_OK;
}

/*!
 * \internal
 * \brief Copies any other request header data over to ast_msg structure.
 *
 * \param rdata The SIP request
 * \param msg The msg structure to copy headers into
 */
static int headers_to_vars(const pjsip_rx_data *rdata, struct ast_msg *msg)
{
	char *c;
	char name[MAX_HDR_SIZE];
	char buf[MAX_HDR_SIZE];
	int res = 0;
	pjsip_hdr *h = rdata->msg_info.msg->hdr.next;
	pjsip_hdr *end= &rdata->msg_info.msg->hdr;

	while (h != end) {
		if ((res = pjsip_hdr_print_on(h, buf, sizeof(buf)-1)) > 0) {
			buf[res] = '\0';
			if ((c = strchr(buf, ':'))) {
				ast_copy_string(buf, ast_skip_blanks(c + 1), sizeof(buf));
			}

			ast_copy_pj_str(name, &h->name, sizeof(name));
			if ((res = ast_msg_set_var(msg, name, buf)) != 0) {
				break;
			}
		}
		h = h->next;
	}
	return 0;
}

/*!
 * \internal
 * \brief Prints the message body into the given char buffer.
 *
 * \details Copies body content from the received data into the given
 * character buffer removing any extra carriage return/line feeds.
 *
 * \param rdata The SIP request
 * \param buf Buffer to fill
 * \param len The length of the buffer
 */
static int print_body(pjsip_rx_data *rdata, char *buf, int len)
{
	int res;

	if (!rdata->msg_info.msg->body || !rdata->msg_info.msg->body->len) {
		return 0;
	}

	if ((res = rdata->msg_info.msg->body->print_body(
		     rdata->msg_info.msg->body, buf, len)) < 0) {
		return res;
	}

	/* remove any trailing carriage return/line feeds */
	while (res > 0 && ((buf[--res] == '\r') || (buf[res] == '\n')));

	buf[++res] = '\0';

	return res;
}

static char hexdigit(int x)
{
	if (x< 10)
		return x +'0';
	return (x - 10) +'a';
}

static void hex_body(char *bufout, unsigned char *buf, int len)
{
	unsigned char *ptrin;
	char *ptrout = bufout;

	for (ptrin = buf; ptrin < buf + 256; ptrin++)
	{
		*ptrout++ = hexdigit((*ptrin >> 4) & 0xf);
		*ptrout++ = hexdigit((*ptrin) & 0xf);
	}
	*ptrout++ = 0;
}

static void parse_tpdu(struct ast_msg *msg, unsigned char *tpdu, int tpdu_len)
{
	if (tpdu_len < 2)
	{
		return;
	}
	if (tpdu[0] & 3)
	{
		ast_log(LOG_WARNING, "Unhandled PDU type %x\n", tpdu[0] & 3);
		return;
	}
	/*int srr = ((tpdu[0] & 0x20) ? 1 : 0);*/
	int udhi = ((tpdu[0] & 0x40) ? 1 : 0);
	/*int rp = ((tpdu[0] & 0x80) ? 1 : 0);*/
	int p = 1;
	char oa[300];
	p += unpackaddress(oa, tpdu + p, sizeof(oa));
	if (p + 9 > tpdu_len)
		return;
	/*int pid = tpdu[p++] */p++;
	int dcs = tpdu[p++];
	struct timeval scts = unpackdate(tpdu + p);
	p += 7;
	unsigned short ud[300];
	unsigned char udh[300];
	int udhl, udl;
	p += unpacksms(dcs, tpdu + p, udh, &udhl, ud, &udl, udhi);
	ud[udl] = 0;

	char buf2[300 * 4 + 5];
	utf16_to_utf8(ud, udl, buf2, sizeof(buf2));
	ast_log(LOG_DEBUG, "SMS UD='%s' OA='%s'.\n", buf2, oa);

	/* TODO: udh, scts */
	char buf_scts[30];
	snprintf(buf_scts, sizeof(buf_scts), "%lld", (long long) scts.tv_sec);
	ast_msg_set_var(msg, "SMS_SMSC_TIMESTAMP", buf_scts);
	ast_msg_set_from(msg, "%s", oa);
	ast_msg_set_body(msg, "%s", buf2);
}

#define DEF_STR(str) { str, sizeof(str) - 1 }

static const pj_str_t STR_IN_REPLY_TO = DEF_STR("In-Reply-To");

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

static void set_preferred_identity(pjsip_tx_data *tdata, const char *uri)
{
	char bracket_uri[1026];

	snprintf(bracket_uri, sizeof(bracket_uri), "<%s>", uri);
	ast_sip_add_header(tdata, "P-Preferred-Identity", bracket_uri);
}

static pj_status_t send_rpack(pjsip_rx_data *rdata, unsigned char ack_ref)
{
	pj_status_t status;
	char buf[7];
	char addr_buf[512];
	buf[0] = 2; /* RPACK mobile-to-network.  */
	buf[1] = ack_ref;
	buf[2] = 0x41;
	buf[3] = 0x02;
	buf[4] = 0x00;
	buf[5] = 0x00;

	struct ast_sip_endpoint *endpoint = ast_pjsip_rdata_get_endpoint(rdata);

	ast_assert(endpoint != NULL);
	pjsip_tx_data *tdata;

	struct ast_sip_transport_state *transport_state = ast_sip_get_transport_state(endpoint->transport);
	if (!transport_state) {
		ast_log(LOG_ERROR, "Failed to get transport state\n");
		return PJ_ENOMEM;
	}

	ssize_t size = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, (pjsip_name_addr *)rdata->msg_info.from->uri, addr_buf, sizeof(addr_buf) - 1);
	if (size <= 0 || size >= sizeof(addr_buf)) {
		return PJ_ENOMEM;
	}
	addr_buf[size] = '\0';

	status = ast_sip_create_request("MESSAGE", NULL, endpoint, addr_buf, NULL, &tdata);
	if (status) {
		ast_log(LOG_WARNING, "PJSIP MESSAGE - Could not create request\n");
		return status;
	}

	ao2_lock(transport_state);

	if (transport_state->service_routes) {
		int idx;

		for (idx = 0; idx < AST_VECTOR_SIZE(transport_state->service_routes); ++idx) {
			char *service_route = AST_VECTOR_GET(transport_state->service_routes, idx);

			ast_sip_add_header(tdata, "Route", service_route);
		}
	}

	ast_sip_add_header(tdata, "Security-Verify", transport_state->volte.security_server);

	if (transport_state->volte.p_access_network_info[0]) {
		ast_sip_add_header(tdata, "P-Access-Network-Info", transport_state->volte.p_access_network_info);
	}

	ao2_unlock(transport_state);

	ast_sip_add_header(tdata, "Require", "sec-agree");
	ast_sip_add_header(tdata, "Proxy-Require", "sec-agree");
	ast_sip_add_header(tdata, "Supported", "path, sec-agree");

	set_preferred_identity(tdata, transport_state->volte.p_associated_uri);
	ast_sip_update_from(tdata, transport_state->volte.p_associated_uri);
	volte_add_contact_params(tdata, PJ_TRUE, endpoint->contact_user,
				 volte_msg_contact_params);

	pjsip_cid_hdr *call_id_hdr = (pjsip_cid_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CALL_ID, NULL);
	if (call_id_hdr) {
		status = add_value_string_hdr(tdata, &STR_IN_REPLY_TO, &call_id_hdr->id);
		if (status)
			return status;
	}

	ast_sip_add_header(tdata, "Accept-Contact", "*;+g.3gpp.smsip");
	ast_sip_add_header(tdata, "Allow", "MESSAGE");
	ast_sip_add_header(tdata, "Request-Disposition", "no-fork");

	struct ast_sip_body body = {
		.type = "application",
		.subtype = "vnd.3gpp.sms",
		.body_text = buf
	};

	status = ast_sip_add_binary_body(tdata, &body, 6);
	if (status) {
		pjsip_tx_data_dec_ref(tdata);
		ast_log(LOG_ERROR, "PJSIP MESSAGE - Could not add body to request\n");
		return status;
	}

	status = ast_sip_send_request(tdata, NULL, endpoint, NULL, NULL);
	if (status) {
		ast_log(LOG_ERROR, "PJSIP MESSAGE - Could not send request\n");
		return status;
	}

	return PJ_SUCCESS;
}

static void parse_rpdata(pjsip_rx_data *rdata, struct ast_msg *msg, int *ack_ref)
{
	if (!rdata->msg_info.msg->body || !rdata->msg_info.msg->body->len) {
		ast_log(LOG_DEBUG, "No data\n");
		return;
	}
	unsigned char buf[300];
	int len = rdata->msg_info.msg->body->print_body(
		rdata->msg_info.msg->body, (char *)buf, sizeof(buf));

	if (len < 3) {
		ast_log(LOG_DEBUG, "MESSAGE RP-DATA is too short or error: %d.\n", len);
		return;
	}
			
	char buf2[MAX_BODY_SIZE * 2 + 1];
	hex_body(buf2, buf, len);
	ast_log(LOG_DEBUG, "SMS RP-DATA '%s'.\n", buf2);
	switch (buf[0])
	{
	case 0x01: {
		unsigned char *p;
		/* RP-DATA */
		*ack_ref = buf[1] & 0xff;
		int sender_len = buf[2] & 0xff;
		unsigned char *sender = &buf[3];
		p = sender + sender_len;
		ast_log(LOG_DEBUG, "Sender len %d.\n", sender_len);
			
		if (p + 3 >= buf+len)
			return;
		int dest_len = p[0] & 0xff;
		ast_log(LOG_DEBUG, "Dest len %d.\n", dest_len);
		if (dest_len != 0)
			return;

		int tpdu_len = p[1] & 0xff;
		if (tpdu_len > len - (p - buf) - 2)
			tpdu_len = len - (p - buf) - 2;
		parse_tpdu(msg, p + 2, tpdu_len);
		return;
	}
	case 0x03: /* RP-ACK */
	case 0x05: /* RP-ERROR */
	default:
		ast_log(LOG_WARNING, "Unknown RP-DATA 0x%02x. Dropping message\n", buf[0]);
		return;
	}
}


/*!
 * \internal
 * \brief Converts a 'sip:' uri to a 'pjsip:' so it can be found by
 * the message tech.
 *
 * \param buf uri to insert 'pjsip' into
 * \param size length of the uri in buf
 * \param capacity total size of buf
 */
static char *sip_to_pjsip(char *buf, int size, int capacity)
{
	int count;
	const char *scheme;
	char *res = buf;

	/* remove any wrapping brackets */
	if (*buf == '<') {
		++buf;
		--size;
	}

	scheme = strncmp(buf, "sip", 3) ? "pjsip:" : "pj";
	count = strlen(scheme);
	if (count + size >= capacity) {
		ast_log(LOG_WARNING, "Unable to handle MESSAGE- incoming uri "
			"too large for given buffer\n");
		return NULL;
	}

	memmove(res + count, buf, size);
	memcpy(res, scheme, count);

	buf += size - 1;
	if (*buf == '>') {
		*buf = '\0';
	}

	return res;
}

/*!
 * \internal
 * \brief Converts a pjsip_rx_data structure to an ast_msg structure.
 *
 * \details Attempts to fill in as much information as possible into the given
 * msg structure copied from the given request data.
 *
 * \param rdata The SIP request
 * \param msg The asterisk message structure to fill in.
 */
static enum pjsip_status_code rx_data_to_ast_msg(pjsip_rx_data *rdata, struct ast_msg *msg, pj_bool_t is_sms, int *ack_ref)
{
	RAII_VAR(struct ast_sip_endpoint *, endpt, NULL, ao2_cleanup);
	pjsip_uri *ruri = rdata->msg_info.msg->line.req.uri;
	pjsip_name_addr *name_addr;
	char buf[MAX_BODY_SIZE];
	const char *field;
	const char *context;
	char exten[AST_MAX_EXTENSION];
	int res = 0;
	int size;

	*ack_ref = -1;

	if (!ast_sip_is_allowed_uri(ruri)) {
		return PJSIP_SC_UNSUPPORTED_URI_SCHEME;
	}

	ast_copy_pj_str(exten, ast_sip_pjsip_uri_get_username(ruri), AST_MAX_EXTENSION);

	/*
	 * We may want to match in the dialplan without any user
	 * options getting in the way.
	 */
	AST_SIP_USER_OPTIONS_TRUNCATE_CHECK(exten);

	endpt = ast_pjsip_rdata_get_endpoint(rdata);
	ast_assert(endpt != NULL);

	context = S_OR(endpt->message_context, endpt->context);
	res |= ast_msg_set_context(msg, "%s", context);
	res |= ast_msg_set_exten(msg, "%s", exten);

	/* to header */
	name_addr = (pjsip_name_addr *)rdata->msg_info.to->uri;
	size = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, name_addr, buf, sizeof(buf) - 1);
	if (size <= 0) {
		return PJSIP_SC_INTERNAL_SERVER_ERROR;
	}
	buf[size] = '\0';
	res |= ast_msg_set_to(msg, "%s", sip_to_pjsip(buf, ++size, sizeof(buf) - 1));

	/* from header */
	name_addr = (pjsip_name_addr *)rdata->msg_info.from->uri;
	size = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, name_addr, buf, sizeof(buf) - 1);
	if (size <= 0) {
		return PJSIP_SC_INTERNAL_SERVER_ERROR;
	}
	buf[size] = '\0';
	res |= ast_msg_set_from(msg, "%s", buf);

	field = pj_sockaddr_print(&rdata->pkt_info.src_addr, buf, sizeof(buf) - 1, 3);
	res |= ast_msg_set_var(msg, "PJSIP_RECVADDR", field);

	switch (rdata->tp_info.transport->key.type) {
	case PJSIP_TRANSPORT_UDP:
	case PJSIP_TRANSPORT_UDP6:
		field = "udp";
		break;
	case PJSIP_TRANSPORT_TCP:
	case PJSIP_TRANSPORT_TCP6:
		field = "tcp";
		break;
	case PJSIP_TRANSPORT_TLS:
	case PJSIP_TRANSPORT_TLS6:
		field = "tls";
		break;
	default:
		field = rdata->tp_info.transport->type_name;
	}
	ast_msg_set_var(msg, "PJSIP_TRANSPORT", field);

	ast_log(LOG_DEBUG, "MESSAGE is_sms=%d.\n", is_sms);
	if (is_sms) {
		parse_rpdata(rdata, msg, ack_ref);
	} else if (print_body(rdata, buf, sizeof(buf) - 1) > 0) {
		res |= ast_msg_set_body(msg, "%s", buf);
	}

	/* endpoint name */
	res |= ast_msg_set_tech(msg, "%s", "PJSIP");
	res |= ast_msg_set_endpoint(msg, "%s", ast_sorcery_object_get_id(endpt));
	if (endpt->id.self.name.valid) {
		res |= ast_msg_set_var(msg, "PJSIP_ENDPOINT", endpt->id.self.name.str);
	}

	if (!is_sms) {
		res |= headers_to_vars(rdata, msg);
	}

	return !res ? PJSIP_SC_OK : PJSIP_SC_INTERNAL_SERVER_ERROR;
}

struct msg_data {
	struct ast_msg *msg;
	char *destination;
	char *from;
};

static void msg_data_destroy(void *obj)
{
	struct msg_data *mdata = obj;

	ast_free(mdata->from);
	ast_free(mdata->destination);

	ast_msg_destroy(mdata->msg);
}

static struct msg_data *msg_data_create(const struct ast_msg *msg, const char *destination, const char *from)
{
	char *uri_params;
	struct msg_data *mdata = ao2_alloc(sizeof(*mdata), msg_data_destroy);

	if (!mdata) {
		return NULL;
	}

	/* typecast to suppress const warning */
	mdata->msg = ast_msg_ref((struct ast_msg *) msg);

	/* To starts with 'pjsip:' which needs to be removed. */
	if (!(destination = strchr(destination, ':'))) {
		ao2_ref(mdata, -1);
		return NULL;
	}
	++destination;/* Now skip the ':' */

	mdata->destination = ast_strdup(destination);
	mdata->from = ast_strdup(from);

	/*
	 * Sometimes from URI can contain URI parameters, so remove them.
	 *
	 * sip:user;user-options@domain;uri-parameters
	 */
	uri_params = strchr(mdata->from, '@');
	if (uri_params && (uri_params = strchr(mdata->from, ';'))) {
		*uri_params = '\0';
	}
	return mdata;
}

static void update_content_type(pjsip_tx_data *tdata, struct ast_msg *msg, struct ast_sip_body *body)
{
	static const pj_str_t CONTENT_TYPE = { "Content-Type", sizeof("Content-Type") - 1 };

	const char *content_type = ast_msg_get_var(msg, pj_strbuf(&CONTENT_TYPE));
	if (content_type) {
		pj_str_t type, subtype;
		pjsip_ctype_hdr *parsed;

		/* Let pjsip do the parsing for us */
		parsed = pjsip_parse_hdr(tdata->pool, &CONTENT_TYPE,
			ast_strdupa(content_type), strlen(content_type),
			NULL);

		if (!parsed) {
			ast_log(LOG_WARNING, "Failed to parse '%s' as a content type. Using text/plain\n",
				content_type);
			return;
		}

		/* We need to turn type and subtype into zero-terminated strings */
		pj_strdup_with_null(tdata->pool, &type, &parsed->media.type);
		pj_strdup_with_null(tdata->pool, &subtype, &parsed->media.subtype);

		body->type = pj_strbuf(&type);
		body->subtype = pj_strbuf(&subtype);
	}
}

static pj_bool_t is_uri_phone(const char *uri)
{
	int i = 0;
	if (!uri || uri[0] == '\0') {
		return PJ_FALSE;
	}

	if (ast_begins_with(uri, "pjsip:")) {
		uri += 6;
	}

	if (uri[0] == '\0') {
		return PJ_FALSE;
	}

	pj_pool_t *pool = pjsip_endpt_create_pool(ast_sip_get_pjsip_endpoint(), "Phone numbers", 256, 256);
	if (!pool) {
		ast_log(LOG_ERROR, "Could not create pool\n");
		return PJ_FALSE;
	}

	pjsip_uri *pjsip_uri = pjsip_parse_uri(pool, (char *)uri, strlen(uri), 0);
	if (!pjsip_uri) {
		ast_log(LOG_ERROR, "Could not parse URI '%s'\n", uri);
		return -1;
	}
	pjsip_sip_uri *sip_uri = pjsip_uri_get_uri(pjsip_uri);

	if (!PJSIP_URI_SCHEME_IS_SIP(sip_uri) && !PJSIP_URI_SCHEME_IS_SIPS(sip_uri)) {
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
		return PJ_FALSE;
	}

	if (!pj_strlen(&sip_uri->user)) {
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
		return PJ_FALSE;
	}

	if (pj_strbuf(&sip_uri->user)[0] == '+') {
		i = 1;
	}

	/* Test URI user against allowed characters in AST_DIGIT_ANY */
	for (; i < pj_strlen(&sip_uri->user); i++) {
		if (!strchr(AST_DIGIT_ANY, pj_strbuf(&sip_uri->user)[i])) {
			break;
		}
	}

	if (i < pj_strlen(&sip_uri->user)) {
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
		return PJ_FALSE;
	}

	pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
	return PJ_TRUE;
}

static int volte_send_rp_data(struct msg_data *mdata, const char *orig_uri, struct ast_sip_endpoint *endpoint, const unsigned char *buf, size_t buflen)
{
	pjsip_tx_data *tdata;
	struct ast_sip_transport *transport;
	int registered = 0;

	transport = ast_sorcery_retrieve_by_id(ast_sip_get_sorcery(), "transport",
					       endpoint->transport);
	if (transport) {
		struct ast_sip_transport_state *trans_state;

		trans_state = ast_sip_get_transport_state(ast_sorcery_object_get_id(transport));
		if (trans_state) {
			/* No locking, this is atomic. */
			registered = trans_state->volte.registered;
		}
		ao2_cleanup(trans_state);
	}
	ao2_cleanup(transport);

	if (!registered) {
		ast_log(LOG_NOTICE, "No VoLTE transport or no registered endpoint '%s'\n", ast_sorcery_object_get_id(endpoint));
		return -1;
	}

	if (ast_sip_create_request("MESSAGE", NULL, endpoint, endpoint->smsc_uri, NULL, &tdata)) {
		ast_log(LOG_WARNING, "PJSIP MESSAGE - Could not create request\n");
		return -1;
	}

	struct ast_sip_transport_state *transport_state = ast_sip_get_transport_state(endpoint->transport);
	if (!transport_state) {
		ast_log(LOG_ERROR, "Failed to get transport state\n");
		return -1;
	}

	ao2_lock(transport_state);

	if (transport_state->service_routes) {
		int idx;

		for (idx = 0; idx < AST_VECTOR_SIZE(transport_state->service_routes); ++idx) {
			char *service_route = AST_VECTOR_GET(transport_state->service_routes, idx);

			ast_sip_add_header(tdata, "Route", service_route);
		}
	}

	ast_sip_add_header(tdata, "Security-Verify", transport_state->volte.security_server);

	if (transport_state->volte.p_access_network_info[0]) {
		ast_sip_add_header(tdata, "P-Access-Network-Info", transport_state->volte.p_access_network_info);
	}

	ao2_unlock(transport_state);

	ast_sip_add_header(tdata, "Require", "sec-agree");
	ast_sip_add_header(tdata, "Proxy-Require", "sec-agree");
	ast_sip_add_header(tdata, "Supported", "path, sec-agree");

	set_preferred_identity(tdata, transport_state->volte.p_associated_uri);
	ast_sip_update_from(tdata, transport_state->volte.p_associated_uri);
	volte_add_contact_params(tdata, PJ_TRUE, endpoint->contact_user,
				 volte_msg_contact_params);

	ast_sip_add_header(tdata, "Allow", "MESSAGE");
	ast_sip_add_header(tdata, "Request-Disposition", "no-fork");
	ast_sip_add_header(tdata, "Accept-Contact", "*;+g.3gpp.smsip");

	struct ast_sip_body body = {
		.type = "application",
		.subtype = "vnd.3gpp.sms",
		.body_text = (const char*)buf
	};
	pj_status_t status;

	status = ast_sip_add_binary_body(tdata, &body, buflen);
	if (status)
	{
		return -1;
	}

	status = ast_sip_send_request(tdata, NULL, endpoint, NULL, NULL);
	if (status) {
		ast_log(LOG_ERROR, "PJSIP MESSAGE - Could not send request\n");
		return -1;
	}

	return 0;	
}

static pj_bool_t is_7bit_compatible(const unsigned short *in, size_t inlen)
{
	for (const unsigned short *p = in; p < in + inlen; p++)
	{
		int v;
		for (v = 0; v < 128 && defaultalphabet[v] != *p; v++);
		if (v < 128)
			continue;
		for (v = 0; v < 128 && escapes[v] != *p; v++);
		if (v == 128)
			return PJ_FALSE; /* TODO: Use escapes but it complicates splitting */
	}
	return PJ_TRUE;
}

static pj_bool_t is_7bit_escape(unsigned short u)
{
	int v;
	for (v = 0; v < 128 && defaultalphabet[v] != u; v++);
	return v == 128;
}

static pj_bool_t is_8bit_compatible(const unsigned short *in, size_t inlen)
{
	for (const unsigned short *p = in; p < in + inlen; p++)
	{
		if (*p > 255)
			return PJ_FALSE;
	}
	return PJ_TRUE;
}

static size_t escaped_len(const unsigned short *in, size_t inlen)
{
	size_t ret = 0;
	for (const unsigned short *p = in; p < in + inlen; p++)
	{
		ret++;
		if (is_7bit_escape(*p))
			ret++;
	}

	return ret;
}

static unsigned short *next_segment(unsigned char dcs, unsigned short *in, size_t inlen)
{
	int remaining = 153;

	if (is7bit(dcs))
	{
		remaining = 153;
	} else if (is8bit(dcs))
	{
		remaining = 134;
	} else
	{
		remaining = 67;
	}

	for (unsigned short *p = in; p < in + inlen; p++)
	{
		size_t curlen = is7bit(dcs) && is_7bit_escape(*p) ? 2 : 1;
		if (remaining < curlen)
			return p;
		remaining -= curlen;
	}

	return in + inlen;
}

static int volte_send_sms(struct ast_sip_endpoint *endpoint, const char *orig_uri, struct msg_data *mdata)
{
	unsigned char buf[1024];
	unsigned short utf16[2048];
	unsigned char *p, *len_byte;
	static uint8_t ref = 1;  // TODO: start at random
	static uint8_t mr = 0;

	const unsigned char *utf8 = (const unsigned char *) ast_msg_get_body(mdata->msg);
	unsigned short *utf16p = utf16;
	while (*utf8 && utf16p - utf16 < sizeof(utf16) / sizeof(utf16[0]) - 2)
	{
		long l = utf8decode((unsigned char **)&utf8);
		if (l < 0x10000)
			*utf16p++ = l;
		else {
			*utf16p++ = 0xD800 | (l & 0x3ff);
			*utf16p++ = 0xDC00 | ((l >> 10) & 0x3ff);
		}
	}

	unsigned char dcs = 0;
	int maxlen = 160;
	size_t utf16len = utf16p - utf16;

	if (is_7bit_compatible(utf16, utf16len))
	{
		dcs = 0x00;
		maxlen = 160;
	} else if (is_8bit_compatible(utf16, utf16len))
	{
		dcs = 0x04;
		maxlen = 140;
	} else
	{
		dcs = 0x08;
		maxlen = 70;
	}

	const char *msg_to = ast_msg_get_to(mdata->msg);

	const char *phone_uri = (msg_to && msg_to[0] ? msg_to : orig_uri);

	if (ast_begins_with(phone_uri, "pjsip:")) {
		phone_uri += 6;
	}

	char phone[50], smsc_phone[50];
	{
		pj_pool_t *pool = pjsip_endpt_create_pool(ast_sip_get_pjsip_endpoint(), "Phone numbers", 256, 256);
		if (!pool) {
			ast_log(LOG_ERROR, "Could not create pool\n");
			return -1;
		}
		pjsip_uri *phone_uri_parsed = pjsip_parse_uri(pool, (char *) phone_uri, strlen(phone_uri), 0);
		if (!phone_uri_parsed) {
			ast_log(LOG_ERROR, "Could not parse URI '%s'\n", phone_uri);
			return -1;
		}
		pjsip_sip_uri *phone_sip_uri = pjsip_uri_get_uri(phone_uri_parsed);
		size_t phonelen = phone_sip_uri->user.slen;
		if (phonelen > sizeof(phone) - 1)
			phonelen = sizeof(phone) - 1;
		strncpy(phone, phone_sip_uri->user.ptr, phonelen);
		phone[phonelen] = '\0';

		pjsip_uri *smsc_phone_uri_parsed = pjsip_parse_uri(pool, (char *) endpoint->smsc_uri, strlen(endpoint->smsc_uri), 0);
		if (!smsc_phone_uri_parsed) {
			ast_log(LOG_ERROR, "Could not parse URI '%s'\n", endpoint->smsc_uri);
			return -1;
		}
		pjsip_sip_uri *smsc_phone_sip_uri = pjsip_uri_get_uri(smsc_phone_uri_parsed);
		size_t smsc_phonelen = smsc_phone_sip_uri->user.slen;
		if (smsc_phonelen > sizeof(smsc_phone) - 1)
			smsc_phonelen = sizeof(smsc_phone) - 1;
		strncpy(smsc_phone, smsc_phone_sip_uri->user.ptr, smsc_phonelen);
		smsc_phone[smsc_phonelen] = '\0';
		pjsip_endpt_release_pool(ast_sip_get_pjsip_endpoint(), pool);
	}

	size_t len = is7bit(dcs) ? escaped_len(utf16, utf16p - utf16) : (utf16p - utf16);
	int parts = 0;

	if (len <= maxlen) {
		parts = 1;
	} else {
		for (unsigned short *p = utf16; p < utf16p; p = next_segment(dcs, p, utf16p - p), parts++);
	}

	int overall_status = 0;
	unsigned short *start_segment = utf16;
	static unsigned short concat_refnum_pool = 0; // TODO: start at random
	unsigned short concat_refnum = concat_refnum_pool++;

	for (int part = 0; part < parts; part++)
	{
		unsigned short *end_segment = parts == 1 ? utf16p : next_segment(dcs, start_segment, utf16p - start_segment);
		buf[0] = 0x00; /* RP-DATA MS to network */
		buf[1] = ref++; // Reference
		buf[2] = 0x00; // No originator address
		p = buf + 3;
		p += packaddress(p, smsc_phone); // Destination Address: SMSC
		// RP uses different length encoding: number of bytes including type byte rather than nibbles excluding type byte. Correct for it
		buf[3] = (buf[3] + 3) / 2;
		len_byte = p;
		p++;

		*p++ = 0x11 | ((parts > 1) ? 0x40 : 0);                      /* SMS_DATA */
		*p++ = mr++;  /* TP-MR */
		p += packaddress(p, phone);
		*p++ = 0x00;                      /* TP-PID */
		*p++ = dcs;
		*p++ = 0xff;                      /* Validity period  */
		if (parts == 1) {
			p += packsms(dcs, p, 0, NULL, utf16len, utf16);
		} else {
			unsigned char udh[5];
			udh[0] = 0x00; // Concatenated message
			udh[1] = 0x03; // Length of IE
			udh[2] = concat_refnum;
			udh[3] = parts;
			udh[4] = part + 1;
			p += packsms(dcs, p, sizeof(udh), udh, end_segment - start_segment, start_segment);
		}
		*len_byte = p - len_byte - 1;

		/* TODO: resend if no RP-ACK is received */
		int status = volte_send_rp_data(mdata, orig_uri, endpoint, buf, p - buf);
		if (status < 0)
			overall_status = status;

		start_segment = end_segment;
	}

	return overall_status;
}

/*!
 * \internal
 * \brief Send a MESSAGE
 *
 * \param data The outbound message data structure
 *
 * \return 0: success, -1: failure
 *
 * mdata contains the To and From specified in the call to the MessageSend
 * dialplan app.  It also contains the ast_msg object that contains the
 * message body and may contain the To and From from the channel datastore,
 * usually set with the MESSAGE or MESSAGE_DATA dialplan functions but
 * could also come from an incoming sip MESSAGE.
 *
 * The mdata->to is always used as the basis for the Request URI
 * while the mdata->msg->to is used for the To header.  If
 * mdata->msg->to isn't available, mdata->to is used for the To header.
 *
 */
static int msg_send(void *data)
{
	struct msg_data *mdata = data; /* The caller holds a reference */

	struct ast_sip_body body = {
		.type = "text",
		.subtype = "plain",
		.body_text = ast_msg_get_body(mdata->msg)
	};

	pjsip_tx_data *tdata;
	RAII_VAR(char *, uri, NULL, ast_free);
	RAII_VAR(struct ast_sip_endpoint *, endpoint, NULL, ao2_cleanup);

	ast_debug(3, "mdata From: %s msg From: %s mdata Destination: %s msg To: %s\n",
		mdata->from, ast_msg_get_from(mdata->msg), mdata->destination, ast_msg_get_to(mdata->msg));

	endpoint = ast_sip_get_endpoint(mdata->destination, 1, &uri);
	if (!endpoint) {
		ast_log(LOG_ERROR,
			"PJSIP MESSAGE - Could not find endpoint '%s' and no default outbound endpoint configured\n",
			mdata->destination);

		ast_test_suite_event_notify("MSG_ENDPOINT_URI_FAIL",
			"MdataFrom: %s\r\n"
			"MsgFrom: %s\r\n"
			"MdataDestination: %s\r\n"
			"MsgTo: %s\r\n",
			mdata->from,
			ast_msg_get_from(mdata->msg),
			mdata->destination,
			ast_msg_get_to(mdata->msg));

		return -1;
	}

	ast_debug(3, "Request URI: %s\n", uri);

	const char *msg_to_orig = ast_msg_get_to(mdata->msg);

	if (endpoint->volte &&
	    is_uri_phone(msg_to_orig && msg_to_orig[0] ? msg_to_orig : uri)) {
		if (!endpoint->smsc_uri || endpoint->smsc_uri[0] == 0) {
			ast_log(LOG_ERROR,
				"PJSIP MESSAGE - Attempt to send SMS on VoLTE without SMS URI set\n");
			return -1;
		}
		return volte_send_sms(endpoint, uri, mdata);
	}

	if (ast_sip_create_request("MESSAGE", NULL, endpoint, uri, NULL, &tdata)) {
		ast_log(LOG_WARNING, "PJSIP MESSAGE - Could not create request\n");
		return -1;
	}

	/* If there was a To in the actual message, */
	if (!ast_strlen_zero(msg_to_orig)) {
		char *msg_to = ast_strdupa(msg_to_orig);

		/*
		 * It's possible that the message To was copied from
		 * an incoming MESSAGE in which case it'll have the
		 * pjsip: tech prepended to it.  We need to remove it.
		 */
		if (ast_begins_with(msg_to, "pjsip:")) {
			msg_to += 6;
		}
		ast_sip_update_to_uri(tdata, msg_to);
	} else {
		/*
		 * If there was no To in the message, it's still possible
		 * that there is a display name in the mdata To.  If so,
		 * we'll copy the URI display name to the tdata To.
		 */
		update_to_display_name(tdata, uri);
	}

	if (!ast_strlen_zero(mdata->from)) {
		ast_sip_update_from(tdata, mdata->from);
	} else if (!ast_strlen_zero(ast_msg_get_from(mdata->msg))) {
		ast_sip_update_from(tdata, (char *)ast_msg_get_from(mdata->msg));
	}

#ifdef TEST_FRAMEWORK
	{
		pjsip_name_addr *tdata_name_addr;
		pjsip_sip_uri *tdata_sip_uri;
		char touri[128];
		char fromuri[128];

		tdata_name_addr = (pjsip_name_addr *) PJSIP_MSG_TO_HDR(tdata->msg)->uri;
		tdata_sip_uri = pjsip_uri_get_uri(tdata_name_addr->uri);
		pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, tdata_sip_uri, touri, sizeof(touri));
		tdata_name_addr = (pjsip_name_addr *) PJSIP_MSG_FROM_HDR(tdata->msg)->uri;
		tdata_sip_uri = pjsip_uri_get_uri(tdata_name_addr->uri);
		pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, tdata_sip_uri, fromuri, sizeof(fromuri));

		ast_test_suite_event_notify("MSG_FROMTO_URI",
			"MdataFrom: %s\r\n"
			"MsgFrom: %s\r\n"
			"MdataDestination: %s\r\n"
			"MsgTo: %s\r\n"
			"Endpoint: %s\r\n"
			"RequestURI: %s\r\n"
			"ToURI: %s\r\n"
			"FromURI: %s\r\n",
			mdata->from,
			ast_msg_get_from(mdata->msg),
			mdata->destination,
			ast_msg_get_to(mdata->msg),
			ast_sorcery_object_get_id(endpoint),
			uri,
			touri,
			fromuri
			);
	}
#endif

	update_content_type(tdata, mdata->msg, &body);

	if (ast_sip_add_body(tdata, &body)) {
		pjsip_tx_data_dec_ref(tdata);
		ast_log(LOG_ERROR, "PJSIP MESSAGE - Could not add body to request\n");
		return -1;
	}

	/*
	 * This copies any headers set with MESSAGE_DATA() to the
	 * tdata.
	 */
	vars_to_headers(mdata->msg, tdata);

	ast_debug(1, "Sending message to '%s' (via endpoint %s) from '%s'\n",
		uri, ast_sorcery_object_get_id(endpoint), mdata->from);

	if (ast_sip_send_request(tdata, NULL, endpoint, NULL, NULL)) {
		ast_log(LOG_ERROR, "PJSIP MESSAGE - Could not send request\n");
		return -1;
	}

	return 0;
}

static int sip_msg_send(const struct ast_msg *msg, const char *destination, const char *from)
{
	struct msg_data *mdata;
	int res;

	if (ast_strlen_zero(destination)) {
		ast_log(LOG_ERROR, "SIP MESSAGE - a 'To' URI  must be specified\n");
		return -1;
	}

	mdata = msg_data_create(msg, destination, from);
	if (!mdata) {
		return -1;
	}

	res = ast_sip_push_task_wait_serializer(message_serializer, msg_send, mdata);
	ao2_ref(mdata, -1);

	return res;
}

static const struct ast_msg_tech msg_tech = {
	.name = "pjsip",
	.msg_send = sip_msg_send,
};

static pj_status_t send_response(pjsip_rx_data *rdata, enum pjsip_status_code code,
				 pjsip_dialog *dlg, pjsip_transaction *tsx)
{
	pjsip_tx_data *tdata;
	pj_status_t status;

	status = ast_sip_create_response(rdata, code, NULL, &tdata);
	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Unable to create response (%d)\n", status);
		return status;
	}

	if (dlg && tsx) {
		status = pjsip_dlg_send_response(dlg, tsx, tdata);
	} else {
		struct ast_sip_endpoint *endpoint;

		endpoint = ast_pjsip_rdata_get_endpoint(rdata);
		status = ast_sip_send_stateful_response(rdata, tdata, endpoint);
		ao2_cleanup(endpoint);
	}

	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Unable to send response (%d)\n", status);
	}

	return status;
}

static pj_bool_t module_on_rx_request(pjsip_rx_data *rdata)
{
	enum pjsip_status_code code;
	struct ast_msg *msg;
	pj_bool_t is_sms;
	int ack_ref = -1;

	/* if not a MESSAGE, don't handle */
	if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, &pjsip_message_method)) {
		return PJ_FALSE;
	}

	code = check_content_type(rdata, &is_sms);
	if (code != PJSIP_SC_OK) {
		send_response(rdata, code, NULL, NULL);
		return PJ_TRUE;
	}

	msg = ast_msg_alloc();
	if (!msg) {
		send_response(rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL);
		return PJ_TRUE;
	}

	code = rx_data_to_ast_msg(rdata, msg, is_sms, &ack_ref);
	if (code != PJSIP_SC_OK) {
		send_response(rdata, code, NULL, NULL);
		ast_msg_destroy(msg);
		return PJ_TRUE;
	}

	if (!ast_msg_has_destination(msg)) {
		ast_debug(1, "MESSAGE request received, but no handler wanted it\n");
		send_response(rdata, PJSIP_SC_NOT_FOUND, NULL, NULL);
		ast_msg_destroy(msg);
		return PJ_TRUE;
	}

	/* Send it to the messaging core.
	 *
	 * If we are unable to send a response, the most likely reason is that we
	 * are handling a retransmission of an incoming MESSAGE and were unable to
	 * create a transaction due to a duplicate key. If we are unable to send
	 * a response, we should not queue the message to the dialplan
	 */
	if (!send_response(rdata, is_sms ? PJSIP_SC_OK : PJSIP_SC_ACCEPTED, NULL, NULL)) {
		ast_msg_queue(msg);
	}

	if (is_sms && ack_ref >= 0)
	{
		send_rpack(rdata, ack_ref);
	}

	return PJ_TRUE;
}

static int incoming_in_dialog_request(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	enum pjsip_status_code code;
	int rc;
	pjsip_dialog *dlg = session->inv_session->dlg;
	pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
	struct ast_msg_data *msg;
	struct ast_party_caller *caller;
	pjsip_name_addr *name_addr;
	size_t from_len;
	size_t to_len;
	struct ast_msg_data_attribute attrs[4];
	int pos = 0;
	int body_pos;

	if (!session->channel) {
		send_response(rdata, PJSIP_SC_NOT_FOUND, dlg, tsx);
		return 0;
	}

	code = check_content_type_in_dialog(rdata);
	if (code != PJSIP_SC_OK) {
		send_response(rdata, code, dlg, tsx);
		return 0;
	}

	caller = ast_channel_caller(session->channel);

	name_addr = (pjsip_name_addr *) rdata->msg_info.from->uri;
	from_len = pj_strlen(&name_addr->display);
	if (from_len) {
		attrs[pos].type = AST_MSG_DATA_ATTR_FROM;
		from_len++;
		attrs[pos].value = ast_alloca(from_len);
		ast_copy_pj_str(attrs[pos].value, &name_addr->display, from_len);
		pos++;
	} else if (caller->id.name.valid && !ast_strlen_zero(caller->id.name.str)) {
		attrs[pos].type = AST_MSG_DATA_ATTR_FROM;
		attrs[pos].value = caller->id.name.str;
		pos++;
	}

	name_addr = (pjsip_name_addr *) rdata->msg_info.to->uri;
	to_len = pj_strlen(&name_addr->display);
	if (to_len) {
		attrs[pos].type = AST_MSG_DATA_ATTR_TO;
		to_len++;
		attrs[pos].value = ast_alloca(to_len);
		ast_copy_pj_str(attrs[pos].value, &name_addr->display, to_len);
		pos++;
	}

	attrs[pos].type = AST_MSG_DATA_ATTR_CONTENT_TYPE;
	attrs[pos].value = ast_alloca(rdata->msg_info.msg->body->content_type.type.slen
		+ rdata->msg_info.msg->body->content_type.subtype.slen + 2);
	sprintf(attrs[pos].value, "%.*s/%.*s",
		(int)rdata->msg_info.msg->body->content_type.type.slen,
		rdata->msg_info.msg->body->content_type.type.ptr,
		(int)rdata->msg_info.msg->body->content_type.subtype.slen,
		rdata->msg_info.msg->body->content_type.subtype.ptr);
	pos++;

	body_pos = pos;
	attrs[pos].type = AST_MSG_DATA_ATTR_BODY;
	attrs[pos].value = ast_malloc(rdata->msg_info.msg->body->len + 1);
	if (!attrs[pos].value) {
		send_response(rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, dlg, tsx);
		return 0;
	}
	ast_copy_string(attrs[pos].value, rdata->msg_info.msg->body->data, rdata->msg_info.msg->body->len + 1);
	pos++;

	msg = ast_msg_data_alloc(AST_MSG_DATA_SOURCE_TYPE_IN_DIALOG, attrs, pos);
	if (!msg) {
		ast_free(attrs[body_pos].value);
		send_response(rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, dlg, tsx);
		return 0;
	}

	ast_debug(1, "Received in-dialog MESSAGE from '%s:%s': %s %s\n",
		ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_FROM),
		ast_channel_name(session->channel),
		ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_TO),
		ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_BODY));

	rc = ast_msg_data_queue_frame(session->channel, msg);
	ast_free(attrs[body_pos].value);
	ast_free(msg);
	if (rc != 0) {
		send_response(rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, dlg, tsx);
	} else {
		send_response(rdata, PJSIP_SC_ACCEPTED, dlg, tsx);
	}

	return 0;
}

static struct ast_sip_session_supplement messaging_supplement = {
	.method = "MESSAGE",
	.incoming_request = incoming_in_dialog_request
};

static pjsip_module messaging_module = {
	.name = {"Messaging Module", 16},
	.id = -1,
	.priority = PJSIP_MOD_PRIORITY_APPLICATION,
	.on_rx_request = module_on_rx_request,
};

static int load_module(void)
{
	if (ast_sip_register_service(&messaging_module) != PJ_SUCCESS) {
		return AST_MODULE_LOAD_DECLINE;
	}

	if (pjsip_endpt_add_capability(ast_sip_get_pjsip_endpoint(),
				       NULL, PJSIP_H_ALLOW, NULL, 1,
				       &pjsip_message_method.name) != PJ_SUCCESS) {

		ast_sip_unregister_service(&messaging_module);
		return AST_MODULE_LOAD_DECLINE;
	}

	if (ast_msg_tech_register(&msg_tech)) {
		ast_sip_unregister_service(&messaging_module);
		return AST_MODULE_LOAD_DECLINE;
	}

	message_serializer = ast_sip_create_serializer("pjsip/messaging");
	if (!message_serializer) {
		ast_sip_unregister_service(&messaging_module);
		ast_msg_tech_unregister(&msg_tech);
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_sip_session_register_supplement(&messaging_supplement);
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_sip_session_unregister_supplement(&messaging_supplement);
	ast_msg_tech_unregister(&msg_tech);
	ast_sip_unregister_service(&messaging_module);
	ast_taskprocessor_unreference(message_serializer);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP Messaging Support",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_APP_DEPEND,
	.requires = "res_pjsip,res_pjsip_session",
);
