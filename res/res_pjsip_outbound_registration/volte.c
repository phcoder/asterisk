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
#include "milenage.h"

#define fmt_str(str) (int)(str).slen, (str).ptr
#define fmt_strp(strp) (int)(strp)->slen, (strp)->ptr

/* Socket for transform configuration */
static struct mnl_socket *g_mnl_socket = NULL;

/* Supported authentication and encryption algorithms. */
struct ipsec_alg {
	const char *sip_name;
	const char *kernel_name;
};

const struct ipsec_alg g_ipsec_alg[] = {
	{ "hmac-md5-96", "md5" },
	{ "hmac-sha-1-96", "sha1" },
	{ NULL, NULL }
};

const struct ipsec_alg g_ipsec_ealg[] = {
	{ "null", "cipher_null" },
	{ NULL, NULL }
};

/* Global init function. Must be called before the first registration is made. May be called again. */
pj_status_t g_volte_init(void)
{
	if (!g_mnl_socket)
		g_mnl_socket = xfrm_init_mnl_socket();

	return PJ_SUCCESS;
}

/* Global exit function. Must be called when module is unloaded. */
void g_volte_exit(void)
{
	if (g_mnl_socket)
		xfrm_exit_mnl_socket(g_mnl_socket);
}

/* Convert from pj_sockaddr to sockaddr_storage. */
static void copy_pj_sockaddr_to_sockaddr_storage(const pj_sockaddr *src, struct sockaddr_storage *dst)
{
	memset(dst, 0, sizeof(struct sockaddr_storage));

	dst->ss_family = (src->addr.sa_family == PJ_AF_INET) ? AF_INET : AF_INET6;

	if (dst->ss_family == AF_INET) {
		struct sockaddr_in *dst_in = (struct sockaddr_in *)dst;
		const pj_sockaddr_in *src_in = (const pj_sockaddr_in *)src;
		dst_in->sin_port = src_in->sin_port;
		memcpy(&dst_in->sin_addr, &src_in->sin_addr, sizeof(struct in_addr));
	} else if (dst->ss_family == AF_INET6) {
		struct sockaddr_in6 *dst_in6 = (struct sockaddr_in6 *)dst;
		const pj_sockaddr_in6 *src_in6 = (const pj_sockaddr_in6 *)src;
		dst_in6->sin6_port = src_in6->sin6_port;
		memcpy(&dst_in6->sin6_addr, &src_in6->sin6_addr, sizeof(struct in6_addr));
	}
}

/* Convert sockaddr_storage IP to string. */
static char *sockaddr_storage_to_string(const struct sockaddr_storage *addr, char *ip_string, size_t ip_string_len)
{
	if (addr->ss_family == AF_INET) {
		const struct sockaddr_in *addr_in = (const struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &addr_in->sin_addr, ip_string, ip_string_len);
	} else if (addr->ss_family == AF_INET6) {
		const struct sockaddr_in6 *addr_in6 = (const struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_string, ip_string_len);
	} else {
		snprintf(ip_string, ip_string_len, "<Unknown AF>");
	}

	return ip_string;
}

/* Delete old SA and SP entries upon new registration or module exit. */
void volte_cleanup_xfrm(struct ast_sip_transport_state *transport_state)
{
	struct sockaddr_storage local_addr_c, local_addr_s;
	struct sockaddr_storage remote_addr_c, remote_addr_s;

	/* Convert from pj_sockaddr to sockaddr_storage. */
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_c, &local_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_c, &remote_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_s, &local_addr_s);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_s, &remote_addr_s);

	if (transport_state->volte.local_sa_c_set || transport_state->volte.local_sa_s_set ||
	    transport_state->volte.remote_sa_c_set || transport_state->volte.remote_sa_s_set ||
	    transport_state->volte.local_sp_c_set || transport_state->volte.local_sp_s_set ||
	    transport_state->volte.remote_sp_c_set || transport_state->volte.remote_sp_s_set)
		ast_debug(1, "Remove old security associations/policies\n");

	/* Remove current security associations and policies. */
	if (transport_state->volte.local_sa_c_set) {
		xfrm_sa_del(g_mnl_socket, (const struct sockaddr *)&local_addr_c,
			    (const struct sockaddr *)&remote_addr_s, transport_state->volte.remote_spi_s);
		transport_state->volte.local_sa_c_set = PJ_FALSE;
	}
	if (transport_state->volte.local_sa_s_set) {
		xfrm_sa_del(g_mnl_socket, (const struct sockaddr *)&local_addr_s,
			    (const struct sockaddr *)&remote_addr_c, transport_state->volte.remote_spi_c);
		transport_state->volte.local_sa_s_set = PJ_FALSE;
	}
	if (transport_state->volte.remote_sa_c_set) {
		xfrm_sa_del(g_mnl_socket, (const struct sockaddr *)&remote_addr_c,
			    (const struct sockaddr *)&local_addr_s, transport_state->volte.local_spi_s);
		transport_state->volte.remote_sa_c_set = PJ_FALSE;
	}
	if (transport_state->volte.remote_sa_s_set) {
		xfrm_sa_del(g_mnl_socket, (const struct sockaddr *)&remote_addr_s,
			    (const struct sockaddr *)&local_addr_c, transport_state->volte.local_spi_c);
		transport_state->volte.remote_sa_s_set = PJ_FALSE;
	}
	if (transport_state->volte.local_sp_c_set) {
		xfrm_policy_del(g_mnl_socket, (const struct sockaddr *)&local_addr_c,
			    (const struct sockaddr *)&remote_addr_s, false);
		transport_state->volte.local_sp_c_set = PJ_FALSE;
	}
	if (transport_state->volte.local_sp_s_set) {
		xfrm_policy_del(g_mnl_socket, (const struct sockaddr *)&local_addr_s,
			    (const struct sockaddr *)&remote_addr_c, false);
		transport_state->volte.local_sp_s_set = PJ_FALSE;
	}
	if (transport_state->volte.remote_sp_c_set) {
		xfrm_policy_del(g_mnl_socket, (const struct sockaddr *)&remote_addr_c,
			    (const struct sockaddr *)&local_addr_s, true);
		transport_state->volte.remote_sp_c_set = PJ_FALSE;
	}
	if (transport_state->volte.remote_sp_s_set) {
		xfrm_policy_del(g_mnl_socket, (const struct sockaddr *)&remote_addr_s,
			    (const struct sockaddr *)&local_addr_c, true);
		transport_state->volte.remote_sp_s_set = PJ_FALSE;
	}

}

