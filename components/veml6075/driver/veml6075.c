#include "veml6075.h"

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
	if (ret != 0)
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
	if (ret == 0) {
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
