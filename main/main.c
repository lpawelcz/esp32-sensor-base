#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "sdkconfig.h" // generated by "make menuconfig"

#include "bme280.h"
#include "veml6075.h"
#include "ble.h"

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22

#define TAG_BME280 "BME280"
#define TAG_VEML6075 "VEML6075"
#define TAG_DPSP "DEEP SLEEP"

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

#include <sys/time.h>
#include "esp_sleep.h"

static RTC_DATA_ATTR struct timeval sleep_enter_time;

static uint8_t adv_raw_data[30] = {0x02, 0x01, 0x06, 0x1A, 0xFF, 0x4C, 0x00,
				   0x02, 0x15, 0xFD, 0xA5, 0x06, 0x93, 0xA4,
				   0xE2, 0x4F, 0xB1, 0xAF, 0xCF, 0xC6, 0xEB,
				   0x07, 0x64, 0x78, 0x25, 0x00, 0x00, 0x00,
								 0x00, 0xC5};

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

void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

s8 i2c_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;

	esp_err_t espRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write(cmd, reg_data, cnt, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = FAIL;
	}
	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

s8 i2c_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

	if (cnt > 1) {
		i2c_master_read(cmd, reg_data, cnt-1, I2C_MASTER_ACK);
	}
	i2c_master_read_byte(cmd, reg_data+cnt-1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);
	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = FAIL;
	}

	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

int scan_i2c(u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;
	esp_err_t espRc;
	int i;
	for(i = 0; i < 128; i++) {
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();

		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);

		i2c_master_write_byte(cmd, i, true);
		i2c_master_write(cmd, reg_data, cnt, true);
		i2c_master_stop(cmd);

		espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100);
		if (espRc == ESP_OK) {
			iError = SUCCESS;
			break;
		} else {
			iError = FAIL;
		}
		ESP_LOGW(TAG_VEML6075, "espRc:%d iError: %d", espRc, iError);
		i2c_cmd_link_delete(cmd);
	}
		ESP_LOGW(TAG_VEML6075, "addr: %d", i);
	return i;
}

int veml6075_measure_uv()
{
	u8 reg_data[2];
	int ret;
	reg_data[0] =	(0 << 7)			|
			(UV_IT_100_MS << UV_IT_POS)	|
			(HD_OFF << HD_POS)		|
			(UV_TRIG << UV_TRIG_POS)	|
			(UV_AF << UV_AF_POS)		|
			(UV_ON << UV_ON_POS);
	reg_data[1] = 0;

	ret = i2c_write(UV_SLAVE_ADDR, UV_CONF, reg_data, 2);
	if (ret != SUCCESS)
		ESP_LOGE(TAG_VEML6075, "Write error %d", ret);

	return ret;
}

int veml6075_get_uv(struct raw_results *res)
{
	int ret;

	ret = i2c_read(UV_SLAVE_ADDR, UV_A_DATA, res->uv_a, 2);
	if (ret < 0){
		return ret;
		ESP_LOGE(TAG_VEML6075, "read UV_A_DATA error: %d", ret);
	}
	ret = i2c_read(UV_SLAVE_ADDR, UV_D_DATA, res->uv_d, 2);
	if (ret < 0){
		return ret;
		ESP_LOGE(TAG_VEML6075, "read UV_D_DATA error: %d", ret);
	}
	ret = i2c_read(UV_SLAVE_ADDR, UV_B_DATA, res->uv_b, 2);
	if (ret < 0){
		return ret;
		ESP_LOGE(TAG_VEML6075, "read UV_B_DATA error: %d", ret);
	}
	ret = i2c_read(UV_SLAVE_ADDR, UV_COMP1_DATA, res->uv_comp1, 2);
	if (ret < 0){
		return ret;
		ESP_LOGE(TAG_VEML6075, "read UV_COMP1_DATA error: %d", ret);
	}
	ret = i2c_read(UV_SLAVE_ADDR, UV_COMP2_DATA, res->uv_comp2, 2);
	if (ret == SUCCESS) {
		ESP_LOGW(TAG_VEML6075, "0x%02x%02x UVA | 0x%02x%02x UVB",
		  res->uv_a[1], res->uv_a[0], res->uv_b[1], res->uv_b[0]);
		ESP_LOGW(TAG_VEML6075, "0x%02x%02x UVD",
					res->uv_d[1], res->uv_d[0]);
		ESP_LOGW(TAG_VEML6075, "0x%02x%02x UV_COMP1 | 0x%02x%02x UV_COMP2",
					res->uv_comp1[1], res->uv_comp1[0],
					res->uv_comp2[1], res->uv_comp2[0]);
	} else {
		ESP_LOGE(TAG_VEML6075, "read UV_COMP2_DATA error: %d", ret);
	}

	return ret;
}
static inline u16 veml6075_cast_to_u16(u8 *data)
{
	u16 casted;
	casted = ((u16) data[1]) << HIGH_BYTE;
	casted += ((u16) data[0]) << LOW_BYTE;

	return casted;
}

static inline void veml6075_cast_results(struct raw_results *raw_res,
			   struct conv_results *conv_res)
{
	conv_res->uv_a = veml6075_cast_to_u16(raw_res->uv_a);
	conv_res->uv_d = veml6075_cast_to_u16(raw_res->uv_d);
	conv_res->uv_b = veml6075_cast_to_u16(raw_res->uv_b);
	conv_res->uv_comp1 = veml6075_cast_to_u16(raw_res->uv_comp1);
	conv_res->uv_comp2 = veml6075_cast_to_u16(raw_res->uv_comp2);
}

