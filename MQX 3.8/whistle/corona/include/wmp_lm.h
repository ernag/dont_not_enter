


#ifndef WMP_LM_H_
#define WMP_LM_H_


#include "whistlemessage.pb.h"
#include "wmp.h"
#include "wifi_mgr.h"
#include "cfg_mgr.h"

int WMP_lm_receive_explicit(uint8_t *pbuffer, size_t buffer_size, void *pLmMsg, LmMessageType lmMsgType);

size_t WMP_lm_checkin_req_gen(uint8_t *pencode_buffer, size_t buffer_size, int key);
size_t WMP_lm_dev_stat_notify_gen(uint8_t *pencode_buffer, size_t buffer_size, LmDevStatus status);
size_t WMP_lm_test_result_gen(uint8_t *pencode_buffer, 
                size_t buffer_size, 
                LmWiFiTestStatus testResult, 
                char *ssid, 
                security_parameters_t *security_param);

int WMP_lm_checkin_resp_parse(uint8_t *pbuffer, size_t buffer_size, LmMobileStat *plmMobileStat);

err_t WMP_cfg_network_type_to_lmm_network_encode(cfg_network_t *pCfgNetwork, LmWiFiNetwork *pOutLmmNetwork);

#endif
