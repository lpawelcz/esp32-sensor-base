#include "ble.h"


int kill_ble(void)
{
	int ret;

	ret = nimble_port_stop();
	if (ret == 0) {
		nimble_port_deinit();
		ret = esp_nimble_hci_and_controller_deinit();
		if (ret != ESP_OK) {
			ESP_LOGE(BLE_TAG, "esp_nimble_hci_and_controller_deinit() failed with error: %d", ret);
		}
	}
	return ret;
}
