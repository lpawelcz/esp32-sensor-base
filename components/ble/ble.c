#include "ble.h"
#include "veml6075.h"

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	switch (event) {
		case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
			esp_ble_gap_start_advertising(&ble_adv_params);
			break;
		case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
			if(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
				ESP_LOGW(TAG_BLE, "BLE Advertising started: %d",
						  param->adv_start_cmpl.status);
			} else {
				ESP_LOGE(TAG_BLE, "Unable to start advertising process: %d", param->scan_start_cmpl.status);
			}
			break;
		default:
			ESP_LOGE(TAG_BLE, "Event %d unhandled", event);
			break;
	}
}

int ble_prepare_binary_measurements(struct measurements *meas, u8 *adv, u8 bytes_cnt)
{
	u8 i;
	adv[0] = 0x02;
	adv[1] = 0x01;
	adv[2] = 0x06;
	adv[3] = bytes_cnt - 4;
	adv[4] = 0xFF;
	adv[5] = 0x02;
	adv[6] = 0xE5;
	i = 7;
	float2bytes(meas->uv_res->uv_a, &adv[i]);
	i += 4;
	float2bytes(meas->uv_res->uv_b, &adv[i]);
	i += 4;
	float2bytes(meas->uv_res->uv_i, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res->temp, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res->press, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res->hum, &adv[i]);
	i += 4;

	ESP_LOGW(TAG_BLE, "adv hex: %02X|%02X|%02X|%02X|%02X|%02X|%02X|",
		adv[0], adv[1], adv[2], adv[3], adv[4], adv[5], adv[6]);
	for (i = 7; i < bytes_cnt; i += 4) {
		ESP_LOGW(TAG_BLE, "converted float hex: 0x%02X%02X%02X%02X",
			adv[i+3], adv[i+2], adv[i+1], adv[i+0]);
	}

	return 0;
}

int ble_transmit_measurements(u8 *adv, u8 bytes_cnt)
{

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	esp_bt_controller_init(&bt_cfg);

	esp_bt_controller_enable(ESP_BT_MODE_BLE);

	esp_bluedroid_init();
	esp_bluedroid_enable();

	ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
	ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv, bytes_cnt));

	return 0;
}
