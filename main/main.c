#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "sdkconfig.h" // generated by "make menuconfig"

#include "bme280.h"
#include "veml6075.h"

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
			(HD_ON << HD_POS)		|
			(UV_TRIG << UV_TRIG_POS)	|
			(UV_AF << UV_AF_POS)		|
			(UV_ON << UV_ON_POS);
	reg_data[1] = 0;

	ret = i2c_write(UV_SLAVE_ADDR, UV_CONF_REG, reg_data, 2);

	return ret;
}

int veml6075_get_uv(u8 *uv_a_data, u8 *uv_b_data)
{
	int ret;

	ret = i2c_read(UV_SLAVE_ADDR, UV_A_DATA, uv_a_data, 2);
	if (ret < 0)
		return ret;
	ret = i2c_read(UV_SLAVE_ADDR, UV_B_DATA, uv_b_data, 2);

	return ret;
}

void veml6075_force() {
	int ret;
	u8 uv_a_data[2], uv_b_data[2];

	ret = veml6075_get_uv(uv_a_data, uv_b_data);

	if (ret == SUCCESS)
		ESP_LOGW(TAG_VEML6075, "0x%02x%02x UVA / 0x%02x%02x UVB ",
		  uv_a_data[1], uv_a_data[0], uv_b_data[1], uv_b_data[0]);
	else
		ESP_LOGE(TAG_VEML6075, "measure error. code: %d", ret);

	ret = veml6075_measure_uv();
	if (ret != SUCCESS)
		ESP_LOGE(TAG_VEML6075, "Write error %d", ret);
}

void BME280_delay_msek(u32 msek)
{
	vTaskDelay(msek/portTICK_PERIOD_MS);
}

void bme280_compensate(double *temp_degc, double *press_hpa, double *hum_perc,
				  s32 ucmp_press, s32 ucmp_temp, s32 ucmp_hum)
{
	*temp_degc = bme280_compensate_temperature_double(ucmp_temp);
	*press_hpa = bme280_compensate_pressure_double(ucmp_press) / 100;
	*hum_perc = bme280_compensate_humidity_double(ucmp_hum);
}

s32 bme280_take_readings(double *temp_degc, double *press_hpa, double *hum_perc)
{
	s32 ucmp_press;
	s32 ucmp_temp;
	s32 ucmp_hum;
	s32 ret;

	ret = bme280_get_forced_uncomp_pressure_temperature_humidity(
				 &ucmp_press, &ucmp_temp, &ucmp_hum);

	if (ret == SUCCESS) {
		bme280_compensate(temp_degc, press_hpa, hum_perc,
				 ucmp_press, ucmp_temp, ucmp_hum);
	} else {
		ESP_LOGE(TAG_BME280, "measure error. code: %d", ret);
	}

	return ret;
}

void bme280_force() {
	double temp_degc, press_hpa, hum_perc;
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
		bme280_take_readings(&temp_degc, &press_hpa, &hum_perc);
		ESP_LOGW(TAG_BME280, "%.2f degC / %.3f hPa / %.3f %%",
					temp_degc, press_hpa, hum_perc);
	} else {
		ESP_LOGE(TAG_BME280, "init or setting error. code: %d", ret);
	}
}
void task_measure(void *ignore) {
	const int wakeup_time_sec = 5;
	int sleep_time_ms;
	struct timeval now;

	veml6075_force();
	bme280_force();

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
	/*xTaskCreate(&task_bme280_forced_mode, "bme280_forced_mode",  2048, NULL, 6, NULL);*/
	xTaskCreate(&task_measure, "veml6075_forced_mode",  2048, NULL, 6, NULL);
}
