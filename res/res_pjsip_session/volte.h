#pragma once

pj_status_t volte_add_accept(pjsip_tx_data *tdata);
pj_status_t volte_add_accept_contact(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_sec_agree(pjsip_tx_data *tdata);
pj_status_t volte_add_security_verify(struct ast_sip_transport_state *transport_state, pjsip_tx_data *tdata);
pj_status_t volte_add_p_access_network_info(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_p_preferred_service(pjsip_tx_data *tdata, char *info);
pj_status_t volte_add_p_early_media_recvonly(pjsip_tx_data *tdata);
pj_status_t volte_add_p_early_media_supported(pjsip_tx_data *tdata);
void volte_add_contact_params(pjsip_tx_data *tdata, const char **params);
