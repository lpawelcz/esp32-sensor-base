#ifndef _COMMON_H
#define _COMMON_H

#define HIGH_BYTE	0x8
#define LOW_BYTE	0x0

typedef unsigned char u8;
typedef unsigned short u16;

/* Forward declaration from: */
struct veml6075_results; 	/* veml6075.h */
struct bme280_results;		/* bme280.h */

struct measurements {
	struct veml6075_results *uv_res;
	struct bme280_results *tph_res;
};

#endif
/* ifndef _COMMON_H */
