#pragma once

pj_status_t volte_add_accept(pjsip_tx_data *tdata);
pj_status_t volte_add_accept_contact(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata);
pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_p_preferred_service(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_precondition(pjsip_tx_data *tdata, pj_bool_t supported);
pj_status_t volte_add_p_early_media_recvonly(pjsip_tx_data *tdata);
pj_status_t volte_add_p_early_media_supported(pjsip_tx_data *tdata);
void volte_add_contact_params(pjsip_tx_data *tdata, const char *contact_user, const char **params);
void volte_init_sdp_qos(struct ast_sip_session_qos_status *local_status);
pj_status_t volte_parse_sdp_qos(const pjmedia_sdp_media *media, struct ast_sip_session_qos_status *status);
pj_status_t volte_add_sdp_qos(pj_pool_t *pool, pjmedia_sdp_media *media, struct ast_sip_session_qos_status *status);
pj_status_t volte_add_sdp_bandwidth_session(pj_pool_t *pool, pjmedia_sdp_session *session, pj_uint32_t bw_value);
pj_status_t volte_add_sdp_bandwidth_media(pj_pool_t *pool, pjmedia_sdp_media *media, pj_uint32_t bw_value);
pj_status_t volte_negotiate_sdp_qos(struct ast_sip_session_qos_status *local_status,
				    struct ast_sip_session_qos_status *remote_status,
				    const char *reason);
pj_status_t volte_update_sdp_qos(struct ast_sip_session_qos_status *local_status);
pj_status_t volte_confirm_sdp_qos(struct ast_sip_session_qos_status *local_status);
pj_bool_t volte_is_supported_precondition(pjsip_rx_data *rdata);
pj_status_t hack_evs(pjmedia_sdp_media *media);
