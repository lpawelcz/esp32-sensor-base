
/* Register addresses */

#define UV_CONF_REG	0x0
#define UV_A_DATA	0x7
#define UV_B_DATA	0x9
#define UVCOMP1_DATA	0xA
#define UVCOMP2_DATA	0xB
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