pj_status_t volte_alloc_spi(struct ast_sip_transport_state *transport_state)
{
	struct sockaddr_storage local_addr_c, local_addr_s;
	struct sockaddr_storage remote_addr_c, remote_addr_s;
//	char src_str[64], dst_str[64];
	pj_status_t status;

	/* Convert from pj_sockaddr to sockaddr_storage. */
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_c, &local_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_c, &remote_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_s, &local_addr_s);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_s, &remote_addr_s);

#if 0
	ast_debug(1, "SPI allocation: local client: %s:%d remote server: %s:%d\n",
		  sockaddr_storage_to_string(&local_addr_c, src_str, sizeof(src_str)),
		  pj_sockaddr_get_port(&transport_state->volte.local_addr_c),
		  sockaddr_storage_to_string(&remote_addr_s, dst_str, sizeof(dst_str)),
		  pj_sockaddr_get_port(&transport_state->volte.remote_addr_s));
	ast_debug(1, "SPI allocation: remote client: %s:%d local server: %s:%d\n",
		  sockaddr_storage_to_string(&remote_addr_c, src_str, sizeof(src_str)),
		  pj_sockaddr_get_port(&transport_state->volte.remote_addr_s),
		  sockaddr_storage_to_string(&local_addr_c, dst_str, sizeof(dst_str)),
		  pj_sockaddr_get_port(&transport_state->volte.local_addr_s));
#endif

	/* Allocate SPI-C and SPI-S towards remote peer. */
	status = xfrm_spi_alloc(g_mnl_socket, 2342, &transport_state->volte.local_spi_c,
				(const struct sockaddr *)&local_addr_c, (const struct sockaddr *)&remote_addr_c);
	if (status) {
spi_alloc_failed:
		ast_log(LOG_ERROR, "Failed to allocate SPI. "
			"Please make sure that the user running asterisk has the rights to do so. "
			"(E.g use \"setcap 'cap_net_admin,cap_sys_resource=ep' /usr/sbin/asterisk\")\n");
		return status;
	}
	status = xfrm_spi_alloc(g_mnl_socket, 2342, &transport_state->volte.local_spi_s,
				(const struct sockaddr *)&local_addr_s, (const struct sockaddr *)&remote_addr_s);
	if (status)
		goto spi_alloc_failed;
	ast_debug(1, "SPI allocation: SPI-C=0x%08x SPI-S=0x%08x\n", transport_state->volte.local_spi_s,
		  transport_state->volte.local_spi_c);

	return PJ_SUCCESS;
}

