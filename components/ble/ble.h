#ifndef _BLE_H
#define _BLE_H

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include "common.h"

#define BLE_COMP_ID	0x02E5		/* Espressif Inc */
#define TAG_BLE		"BLE"

typedef unsigned char u8;
typedef unsigned int u32;
struct measurements;

static inline void float2bytes(float float_val, u8 *arr)
{
	u32 int_val, i;

	int_val = *((int*)&float_val);

	for (i = 0; i < 4; ++i) {
		arr[i] = (int_val >> (8 * i)) & 0xFF;
	}
}

static esp_ble_adv_params_t ble_adv_params = {
	/* Advertising interval */
	.adv_int_min = 0x1000,
	.adv_int_max = 0x2000,
	/* Advertising mode: non-connectable */
	.adv_type = ADV_TYPE_NONCONN_IND,
	.own_addr_type  = BLE_ADDR_TYPE_PUBLIC,
	/* Advertising channel list: all 3 channels: 37, 38, 39 */
	.channel_map = ADV_CHNL_ALL,
	/* Allow both scan and connection requests from anyone */
	.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,

};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
int ble_prepare_binary_measurements(struct measurements *meas, u8 *adv, u8 bytes_cnt);
int ble_transmit_measurements(u8 *adv, u8 bytes_cnt);

#endif
/* ifndef _BLE_H */
