#ifndef _VEML6075_H
#define _VEML6075_H

#include "common.h"
#include "i2c.h"

struct raw_results {
	u8 uv_a[2];
	u8 uv_d[2];
	u8 uv_b[2];
	u8 uv_comp1[2];
	u8 uv_comp2[2];
};

struct conv_results {
	u16 uv_a;
	u16 uv_d;
	u16 uv_b;
	u16 uv_comp1;
	u16 uv_comp2;
};

struct veml6075_results {
	float uv_a;
	float uv_b;
	unsigned char uv_i;
};

struct bme280_results {
	signed short int temp;
	unsigned int press;
	unsigned short int hum;
};

#define TAG_VEML6075 "VEML6075"

/* Coefficients for UV index calculation */
#define A_COEF		2.22
#define B_COEF		1.33
#define C_COEF		2.95
#define D_COEF		1.74
#define UV_A_RESP	0.001461
#define UV_B_RESP	0.002591

/* Register addresses */

#define UV_CONF		0x0
#define UV_A_DATA	0x7
#define UV_D_DATA	0x8
#define UV_B_DATA	0x9
#define UV_COMP1_DATA	0xA
#define UV_COMP2_DATA	0xB
#define ID		0xC

/* Configuration values */

#define UV_SLAVE_ADDR	0x10

#define UV_IT_50_MS	0x0
#define UV_IT_100_MS	0x1
#define UV_IT_200_MS	0x2
#define UV_IT_400_MS	0x3
#define UV_IT_800_MS	0x4

#define HD_ON		0x1
#define HD_OFF		0x0

#define UV_TRIG		0x1
#define UV_NO_TRIG	0x0

#define UV_AF		0x1
#define UV_NO_AF	0x0

#define UV_ON		0x0
#define UV_OFF		0x1

/* Configuration values position in register */

#define UV_IT_POS	0x4
#define HD_POS		0x3
#define UV_TRIG_POS	0x2
#define UV_AF_POS	0x1
#define UV_ON_POS	0x0

int veml6075_measure_uv();
int veml6075_get_uv(struct raw_results *res);
int veml6075_force(struct veml6075_results *res);

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
	float uv_i;

	veml6075_cast_results(raw_res, &conv_res);

	res->uv_a = veml6075_comp_uv_a(&conv_res);
	res->uv_b = veml6075_comp_uv_b(&conv_res);
	uv_i = veml6075_calc_uv_index(res->uv_a, res->uv_b);
	res->uv_i = (unsigned char)uv_i;

	return uv_i;
}

#endif
/* ifndef _VEML6075_H */