/* Set new SA and SP entries upon secuirty handshake. */
static pj_status_t volte_set_xfrm(struct ast_sip_transport_state *transport_state, const pj_str_t *alg, const pj_str_t *ealg, uint8_t *ik)
{
	struct xfrm_algobuf auth_algo, ciph_algo;
	int i, j;
	struct sockaddr_storage local_addr_c, local_addr_s;
	struct sockaddr_storage remote_addr_c, remote_addr_s;
	char src_str[64], dst_str[64];
	pj_status_t sa_add_failed = PJ_SUCCESS, sp_add_failed = PJ_SUCCESS;
	pj_status_t status;

	/* Set authentication and encryption algorithms and key. */
	for (i = 0; g_ipsec_alg[i].sip_name; i++) {
		if (!pj_strncmp2(alg, g_ipsec_alg[i].sip_name, strlen(g_ipsec_alg[i].sip_name)))
			break;
	}
	if (!g_ipsec_alg[i].kernel_name) {
		ast_log(LOG_ERROR, "Given 'alg' not supported.\n");
		return -EINVAL;
	}
	for (j = 0; g_ipsec_ealg[j].sip_name; j++) {
		if (!pj_strncmp2(ealg, g_ipsec_ealg[j].sip_name, strlen(g_ipsec_ealg[j].sip_name)))
			break;
	}
	if (!g_ipsec_ealg[j].kernel_name) {
		ast_log(LOG_ERROR, "Given 'ealg' not supported.\n");
		return -EINVAL;
	}
	ast_assert(sizeof(auth_algo.buf) >= 160);
	memset(&auth_algo, 0, sizeof(auth_algo));
	strcpy(auth_algo.algo.alg_name, g_ipsec_alg[i].kernel_name);
	switch (i) {
	case 0:
		memcpy(auth_algo.algo.alg_key, ik, 16);
		auth_algo.algo.alg_key_len = 128;
		break;
	case 1:
		memcpy(auth_algo.algo.alg_key, ik, 16);
		memset(auth_algo.algo.alg_key + 16, 0x00, 4);
		auth_algo.algo.alg_key_len = 160;
		break;
	}
	ast_debug(1, "xfrm: auth key: 0x%02x%02x%02x%02x\n",
		  (uint8_t)auth_algo.algo.alg_key[0], (uint8_t)auth_algo.algo.alg_key[1],
		  (uint8_t)auth_algo.algo.alg_key[2], (uint8_t)auth_algo.algo.alg_key[3]);
	memset(&ciph_algo, 0, sizeof(ciph_algo));
	strcpy(ciph_algo.algo.alg_name, g_ipsec_ealg[j].kernel_name);
	switch (j) {
	case 0:
		ciph_algo.algo.alg_key_len = 0;
		break;
	}

	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_c, &local_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_c, &remote_addr_c);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.local_addr_s, &local_addr_s);
	copy_pj_sockaddr_to_sockaddr_storage(&transport_state->volte.remote_addr_s, &remote_addr_s);

	ast_debug(1, "xfrm: local client: %s:%d (SPI=0x%08x) remote server: %s:%d (SPI=0x%08x)\n",
		  sockaddr_storage_to_string(&local_addr_c, src_str, sizeof(src_str)),
		  pj_sockaddr_get_port(&transport_state->volte.local_addr_c), transport_state->volte.local_spi_c,
		  sockaddr_storage_to_string(&remote_addr_s, dst_str, sizeof(dst_str)),
		  pj_sockaddr_get_port(&transport_state->volte.remote_addr_s), transport_state->volte.remote_spi_s);
	ast_debug(1, "xfrm: remote client: %s:%d (SPI=0x%08x) local server: %s:%d (SPI=0x%08x)\n",
		  sockaddr_storage_to_string(&remote_addr_c, src_str, sizeof(src_str)),
		  pj_sockaddr_get_port(&transport_state->volte.remote_addr_c), transport_state->volte.remote_spi_c,
		  sockaddr_storage_to_string(&local_addr_s, dst_str, sizeof(dst_str)),
		  pj_sockaddr_get_port(&transport_state->volte.local_addr_s), transport_state->volte.local_spi_s);
	ast_debug(1, "xfrm: alg: %s ealg: %s\n", auth_algo.algo.alg_name, ciph_algo.algo.alg_name);

	status = xfrm_sa_add(g_mnl_socket, transport_state->volte.remote_spi_s, (const struct sockaddr *)&local_addr_c,
			     (const struct sockaddr *)&remote_addr_s, transport_state->volte.remote_spi_s,
			     &auth_algo.algo, &ciph_algo.algo);
	if (status)
		sa_add_failed = status;
	else
		transport_state->volte.local_sa_c_set = PJ_TRUE;
	status = xfrm_sa_add(g_mnl_socket, transport_state->volte.remote_spi_c, (const struct sockaddr *)&local_addr_s,
			     (const struct sockaddr *)&remote_addr_c, transport_state->volte.remote_spi_c,
			     &auth_algo.algo, &ciph_algo.algo);
	if (status)
		sa_add_failed = status;
	else
		transport_state->volte.local_sa_s_set = PJ_TRUE;
	status = xfrm_sa_add(g_mnl_socket, transport_state->volte.local_spi_s, (const struct sockaddr *)&remote_addr_c,
			     (const struct sockaddr *)&local_addr_s, transport_state->volte.local_spi_s,
			     &auth_algo.algo, &ciph_algo.algo);
	if (status)
		sa_add_failed = status;
	else
		transport_state->volte.remote_sa_c_set = PJ_TRUE;

	status = xfrm_sa_add(g_mnl_socket, transport_state->volte.local_spi_c, (const struct sockaddr *)&remote_addr_s,
			     (const struct sockaddr *)&local_addr_c, transport_state->volte.local_spi_c,
			     &auth_algo.algo, &ciph_algo.algo);
	if (status)
		sa_add_failed = status;
	else
		transport_state->volte.remote_sa_s_set = PJ_TRUE;
	status = xfrm_policy_add(g_mnl_socket, (const struct sockaddr *)&local_addr_c,
				(const struct sockaddr *)&remote_addr_s, transport_state->volte.remote_spi_s, false);
	if (status)
		sp_add_failed = status;
	else
		transport_state->volte.local_sp_c_set = PJ_TRUE;
	status = xfrm_policy_add(g_mnl_socket, (const struct sockaddr *)&local_addr_s,
				(const struct sockaddr *)&remote_addr_c, transport_state->volte.remote_spi_c, false);
	if (status)
		sp_add_failed = status;
	else
		transport_state->volte.local_sp_s_set = PJ_TRUE;
	status = xfrm_policy_add(g_mnl_socket, (const struct sockaddr *)&remote_addr_c,
				(const struct sockaddr *)&local_addr_s, transport_state->volte.local_spi_s, true);
	if (status)
		sp_add_failed = status;
	else
		transport_state->volte.remote_sp_c_set = PJ_TRUE;
	status = xfrm_policy_add(g_mnl_socket, (const struct sockaddr *)&remote_addr_s,
				(const struct sockaddr *)&local_addr_c, transport_state->volte.local_spi_c, true);
	if (status)
		sp_add_failed = status;
	else
		transport_state->volte.remote_sp_s_set = PJ_TRUE;

	if (sa_add_failed)
		ast_log(LOG_ERROR, "Failed to add IPSec SA.\n");
	if (sp_add_failed)
		ast_log(LOG_ERROR, "Failed to add IPSec SP.\n");

	return (sa_add_failed) ? sa_add_failed : sp_add_failed;
}