static inline float veml6075_comp_uv_a(struct conv_results *res)
{
	return (res->uv_a - res->uv_d) -
	       (A_COEF * (res->uv_comp1 - res->uv_d)) -
	       (B_COEF * (res->uv_comp2 - res->uv_d));
}

static inline float veml6075_comp_uv_b(struct conv_results *res)
{
	return (res->uv_b - res->uv_d) -
	       (C_COEF * (res->uv_comp1 - res->uv_d)) -
	       (D_COEF * (res->uv_comp2 - res->uv_d));
}

static inline float veml6075_calc_uv_index(float comp_uv_a, float comp_uv_b)
{
	return ((comp_uv_b * UV_B_RESP) + (comp_uv_a * UV_A_RESP)) / 2;
}

static inline float veml6075_get_uv_index(struct raw_results *raw_res,
					   struct veml6075_results *res)
{
	struct conv_results conv_res;

	veml6075_cast_results(raw_res, &conv_res);

	res->uv_a = veml6075_comp_uv_a(&conv_res);
	res->uv_b = veml6075_comp_uv_b(&conv_res);
	res->uv_i = veml6075_calc_uv_index(res->uv_a, res->uv_b);

	return res->uv_i;
}

int veml6075_force(struct veml6075_results *res) {
	struct raw_results raw_res;
	int ret;
	float uv_index;

	ret = veml6075_get_uv(&raw_res);
	if (ret < 0)
		return ret;
	uv_index = veml6075_get_uv_index(&raw_res, res);
	ESP_LOGW(TAG_VEML6075, "UV INDEX: %lf", uv_index);

	ret = veml6075_measure_uv();

	return ret;
}

void BME280_delay_msek(u32 msek)
{
	vTaskDelay(msek/portTICK_PERIOD_MS);
}

void bme280_compensate(struct bme280_results *res, s32 ucmp_press,
					s32 ucmp_temp, s32 ucmp_hum)
{
	res->temp = (float)bme280_compensate_temperature_double(ucmp_temp);
	res->press = (float)bme280_compensate_pressure_double(ucmp_press) / 100;
	res->hum = (float)bme280_compensate_humidity_double(ucmp_hum);
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
		ESP_LOGE(TAG_BME280, "measure error. code: %d", ret);
	}

	return ret;
}

void bme280_force(struct bme280_results *res) {
	s32 ret;

	struct bme280_t bme280 = {
		.bus_write = i2c_write,
		.bus_read = i2c_read,
		.dev_addr = BME280_I2C_ADDRESS2,
		.delay_msec = BME280_delay_msek
	};


	ret = bme280_init(&bme280);

	ret += bme280_set_oversamp_pressure(BME280_OVERSAMP_1X);
	ret += bme280_set_oversamp_temperature(BME280_OVERSAMP_1X);
	ret += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

	ret += bme280_set_filter(BME280_FILTER_COEFF_OFF);
	if (ret == SUCCESS) {
		bme280_take_readings(res);
		ESP_LOGW(TAG_BME280, "%.2f degC / %.3f hPa / %.3f %%",
					res->temp, res->press, res->hum);
	} else {
		ESP_LOGE(TAG_BME280, "init or setting error. code: %d", ret);
	}
}

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

static inline void float2bytes(float float_val, u8 *arr)
{
	u32 int_val, i;

	int_val = *((int*)&float_val);

	for (i = 0; i < 4; ++i) {
		arr[i] = (int_val >> (8 * i)) & 0xFF;
	}
}

/*int ble_prepare_binary_measurements(struct measurements *meas)*/
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
	float2bytes(meas->uv_res.uv_a, &adv[i]);
	i += 4;
	float2bytes(meas->uv_res.uv_b, &adv[i]);
	i += 4;
	float2bytes(meas->uv_res.uv_i, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res.temp, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res.press, &adv[i]);
	i += 4;
	float2bytes(meas->tph_res.hum, &adv[i]);
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

void task_measure(void *ignore) {
	const int wakeup_time_sec = 5;
	const u8 bytes_cnt = sizeof(struct veml6075_results) +
			     sizeof(struct bme280_results) + 7;
	int sleep_time_ms;
	struct timeval now;
	struct veml6075_results uv_res;
	struct bme280_results tph_res;
	u8 adv[bytes_cnt];

	veml6075_force(&uv_res);
	bme280_force(&tph_res);
	struct measurements meas = {
		.uv_res = uv_res,
		.tph_res = tph_res
	};

	ble_prepare_binary_measurements(&meas, adv, bytes_cnt);
	ble_transmit_measurements(adv, bytes_cnt);
	BME280_delay_msek(50);
	gettimeofday(&now, NULL);
	sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 +
			    (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

	switch (esp_sleep_get_wakeup_cause()) {
	case ESP_SLEEP_WAKEUP_TIMER: {
		ESP_LOGI(TAG_DPSP,
			"Wake up from timer. Time spent in deep sleep: %dms",
								sleep_time_ms);
		break;
	}
	case ESP_SLEEP_WAKEUP_UNDEFINED:
	default:
		ESP_LOGI(TAG_DPSP, "Not a deep sleep reset");
	}

	ESP_LOGI(TAG_DPSP, "Enabling timer wakeup, %ds", wakeup_time_sec);
	esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
	gettimeofday(&sleep_enter_time, NULL);
	ESP_LOGI(TAG_DPSP, "Entering deep sleep");

	esp_deep_sleep_start();

	vTaskDelete(NULL);
}
void app_main(void)
{
	i2c_master_init();
	/*
	 *xTaskCreate(&task_bme280_forced_mode, "bme280_forced_mode",
	 *                                        2048, NULL, 6, NULL);
	 */
	xTaskCreate(&task_measure, "task_measure", 2048, NULL, 6, NULL);
}
