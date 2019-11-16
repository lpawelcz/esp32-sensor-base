#ifndef _BLE_H
#define _BLE_H

#include "common.h"

/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

#define BLE_TAG "BLE_PRPH"

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;
void ble_store_config_init(void);

int kill_ble(void);

#endif
/* ifndef _BLE_H */