/* Header field names */
const pj_str_t STR_SUPPORTED = { "Supported", 9 };
const pj_str_t STR_REQUIRE = { "Require", 7 };
const pj_str_t STR_PROXY_REQUIRE = { "Proxy-Require", 13 };
const pj_str_t STR_PATH = { "path", 4 };
const pj_str_t STR_SEC_AGREE = { "sec-agree", 9 };
const pj_str_t STR_AUTHORIZATION = { "Authorization", 13 };
const pj_str_t STR_AUTS = { "auts", 4 };
const pj_str_t STR_SECURITY_CLIENT = { "Security-Client", 15 };
const pj_str_t STR_SECURITY_SERVER = { "Security-Server", 15 };
const pj_str_t STR_SECURITY_VERIFY = { "Security-Verify", 15 };
const pj_str_t STR_Q = { "q", 1 };
const pj_str_t STR_PROT = { "prot", 4 };
const pj_str_t STR_MOD = { "mod", 3 };
const pj_str_t STR_SPI_C = { "spi-c", 5 };
const pj_str_t STR_SPI_S = { "spi-s", 5 };
const pj_str_t STR_PORT_C = { "port-c", 6 };
const pj_str_t STR_PORT_S = { "port-s", 6 };
const pj_str_t STR_ALG = { "alg", 3 };
const pj_str_t STR_EALG = { "ealg", 4 };
const pj_str_t STR_P_ASSOCIATED_URI = { "P-Associated-URI", 16 };
const pj_str_t STR_P_ACCESS_NETWORK_INFO = { "P-Access-Network-Info", 21 };
const pj_str_t STR_EXPIRES = { "Expires", 7 };

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

/* Add a value to an array header. If the header does not exist, it is created. */
static pj_status_t add_value_array_hdr(pjsip_tx_data *tdata, const pj_str_t *name, const pj_str_t *value)
{
	pjsip_generic_array_hdr *hdr;
	pj_bool_t created = PJ_FALSE;

	/* Create header, if not yet created. */
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, name, NULL);
	if (!hdr) {
		hdr = pjsip_generic_array_hdr_create(tdata->pool, name);
		created = PJ_TRUE;
	}
	if (!hdr) {
		ast_log(LOG_ERROR, "Failed to create array header.");
		return -ENOMEM;
	}
	if (hdr->count == PJSIP_GENERIC_ARRAY_MAX_COUNT) {
		ast_log(LOG_ERROR, "Too many evalue in array, skipping %s.", value->ptr);
		return -E2BIG;
	}
	pj_strdup(tdata->pool, &hdr->values[hdr->count++], value);

	/* Append header, if created. */
	if (created)
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *)hdr);

	return PJ_SUCCESS;
}

/* Add security client header to SIP message. */
static pj_status_t add_securety_client_hdr(pjsip_tx_data *tdata, const struct ipsec_alg alg[],
					   const struct ipsec_alg ealg[], uint32_t spi_c, uint32_t spi_s,
					   uint16_t port_c, uint16_t port_s)
{
	pjsip_generic_array_hdr *hdr;
	char str[256];
	int i, j;

	/* remove old header */
	hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_SECURITY_CLIENT, NULL);
	if (hdr) {
		pj_list_erase(hdr);
	}

	/* Add Security-Client header. */
	hdr = pjsip_generic_array_hdr_create(tdata->pool, &STR_SECURITY_CLIENT);
	if (!hdr) {
		ast_log(LOG_ERROR, "Failed to create header.");
		return -ENOMEM;
	}

	/* Create tupple for given algorithms. */
	for (i = 0; alg[i].sip_name; i++) {
		for (j = 0; ealg[j].sip_name; j++) {
			snprintf(str, sizeof(str), "ipsec-3gpp; alg=%s; ealg=%s; spi-c=%u; spi-s=%u; "
				 "port-c=%u; port-s=%u", alg[i].sip_name, ealg[j].sip_name, spi_c, spi_s,
				 port_c, port_s);
			if (hdr->count == PJSIP_GENERIC_ARRAY_MAX_COUNT) {
				ast_log(LOG_ERROR, "Too many evalue in array, skipping '%s'.", str);
				continue;
			}
			pj_strdup2(tdata->pool, &hdr->values[hdr->count++], str);
		}
	}

	/* Append header */
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *)hdr);

	return PJ_SUCCESS;
}


