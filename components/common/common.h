#ifndef _COMMON_H
#define _COMMON_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include <sys/time.h>

#define TAG_DPSP "DEEP SLEEP"
#define HIGH_BYTE	0x8
#define LOW_BYTE	0x0

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

/* Forward declaration from: */
struct veml6075_results; 	/* veml6075.h */
struct bme280_results;		/* bme280.h */

struct measurements {
	struct veml6075_results *uv_res;
	struct bme280_results *tph_res;
} meas;

#endif
/* ifndef _COMMON_H */
