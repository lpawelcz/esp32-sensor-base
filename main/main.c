#include "i2c.h"
#include "common.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "sdkconfig.h" // generated by "make menuconfig"

#include "bme280.h"
#include "veml6075.h"

#define TAG_BME280 "BME280"
#define TAG_DPSP "DEEP SLEEP"


#include <sys/time.h>
#include "esp_sleep.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "ble.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"
#include "esp_bt.h"
#include "esp_wifi.h"
#include "driver/adc.h"
#include "esp_pm.h"

#include "stdio.h"

/* global variables */
TaskHandle_t sleep_task_h;
SemaphoreHandle_t measure_mutex;
/**
 * Logs information about a connection to the console.
 */
static void bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
	ESP_LOGI(BLE_TAG, "handle=%d our_ota_addr_type=%d our_ota_addr=",
				desc->conn_handle, desc->our_ota_addr.type);
	print_addr(desc->our_ota_addr.val);
	ESP_LOGI(BLE_TAG, " our_id_addr_type=%d our_id_addr=",
					desc->our_id_addr.type);
	print_addr(desc->our_id_addr.val);
	ESP_LOGI(BLE_TAG, " peer_ota_addr_type=%d peer_ota_addr=",
					desc->peer_ota_addr.type);
	print_addr(desc->peer_ota_addr.val);
	ESP_LOGI(BLE_TAG, " peer_id_addr_type=%d peer_id_addr=",
					desc->peer_id_addr.type);
	print_addr(desc->peer_id_addr.val);
	ESP_LOGI(BLE_TAG, " conn_itvl=%d conn_latency=%d supervision_timeout=%d"
				   " encrypted=%d authenticated=%d bonded=%d\n",
	desc->conn_itvl, desc->conn_latency,
	desc->supervision_timeout,
	desc->sec_state.encrypted,
	desc->sec_state.authenticated,
	desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void bleprph_advertise(void)
{
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	const char *name;
	int rc;

	/**
	*  Set the advertisement data included in our advertisements:
	*     o Flags (indicates advertisement type and other general info).
	*     o Advertising tx power.
	*     o Device name.
	*     o 16-bit service UUIDs (alert notifications).
	*/

	memset(&fields, 0, sizeof fields);

	/* Advertise two flags:
	*     o Discoverability in forthcoming advertisement (general)
	*     o BLE-only (BR/EDR unsupported).
	*/
	fields.flags = BLE_HS_ADV_F_DISC_GEN |
		   BLE_HS_ADV_F_BREDR_UNSUP;

	/* Indicate that the TX power level field should be included; have the
	* stack fill this value automatically.  This is done by assigning the
	* special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
	*/
	fields.tx_pwr_lvl_is_present = 1;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

	name = ble_svc_gap_device_name();
	fields.name = (uint8_t *)name;
	fields.name_len = strlen(name);
	fields.name_is_complete = 1;

	fields.uuids16 = (ble_uuid16_t[]) {
		BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
	};
	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGE(BLE_TAG, "error setting advertisement data; rc=%d\n", rc);
		return;
	}

	/* Begin advertising. */
	memset(&adv_params, 0, sizeof adv_params);
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
			   &adv_params, bleprph_gap_event, NULL);
	if (rc != 0) {
		ESP_LOGE(BLE_TAG, "error enabling advertisement; rc=%d\n", rc);
		return;
	}
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
	struct ble_gap_conn_desc desc;
	int rc;

	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		/* A new connection was established or a connection attempt failed. */
		ESP_LOGI(BLE_TAG, "connection %s; status=%d ",
			    event->connect.status == 0 ? "established" : "failed",
			    event->connect.status);
		if (event->connect.status == 0) {
			rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			assert(rc == 0);
			bleprph_print_conn_desc(&desc);
		}
		ESP_LOGI(BLE_TAG, "\n");

		if (event->connect.status != 0) {
			/* Connection failed; resume advertising. */
			bleprph_advertise();
		}
		return 0;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(BLE_TAG, "disconnect; reason=%d ", event->disconnect.reason);
		bleprph_print_conn_desc(&event->disconnect.conn);
		ESP_LOGI(BLE_TAG, "\n");

		/* Connection terminated; resume advertising. */
		bleprph_advertise();
		return 0;

	case BLE_GAP_EVENT_CONN_UPDATE:
		/* The central has updated the connection parameters. */
		ESP_LOGI(BLE_TAG, "connection updated; status=%d ",
			    event->conn_update.status);
		rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
		assert(rc == 0);
		bleprph_print_conn_desc(&desc);
		ESP_LOGI(BLE_TAG, "\n");
		return 0;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		ESP_LOGI(BLE_TAG, "advertise complete; reason=%d",
			    event->adv_complete.reason);
		bleprph_advertise();
		return 0;

	case BLE_GAP_EVENT_ENC_CHANGE:
		/* Encryption has been enabled or disabled for this connection. */
		ESP_LOGI(BLE_TAG, "encryption change event; status=%d ",
			    event->enc_change.status);
		rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
		assert(rc == 0);
		bleprph_print_conn_desc(&desc);
		ESP_LOGI(BLE_TAG, "\n");
		return 0;

	case BLE_GAP_EVENT_SUBSCRIBE:
		ESP_LOGI(BLE_TAG, "subscribe event; conn_handle=%d attr_handle=%d "
			    "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
			    event->subscribe.conn_handle,
			    event->subscribe.attr_handle,
			    event->subscribe.reason,
			    event->subscribe.prev_notify,
			    event->subscribe.cur_notify,
			    event->subscribe.prev_indicate,
			    event->subscribe.cur_indicate);
		return 0;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGI(BLE_TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
			    event->mtu.conn_handle,
			    event->mtu.channel_id,
			    event->mtu.value);
		return 0;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
	/* We already have a bond with the peer, but it is attempting to
	 * establish a new secure link.  This app sacrifices security for
	 * convenience: just throw away the old bond and accept the new link.
	 */

	/* Delete the old bond. */
		rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
		assert(rc == 0);
		ble_store_util_delete_peer(&desc.peer_id_addr);

	/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
	 * continue with the pairing operation.
	 */
		return BLE_GAP_REPEAT_PAIRING_RETRY;

	case BLE_GAP_EVENT_PASSKEY_ACTION:
		ESP_LOGI(BLE_TAG, "PASSKEY_ACTION_EVENT started \n");