/* Add Sec-Agree to header. */
pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata)
{
	pj_status_t status;

	/* "Require: sec-agree" */
	status = add_value_array_hdr(tdata, &STR_REQUIRE, &STR_SEC_AGREE);
	if (status)
		return status;
	/* "Proxy-Require: sec-agree" */
	status = add_value_string_hdr(tdata, &STR_PROXY_REQUIRE, &STR_SEC_AGREE);
	if (status)
		return status;
	/* "Supported: path,sec-agree" */
	status = add_value_array_hdr(tdata, &STR_SUPPORTED, &STR_PATH);
	if (status)
		return status;
	status = add_value_array_hdr(tdata, &STR_SUPPORTED, &STR_SEC_AGREE);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add initial Authorization header. */
pj_status_t volte_init_authorization(pjsip_tx_data *tdata, const char *fromdomain, const char *username)
{
	char authorization[1024];
	pj_status_t status;

	snprintf(authorization, sizeof(authorization),
		 "Digest uri=\"sip:%s\",username=\"%s\",response=\"\",realm=\"%s\",nonce=\"\"",
		 fromdomain, username, fromdomain);

	pj_str_t authorization_str = {authorization, strlen(authorization)};
	status = add_value_string_hdr(tdata, &STR_AUTHORIZATION, &authorization_str);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Remove initial Authorization header. */
pj_status_t volte_del_authorization(pjsip_tx_data *tdata)
{
	pjsip_authorization_hdr *auth_hdr;

	/* remove double authentication header */
	auth_hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_AUTHORIZATION, NULL);
	if (auth_hdr) {
		pj_list_erase(auth_hdr);
	}

	return PJ_SUCCESS;
}

/* Reset old transport and clear IPSec transformations */
pj_status_t volte_reset_transport(struct ast_sip_transport_state *transport_state)
{
	pj_status_t status;
	int old_port_c, old_port_s;

	/* Cleanup IPSec transform. */
	volte_cleanup_xfrm(transport_state);

	/* Cleanup old transports. */
	old_port_s = pj_sockaddr_get_port(&transport_state->volte.local_addr_s);
	if (old_port_s > 0 && old_port_s < 65535 && transport_state->volte.transport) {
		/* Create factory with original transport port. */
		status = pjsip_tcp_transport_restart(transport_state->volte.transport->factory,
						     &transport_state->volte.local_addr_orig, NULL);
		if (status != PJ_SUCCESS) {
			ast_log(LOG_ERROR, "Failed to change server connection addresses (errno=%d).\n", errno);
			return status;
		}
	}
	old_port_c = pj_sockaddr_get_port(&transport_state->volte.local_addr_c);
	if (old_port_c > 0 && old_port_c < 65535 && transport_state->volte.transport) {
		/* Create outgoing socket with original transport port. */
		status = transport_state->volte.transport->create_new_sock(transport_state->volte.transport, NULL);
		if (status != PJ_SUCCESS) {
			ast_log(LOG_ERROR, "Failed to get connection addresses (errno=%d).\n", errno);
			return status;
		}
		status = transport_state->volte.transport->connect_new_sock(transport_state->volte.transport,
				&transport_state->volte.local_addr_c, &transport_state->volte.remote_addr_orig);
		if (status != PJ_SUCCESS) {
			ast_log(LOG_ERROR, "Failed to change connection addresses (errno=%d).\n", errno);
			return status;
		}
	}

	transport_state->volte.transport = NULL;

	return PJ_SUCCESS;
}

/* Alloc transport. */
pj_status_t volte_alloc_transport(struct ast_sip_transport_state *transport_state)
{
	pj_status_t status;

	/* Reset addresses. */
	memset(&transport_state->volte.local_addr_c, 0, sizeof(pj_sockaddr));
	transport_state->volte.local_addr_c.addr.sa_family = PJ_AF_INET;
	memset(&transport_state->volte.local_addr_s, 0, sizeof(pj_sockaddr));
	transport_state->volte.local_addr_s.addr.sa_family = PJ_AF_INET;
	memset(&transport_state->volte.remote_addr_c, 0, sizeof(pj_sockaddr));
	transport_state->volte.remote_addr_c.addr.sa_family = PJ_AF_INET;
	memset(&transport_state->volte.remote_addr_s, 0, sizeof(pj_sockaddr));
	transport_state->volte.remote_addr_s.addr.sa_family = PJ_AF_INET;

	/* Set new local ports */
	pj_sockaddr_set_port(&transport_state->volte.local_addr_c, transport_state->volte.local_port_c);
	pj_sockaddr_set_port(&transport_state->volte.local_addr_s, transport_state->volte.local_port_s);

	/* Allocate SPI */
	status = volte_alloc_spi(transport_state);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Add security client header. */
pj_status_t volte_add_security_client(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata)
{
	int local_port_c, local_port_s;
	pj_status_t status;

	/* Get local ports. */
	local_port_c = pj_sockaddr_get_port(&transport_state->volte.local_addr_c);
	local_port_s = pj_sockaddr_get_port(&transport_state->volte.local_addr_s);

	status = add_securety_client_hdr(tdata, g_ipsec_alg, g_ipsec_ealg, transport_state->volte.local_spi_c,
					 transport_state->volte.local_spi_s, local_port_c, local_port_s);
	if (status)
		return status;

	return PJ_SUCCESS;
}

/* Set new transport and set IPSec transformations */
pj_status_t volte_set_transport(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata,
				const pj_str_t *alg, const pj_str_t *ealg, uint8_t *ik, uint32_t remote_spi_c,
				uint32_t remote_spi_s, uint16_t remote_port_c, uint16_t remote_port_s)
{
	int local_port_c, local_port_s;
	pj_status_t status;

	/* Cleanup IPSec transform. */
	volte_cleanup_xfrm(transport_state);

	/* Get addresses from current transport and replace the local ports. */
	local_port_c = pj_sockaddr_get_port(&transport_state->volte.local_addr_c);
	local_port_s = pj_sockaddr_get_port(&transport_state->volte.local_addr_s);
	memcpy(&transport_state->volte.local_addr_c, &tdata->tp_info.transport->local_addr, sizeof(pj_sockaddr));
	memcpy(&transport_state->volte.local_addr_s, &tdata->tp_info.transport->local_addr, sizeof(pj_sockaddr));
	memcpy(&transport_state->volte.local_addr_orig, &tdata->tp_info.transport->factory->local_addr, sizeof(pj_sockaddr));
	if (transport_state->volte.local_addr_orig.addr.sa_family == PJ_AF_INET6) {
		memset(&transport_state->volte.local_addr_orig.ipv6.sin6_addr, 0,
		       sizeof(transport_state->volte.local_addr_orig.ipv6.sin6_addr));
	} else {
		memset(&transport_state->volte.local_addr_orig.ipv4.sin_addr, 0,
		       sizeof(transport_state->volte.local_addr_orig.ipv4.sin_addr));
	}

	memcpy(&transport_state->volte.remote_addr_c, &tdata->tp_info.dst_addr, sizeof(pj_sockaddr));
	memcpy(&transport_state->volte.remote_addr_s, &tdata->tp_info.dst_addr, sizeof(pj_sockaddr));
	memcpy(&transport_state->volte.remote_addr_orig, &tdata->tp_info.dst_addr, sizeof(pj_sockaddr));
	pj_sockaddr_set_port(&transport_state->volte.local_addr_c, local_port_c);
	pj_sockaddr_set_port(&transport_state->volte.local_addr_s, local_port_s);
	pj_sockaddr_set_port(&transport_state->volte.remote_addr_c, remote_port_c);
	pj_sockaddr_set_port(&transport_state->volte.remote_addr_s, remote_port_s);

	/* Set IPSec transform. */
	transport_state->volte.remote_spi_c = remote_spi_c;
	transport_state->volte.remote_spi_s = remote_spi_s;
	volte_set_xfrm(transport_state, alg, ealg, ik);

	/* Create sockets (listening and outgoing) with new transport ports. */
	if (!tdata->tp_info.transport || !tdata->tp_info.transport->factory) {
		ast_log(LOG_ERROR, "The Message has no transport or no transport factory. Please fix!\n");
		return -EINVAL;
	}
	status = pjsip_tcp_transport_restart(tdata->tp_info.transport->factory, &transport_state->volte.local_addr_s,
					     NULL);
	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Failed to change server connection addresses (errno=%d).\n", errno);
		return status;
	}
	if (!tdata->tp_info.transport->create_new_sock || !tdata->tp_info.transport->connect_new_sock) {
		ast_log(LOG_ERROR, "The transport protocol does not support socket change. Please fix!\n");
		return -ENOTSUP;
	}
	status = tdata->tp_info.transport->create_new_sock(tdata->tp_info.transport,
								 &transport_state->volte.local_addr_c);
	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Failed to get client connection addresses (errno=%d).\n", errno);
		return status;
	}
	status = tdata->tp_info.transport->connect_new_sock(tdata->tp_info.transport,
								  &transport_state->volte.local_addr_c,
								  &transport_state->volte.remote_addr_s);
	if (status != PJ_SUCCESS) {
		ast_log(LOG_ERROR, "Failed to change client connection addresses (errno=%d).\n", errno);
		return status;
	}
	transport_state->volte.transport = tdata->tp_info.transport;

	return PJ_SUCCESS;
}

