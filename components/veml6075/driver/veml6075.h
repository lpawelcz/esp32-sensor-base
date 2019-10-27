typedef unsigned char u8;

struct u8_results {
	u8 uv_a[2];
	u8 uv_d[2];
	u8 uv_b[2];
	u8 uv_comp1[2];
	u8 uv_comp2[2];
};

struct u16_results {
	u16 uv_a;
	u16 uv_d;
	u16 uv_b;
	u16 uv_comp1;
	u16 uv_comp2;
};

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

#define HIGH_BYTE	0x8
#define LOW_BYTE	0x0
