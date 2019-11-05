/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"
#include "common.h"
#include "veml6075.h"

#define BLE_TAG "BLE_PRPH"

static signed short int temp;
static unsigned short int hum;
static unsigned int press;
static unsigned char uvi;

/* Environmental Sensing Service (ESS) */
static const ble_uuid128_t gatt_svr_ess_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5f, 0x80, 0x00, 0x00, 0x80 ,0x00, 0x10,
					     0x00, 0x00, 0x1A, 0x18, 0x00, 0x00);

/* Temperature characteristic */
static const ble_uuid128_t gatt_svr_ess_temp_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5f, 0x80, 0x00, 0x00, 0x80 ,0x00, 0x10,
					     0x00, 0x00, 0x6E, 0x2A, 0x00, 0x00);

/* Humidity characteristic */
static const ble_uuid128_t gatt_svr_ess_hum_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5f, 0x80, 0x00, 0x00, 0x80 ,0x00, 0x10,
					     0x00, 0x00, 0x6F, 0x2A, 0x00, 0x00);

/* Pressure characteristic */
static const ble_uuid128_t gatt_svr_ess_press_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5f, 0x80, 0x00, 0x00, 0x80 ,0x00, 0x10,
					     0x00, 0x00, 0x6D, 0x2A, 0x00, 0x00);

/* UV index characteristic */
static const ble_uuid128_t gatt_svr_ess_uvi_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5f, 0x80, 0x00, 0x00, 0x80 ,0x00, 0x10,
					     0x00, 0x00, 0x76, 0x2A, 0x00, 0x00);

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Environmental Sensiong Service (ESS) */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
	.uuid = &gatt_svr_ess_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
	{ {
		/* Temperature characteristic */
		.uuid = &gatt_svr_ess_temp_uuid.u,
		.access_cb = gatt_svr_chr_access_sec_test,
		.flags = BLE_GATT_CHR_F_READ,
	    }, {
		/* Humidity characteristic */
		.uuid = &gatt_svr_ess_hum_uuid.u,
		.access_cb = gatt_svr_chr_access_sec_test,
		.flags = BLE_GATT_CHR_F_READ,
	    }, {
		/* Pressure characteristic */
		.uuid = &gatt_svr_ess_press_uuid.u,
		.access_cb = gatt_svr_chr_access_sec_test,
		.flags = BLE_GATT_CHR_F_READ,
	    }, {
		/* UV index characteristic */
		.uuid = &gatt_svr_ess_uvi_uuid.u,
		.access_cb = gatt_svr_chr_access_sec_test,
		.flags = BLE_GATT_CHR_F_READ,
	    }, {
		0, /* No more characteristics in this service. */
	    }
	},
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_ess_temp_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a signed 16-bit temperature in degrees Clesius */
        temp = meas.tph_res->temp;
	/*rand() % (4020 - (-4100) + 1) - 4100;*/
        rc = os_mbuf_append(ctxt->om, &temp, sizeof temp);

        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
	if (ble_uuid_cmp(uuid, &gatt_svr_ess_hum_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a unsigned 16-bit humidity in percent */
        /*hum = rand() % 10001;*/
        hum = meas.tph_res->hum;
        rc = os_mbuf_append(ctxt->om, &hum, sizeof hum);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
	if (ble_uuid_cmp(uuid, &gatt_svr_ess_press_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a unsigned 32-bit pressure in Pa */
        /*press = rand() % (1086000 - 965200 + 1) + 965200;*/
        press = meas.tph_res->press;
        rc = os_mbuf_append(ctxt->om, &press, sizeof press);

        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
	if (ble_uuid_cmp(uuid, &gatt_svr_ess_uvi_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a unsigned 8-bit UV index */
	/*uvi = rand() % 12;*/
        uvi = meas.uv_res->uv_i;
        rc = os_mbuf_append(ctxt->om, &uvi, sizeof uvi);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(BLE_TAG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(BLE_TAG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(BLE_TAG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