/*
 *                struct ble_sm_io pkey = {0};
 *                int key = 0;
 *
 *                if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
 *                        pkey.action = event->passkey.params.action;
 *                        pkey.passkey = 123456; // This is the passkey to be entered on peer
 *                        ESP_LOGI(BLE_TAG, "Enter passkey %d on the peer side", pkey.passkey);
 *                        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
 *                        ESP_LOGI(BLE_TAG, "ble_sm_inject_io result: %d\n", rc);
 *                } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
 *                        ESP_LOGI(BLE_TAG, "Passkey on device's display: %d", event->passkey.params.numcmp);
 *                        ESP_LOGI(BLE_TAG, "Accept or reject the passkey through console in this format -> key Y or key N");
 *                        pkey.action = event->passkey.params.action;
 *                        if (scli_receive_key(&key)) {
 *                                pkey.numcmp_accept = key;
 *                        } else {
 *                                pkey.numcmp_accept = 0;
 *                                ESP_LOGE(BLE_TAG, "Timeout! Rejecting the key");
 *                        }
 *                        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
 *                        ESP_LOGI(BLE_TAG, "ble_sm_inject_io result: %d\n", rc);
 *                } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
 *                        static uint8_t tem_oob[16] = {0};
 *                        pkey.action = event->passkey.params.action;
 *                        for (int i = 0; i < 16; i++) {
 *                                pkey.oob[i] = tem_oob[i];
 *                        }
 *                        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
 *                        ESP_LOGI(BLE_TAG, "ble_sm_inject_io result: %d\n", rc);
 *                } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
 *                        ESP_LOGI(BLE_TAG, "Enter the passkey through console in this format-> key 123456");
 *                        pkey.action = event->passkey.params.action;
 *                        if (scli_receive_key(&key)) {
 *                                pkey.passkey = key;
 *                        } else {
 *                                pkey.passkey = 0;
 *                                ESP_LOGE(BLE_TAG, "Timeout! Passing 0 as the key");
 *                        }
 *                        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
 *                        ESP_LOGI(BLE_TAG, "ble_sm_inject_io result: %d\n", rc);
 *                }
 */
		return 0;
	}
	return 0;
}

static void bleprph_on_reset(int reason)
{
	ESP_LOGE(BLE_TAG, "Resetting state; reason=%d\n", reason);
}

static void bleprph_on_sync(void)
{
	int rc;

	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);

	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		ESP_LOGE(BLE_TAG, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* Printing ADDR */
	uint8_t addr_val[6] = {0};
	rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

	ESP_LOGI(BLE_TAG, "Device Address: ");
	print_addr(addr_val);
	ESP_LOGI(BLE_TAG, "\n");
	/* Begin advertising. */
	bleprph_advertise();
}

void bleprph_host_task(void *param)
{
	ESP_LOGI(BLE_TAG, "BLE Host Task Started");
	/* This function will return only when nimble_port_stop() is executed */
	nimble_port_run();

	nimble_port_freertos_deinit();
}

void delay_ms(u32 ms)
{
	vTaskDelay(ms/portTICK_PERIOD_MS);
}