static void on_syntax_error(pj_scanner *scanner)
{
	PJ_UNUSED_ARG(scanner);
}

/* Store and parse security server from SIP header and fill a structure. */
pj_status_t volte_get_security_server(struct ast_sip_transport_state *transport_state, pjsip_rx_data *rdata,
				      struct security_server *sec)
{
	pjsip_generic_string_hdr *sec_hdr;
	struct security_server secs[8];
	pj_scanner scanner;
	int idx, i, j, q_index;
	float q_best = 0.0;

	/* Get Security-Server from header. */
	sec_hdr = pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_SECURITY_SERVER, NULL);
	if (!sec_hdr || !sec_hdr->hvalue.ptr) {
		ast_log(LOG_ERROR, "Missing 'Security-Server' in REGISTER response.");
		return -EINVAL;
	}

	/* Store for security verify. */
	if (sec_hdr->hvalue.slen < sizeof(transport_state->volte.security_server)) {
		memcpy(transport_state->volte.security_server, sec_hdr->hvalue.ptr, sec_hdr->hvalue.slen);
		transport_state->volte.security_server[sec_hdr->hvalue.slen] = '\0';
	} else {
		ast_log(LOG_ERROR, "Security-Server' too large.");
		return -EINVAL;
	}

	memset(secs, 0, sizeof(secs));

	/* NOTE: The string to parse must be null-terminated. */
	pj_scan_init(&scanner, transport_state->volte.security_server, strlen(transport_state->volte.security_server),
		     0, &on_syntax_error);

	idx = 0;
	for (;;) {
		pj_str_t name, value;

		pjsip_parse_param_imp(&scanner, rdata->tp_info.pool, &name, &value, 0);

		if (!pj_stricmp(&name, &STR_Q)) {
			secs[idx].q = pj_strtof(&value);
		} else
		if (!pj_stricmp(&name, &STR_PROT)) {
			secs[idx].prot = value;
		} else
		if (!pj_stricmp(&name, &STR_MOD)) {
			secs[idx].mod = value;
		} else
		if (!pj_stricmp(&name, &STR_SPI_C)) {
			secs[idx].spi_c = value;
		} else
		if (!pj_stricmp(&name, &STR_SPI_S)) {
			secs[idx].spi_s = value;
		} else
		if (!pj_stricmp(&name, &STR_PORT_C)) {
			secs[idx].port_c = value;
		} else
		if (!pj_stricmp(&name, &STR_PORT_S)) {
			secs[idx].port_s = value;
		} else
		if (!pj_stricmp(&name, &STR_ALG)) {
			secs[idx].alg = value;
		} else
		if (!pj_stricmp(&name, &STR_EALG)) {
			secs[idx].ealg = value;
		}

		if (pj_scan_is_eof(&scanner))
			break;

		/* Eat semicolon */
		if (*scanner.curptr == ';')
			pj_scan_get_char(&scanner);

		/* Eat comma, go to next sec index */
		if (*scanner.curptr == ',') {
			pj_scan_get_char(&scanner);
			if (++idx == 8)
				break;
		}
	}
	pj_scan_fini(&scanner);

	q_index = -1;
	for (idx = 0; idx < 8; idx++) {
		if (!secs[idx].spi_c.ptr && !secs[idx].spi_s.ptr && !secs[idx].port_c.ptr &&
		    !secs[idx].port_s.ptr && !secs[idx].alg.ptr) {
			break;
		}
		ast_debug(1, "'Security-Server candidate #%d: "
			  "prot=%.*s, spi-c=%.*s, spi-s=%.*s, port-c=%.*s, port-s=%.*s, alg=%.*s, ealg=%.*s, q=%.1f\n",
			  idx, fmt_str(secs[idx].prot), fmt_str(secs[idx].spi_c), fmt_str(secs[idx].spi_s),
			  fmt_str(secs[idx].port_c), fmt_str(secs[idx].port_s), fmt_str(secs[idx].alg),
			  fmt_str(secs[idx].ealg), secs[idx].q);
		if (!secs[idx].spi_c.ptr || !secs[idx].spi_s.ptr || !secs[idx].port_c.ptr ||
		    !secs[idx].port_s.ptr || !secs[idx].alg.ptr) {
			ast_log(LOG_ERROR, "Missing 'Security-Server' element(s) in REGISTER response.\n");
			continue;
		}
		if (q_index < 0 || secs[idx].q > q_best) {
			q_best = secs[idx].q;
			q_index = idx;
		}
	}
	if (q_index < 0) {
		ast_log(LOG_ERROR, "No candidate in Security-Server header found.\n");
		return -EINVAL;
	}
	ast_debug(1, "'Selecting best candidate #%d: ", q_index);
	memcpy(sec, &secs[q_index], sizeof(*sec));

	for (i = 0; g_ipsec_alg[i].sip_name; i++) {
		if (!pj_strncmp2(&sec->alg, g_ipsec_alg[i].sip_name, strlen(g_ipsec_alg[i].sip_name))) {
			break;
		}
	}
	if (!g_ipsec_alg[i].kernel_name) {
		ast_log(LOG_ERROR, "Given 'alg=%.*s' in Security-Server header not found.\n", fmt_str(sec->alg));
		return -EINVAL;
	}
	if (sec->ealg.slen) {
		for (j = 0; g_ipsec_ealg[j].sip_name; j++) {
			if (!pj_strncmp2(&sec->ealg, g_ipsec_ealg[j].sip_name, strlen(g_ipsec_ealg[j].sip_name))) {
				break;
			}
		}
		if (!g_ipsec_ealg[j].kernel_name) {
			ast_log(LOG_ERROR, "Given 'ealg=%.*s' in Security-Server header not found.\n", fmt_str(sec->ealg));
			return -EINVAL;
		}
	} else {
		ast_log(LOG_NOTICE, "No 'ealg' in security-Server header found, assuming 'null'.\n");
		pj_cstr(&sec->ealg, "null");
	}

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

