#pragma once

#include "netlink_xfrm.h"

struct security_server {
	float q;
	pj_str_t prot;
	pj_str_t mod;
	pj_str_t spi_c;
	pj_str_t spi_s;
	pj_str_t port_c;
	pj_str_t port_s;
	pj_str_t alg;
	pj_str_t ealg;
};

pj_status_t g_volte_init(void);
void g_volte_exit(void);

void volte_cleanup_xfrm(struct ast_sip_transport_state *transport_state);
pj_status_t volte_alloc_spi(struct ast_sip_transport_state *transport_state);

pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata);
pj_status_t volte_init_authorization(pjsip_tx_data *tdata, const char *fromdomain, const char *username);
pj_status_t volte_del_authorization(pjsip_tx_data *tdata);
pj_status_t volte_reset_transport(struct ast_sip_transport_state *transport_state);
pj_status_t volte_reset_transport_factory(struct ast_sip_transport_state *transport_state);
pj_status_t volte_alloc_transport(struct ast_sip_transport_state *transport_state);
pj_status_t volte_add_security_client(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_set_transport(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata,
				const pj_str_t *alg, const pj_str_t *ealg, uint8_t *ik, uint8_t *ck, uint32_t remote_spi_c,
				uint32_t remote_spi_s, uint16_t remote_port_c, uint16_t remote_port_s);
pj_status_t volte_get_security_server(struct ast_sip_transport_state *transport_state, pjsip_rx_data *rdata,
				      struct security_server *sec);
pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_hex_to_octet_string(const char *name, const char *input, uint8_t *output, size_t output_size);
pj_status_t volte_get_auth(pjsip_rx_data *rdata, pjsip_hdr_e auth_type, pj_str_t *algo, uint8_t *rand, uint8_t *autn,
			   uint8_t *server_data, size_t *sizeof_server_data);
pj_status_t volte_send_authrequest(const char *registration_name, pj_str_t *algo, uint8_t *rand, uint8_t *autn);
pj_status_t volte_authenticate(const char *opc_str, const char *k_str, const char *sqn_str, uint8_t *rand,
			       uint8_t *autn, uint8_t *out_res, int *out_res_len, uint8_t *out_ik, uint8_t *out_ck,
			       uint8_t *out_auts, bool use_xor);
pj_status_t volte_add_auts(pjsip_tx_data *tdata, uint8_t *auts);
pj_status_t volte_store_cnonce_nc(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_get_p_associated_uri(struct ast_sip_transport_state *transport_state, pjsip_rx_data *rdata);
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info);
pj_status_t volte_expires_0(pjsip_tx_data *tdata);
const char *volte_add_contact_params(const char *imei, const char *accesstype);