void bme280_compensate(struct bme280_results *res, s32 ucmp_press,
					s32 ucmp_temp, s32 ucmp_hum)
{
	res->temp = (signed short int)(bme280_compensate_temperature_double(ucmp_temp) * 100);
	res->press = (unsigned int)(bme280_compensate_pressure_double(ucmp_press) * 10);
	res->hum = (unsigned short int)(bme280_compensate_humidity_double(ucmp_hum) *100);
}

s32 bme280_take_readings(struct bme280_results *res)
{
	s32 ucmp_press;
	s32 ucmp_temp;
	s32 ucmp_hum;
	s32 ret;

	ret = bme280_get_forced_uncomp_pressure_temperature_humidity(
				 &ucmp_press, &ucmp_temp, &ucmp_hum);

	if (ret == SUCCESS) {
		bme280_compensate(res, ucmp_press, ucmp_temp, ucmp_hum);
	} else {
		ESP_LOGE(TAG_BME280, "measure error: %d", ret);
	}

	return ret;
}

void bme280_force(struct bme280_results *res) {
	s32 ret;

	struct bme280_t bme280 = {
		.bus_write = i2c_write,
		.bus_read = i2c_read,
		.delay_msec = delay_ms,
		.dev_addr = BME280_I2C_ADDRESS2
	};


	ret = bme280_init(&bme280);

	ret += bme280_set_oversamp_pressure(BME280_OVERSAMP_1X);
	ret += bme280_set_oversamp_temperature(BME280_OVERSAMP_1X);
	ret += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

	ret += bme280_set_filter(BME280_FILTER_COEFF_OFF);
	if (ret == SUCCESS) {
		bme280_take_readings(res);
		ESP_LOGW(TAG_BME280, "%d degC / %d hPa / %d%%",
					res->temp, res->press, res->hum);
	} else {
		ESP_LOGE(TAG_BME280, "init or setting error: %d", ret);
	}
}

void task_measure(void *ignore) {
	struct veml6075_results uv_res;
	struct bme280_results tph_res;

	veml6075_force(&uv_res);
	bme280_force(&tph_res);
	if (xSemaphoreTake(measure_mutex, (TickType_t)20) == pdTRUE) {
		meas.uv_res = &uv_res;
		meas.tph_res = &tph_res;
		xSemaphoreGive(measure_mutex);
	}

	vTaskDelete(NULL);
}

void sleep_task(void *ignore)
{
	const int wakeup_time_sec = 120;
	const int wait_timeout_ms = 5000;
	const int wait_timeout_ticks = wait_timeout_ms / portTICK_PERIOD_MS;
	int ret;

	ESP_LOGE(TAG_DPSP, "start waiting");
	ret = xTaskNotifyWait(0, ULONG_MAX, NULL, wait_timeout_ticks);
	if (ret == pdPASS)
		ESP_LOGE(TAG_DPSP, "wake up from waiting, time to disable bluetooth and deep sleep");
	else
		ESP_LOGE(TAG_DPSP, "wake up from waiting, timeout passed");
	vTaskDelay(100/portTICK_PERIOD_MS);
	/* Stop nible host and disable the BT controller */
	ret = nimble_port_stop();
	if (ret == 0) {
		nimble_port_deinit();
		ret = esp_nimble_hci_and_controller_deinit();
		if (ret != ESP_OK) {
			ESP_LOGE(TAG_DPSP, "esp_nimble_hci_and_controller_deinit() failed with error: %d", ret);
		}
	}

	/* Go into deep sleep mode */
	esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);

	ESP_LOGE(TAG_DPSP, "Entering deep sleep");
	esp_deep_sleep_start();

	vTaskDelete(NULL);
}
void app_main(void)
{
	int rc;

	measure_mutex = xSemaphoreCreateMutex();
	xTaskCreate(&sleep_task, "sleep_task", 2048, NULL, 6, &sleep_task_h);
	/* Enable automatic light sleep */
	esp_pm_config_esp32_t pm_config = {
		.max_freq_mhz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ,
		.min_freq_mhz = 10,
		.light_sleep_enable = true
	};
	ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

	i2c_master_init();
	/* Start measuring */
	xTaskCreate(&task_measure, "task_measure", 2048, NULL, 6, NULL);

	/* Initialize NVS — it is used to store PHY calibration data */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ESP_ERROR_CHECK(nvs_flash_init());
	}

	ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
	esp_bt_sleep_enable();

	nimble_port_init();
	/* Initialize the NimBLE host configuration. */
	ble_hs_cfg.reset_cb = bleprph_on_reset;
	ble_hs_cfg.sync_cb = bleprph_on_sync;
	ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

	rc = gatt_svr_init();
	assert(rc == 0);

	/* Set the default device name. */
	rc = ble_svc_gap_device_name_set("env-#1");
	assert(rc == 0);

	/* XXX Need to have template for store */
	ble_store_config_init();

	nimble_port_freertos_init(bleprph_host_task);

}