pj_status_t volte_hex_to_octet_string(const char *name, const char *input, uint8_t *output, size_t output_size)
{
	int i, n;

	if (!input || !input[0]) {
		ast_log(LOG_ERROR, "Missing value for hex string '%s'.\n", name);
		return -EINVAL;
	}
	i = n = 0;
	while (*input) {
		if (i == output_size) {
			ast_log(LOG_ERROR, "Value for hex string '%s' too long, expecting %zu bytes.\n", name,
				output_size);
			return -EINVAL;
		}
		if (*input >= '0' && *input <= '9')
			output[i] = (output[i] << 4) | (*input - '0');
		else if (*input >= 'a' && *input <= 'f')
			output[i] = (output[i] << 4) | (*input - 'a' + 10);
		else if (*input >= 'A' && *input <= 'F')
			output[i] = (output[i] << 4) | (*input - 'A' + 10);
		else {
			ast_log(LOG_ERROR, "Value for hex string '%s' has invalid character '%c'.\n", name, *input);
			return -EINVAL;
		}
		if (++n == 2) {
			n = 0;
			i++;
		}
		input++;
	}
	if (i < output_size) {
		ast_log(LOG_ERROR, "Value for hex string '%s' too short, expecting %zu bytes.\n", name, output_size);
		return -EINVAL;
	}

	return PJ_SUCCESS;
}

pj_status_t volte_get_auth(pjsip_rx_data *rdata, pjsip_hdr_e auth_type, pj_str_t *algo, uint8_t *rand, uint8_t *autn)
{
	pjsip_www_authenticate_hdr *auth_hdr;
	uint8_t rand_autn[32];

	auth_hdr = pjsip_msg_find_hdr(rdata->msg_info.msg, auth_type, NULL);
	if (!auth_hdr || !auth_hdr->challenge.digest.nonce.ptr || !auth_hdr->challenge.digest.algorithm.ptr) {
		ast_log(LOG_ERROR, "Authentication header missing or incomplete in REGISTER response.\n");
		return -EINVAL;
	}

	if (!pj_strncmp2(&auth_hdr->challenge.digest.algorithm, "AKAv2-MD5", 9)) {
		ast_log(LOG_ERROR, "Authentication algorithm not supported. Please fix!\n");
		return -EINVAL;
	}
	if (!!pj_strncmp2(&auth_hdr->challenge.digest.algorithm, "AKAv1-MD5", 9)) {
		ast_log(LOG_ERROR, "Authentication algorithm not supported.\n");
		return -EINVAL;
	}

	*algo = auth_hdr->challenge.digest.algorithm;

	ast_base64decode(rand_autn, auth_hdr->challenge.digest.nonce.ptr, sizeof(rand_autn));

	memcpy(rand, rand_autn, 16);
	memcpy(autn, rand_autn + 16, 16);

	return PJ_SUCCESS;
}

pj_status_t volte_send_authrequest(const char *registration_name, pj_str_t *algo, uint8_t *rand, uint8_t *autn)
{
	char rand_str[33] = "                                ", autn_str[33] = "                                ";
	int i;

	for (i = 0; i < 16; i++) {
		sprintf(rand_str + 2 * i, "%02x", rand[i]);
		sprintf(autn_str + 2 * i, "%02x", autn[i]);
	}
	manager_event(0, "AuthRequest", "Registration: %s\r\nAlgorithm: %.*s\r\nRAND: %s\r\nAUTN: %s\r\n",
		      registration_name, fmt_strp(algo), rand_str, autn_str);

	return PJ_SUCCESS;
}

pj_status_t volte_authenticate(const char *opc_str, const char *k_str, const char *sqn_str, uint8_t *rand,
			       uint8_t *autn, uint8_t *out_res, int *out_res_len, uint8_t *out_ik, uint8_t *out_ck,
			       uint8_t *out_auts, bool use_xor)
{
	uint8_t opc[16], k[16], sqn[6];
	int rc;
	pj_status_t status;

	status = volte_hex_to_octet_string("OPC", opc_str, opc, sizeof(opc));
	if (status)
		return status;
	status = volte_hex_to_octet_string("K", k_str, k, sizeof(k));
	if (status)
		return status;
	status = volte_hex_to_octet_string("SQN", sqn_str, sqn, sizeof(sqn));
	if (status)
		return status;

	/* Do XOR. See https://osmocom.org/attachments/2780/0001-WIP-Add-support-for-XOR-authentication.patch */
	if (use_xor) {
		uint8_t xdout[16];
		int i;

		/* s1 */
		for (i = 0; i < 16; i++)
			xdout[i] = k[i] ^ rand[i];
		/* s2 */
		memcpy(out_res, xdout, 16);
		*out_res_len = 16;
		/* ck */
		memcpy(out_ck, xdout+1, 16 - 1);
		out_ck[15] = xdout[0];
		/* ik */
		memcpy(out_ik, xdout+2, 16 - 2);
		memcpy(out_ik + 14, xdout, 2);

		return PJ_SUCCESS;
	}

	rc = milenage_check(opc, k, sqn, rand, autn, out_ik, out_ck, out_res, out_res_len, out_auts);
	if (rc == -1) {
		ast_log(LOG_ERROR, "Milenage authentication failed.\n");
		return -EINVAL;
	}

	if (rc == -2) {
		/* Tell the caller to perform resync process. */
		return -EAGAIN;
	}

	return PJ_SUCCESS;
}

pj_status_t volte_add_auts(pjsip_tx_data *tdata, uint8_t *auts)
{
	pjsip_authorization_hdr *auth_hdr;
	char enc_auts[23] = "\"                    \"";
	pjsip_param *param;

	auth_hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_AUTHORIZATION, NULL);
	if (!auth_hdr) {
		ast_log(LOG_ERROR, "Authorization header not found in TX message. Please fix!\n");
		return -EINVAL;
	}

	/* Encode base64. */
	ast_base64encode(enc_auts + 1, auts, 14, 21);
	enc_auts[21] = '"';

	/* Append 'auts' to Authorization header and replace that header. */
	param = PJ_POOL_ALLOC_T(tdata->pool, pjsip_param);
	if (!param)
		return -ENOMEM;
	param->name = STR_AUTS;
	pj_strdup2(tdata->pool, &param->value, enc_auts);
	pj_list_insert_before(&auth_hdr->credential.digest.other_param, param);

	return PJ_SUCCESS;
}

pj_status_t volte_store_cnonce_nc(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata)
{
	pjsip_authorization_hdr *auth_hdr;

	auth_hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_AUTHORIZATION, NULL);
	if (!auth_hdr) {
		ast_log(LOG_ERROR, "Authorization header not found in TX message. Please fix!\n");
		return -EINVAL;
	}

	/* Store cnonce and nc for unregister. */
	if (auth_hdr->credential.digest.cnonce.slen < sizeof(transport_state->volte.cnonce)) {
		char nc_str[auth_hdr->credential.digest.nc.slen + 1];
		memcpy(transport_state->volte.cnonce, auth_hdr->credential.digest.cnonce.ptr,
		       auth_hdr->credential.digest.cnonce.slen);
		transport_state->volte.cnonce[auth_hdr->credential.digest.cnonce.slen] = '\0';
		memcpy(nc_str, auth_hdr->credential.digest.nc.ptr, auth_hdr->credential.digest.nc.slen);
		nc_str[auth_hdr->credential.digest.nc.slen] = '\0';
		transport_state->volte.nc = strtoul(nc_str, NULL, 16);
	} else {
		ast_log(LOG_ERROR, "cnonce too large.");
		return -EINVAL;
	}

	return PJ_SUCCESS;
}

/* Store and get P-Associated-URI. */
pj_status_t volte_get_p_associated_uri(struct ast_sip_transport_state *transport_state, pjsip_rx_data *rdata)
{
	pjsip_generic_string_hdr *pau_hdr;
	int i, start = 0, end = 0;

	/* Get P-Associated-URI from header. */
	pau_hdr = pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_P_ASSOCIATED_URI, NULL);
	if (!pau_hdr || !pau_hdr->hvalue.ptr) {
		ast_log(LOG_NOTICE, "Missing 'P-Associated-URI' in REGISTER response.");
		return -EINVAL;
	}

	/* Get first entry, enclosed by <>. */
	for (i = 0; i < pau_hdr->hvalue.slen; i++) {
		if (!start && pau_hdr->hvalue.ptr[i] == '<')
			start = i + 1;
		if (!end && pau_hdr->hvalue.ptr[i] == '>')
			end = i;
	}
	if (!start || !end) {
		ast_log(LOG_ERROR, "Missing a value, enclosed by '<....>' in 'P-Associated-URI' in REGISTER response.");
		return -EINVAL;
	}

	/* Store. */
	if (end - start < sizeof(transport_state->volte.p_associated_uri)) {
		memcpy(transport_state->volte.p_associated_uri, pau_hdr->hvalue.ptr + start, end - start);
		transport_state->volte.p_associated_uri[end - start] = '\0';
	} else {
		ast_log(LOG_ERROR, "P-Associated-URI' too large.");
		return -EINVAL;
	}

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

/* Set Expires header to 0. */
pj_status_t volte_expires_0(pjsip_tx_data *tdata)
{
	pjsip_generic_int_hdr *expires_hdr;

	/* remove double authentication header */
	expires_hdr = pjsip_msg_find_hdr_by_name(tdata->msg, &STR_EXPIRES, NULL);
	if (expires_hdr) {
		expires_hdr->ivalue = 0;
	}

	return PJ_SUCCESS;
}

const char *volte_add_contact_params(const char *imei)
{
	static char contact[256], imei_str[64];

	if (!imei || !imei[0])
		imei = "000000000000000";

	if (strlen(imei) == 15) {
		sprintf(imei_str, "%.8s-%.6s-%s", imei, imei + 8, imei + 14);
		imei = imei_str;
	}

	sprintf(contact, "+g.3gpp.icsi-ref=\"urn%%3Aurn-7%%3A3gpp-service.ims.icsi.mmtel\";"
			 "audio;+sip.instance=\"<urn:gsma:imei:%s>\"", imei);

	return contact;
}
