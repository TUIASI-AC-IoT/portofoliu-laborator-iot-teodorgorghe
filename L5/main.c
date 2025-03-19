/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
#include "config.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "soft-ap.h"
#include "http-server.h"

#include "../mdns/include/mdns.h"

static const char *TAG = "scan";

void ssid_scan_task() {
  char* ssid_list = get_ssid_list();
  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  uint16_t number = CONFIG_EXAMPLE_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[CONFIG_EXAMPLE_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_scan_start(NULL, true);
  ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  
  set_size_ssid(ap_count);
  ESP_LOGI(TAG, "Total APs scanned = %u %u", ap_count, number);

  for (int i = 0; (i < CONFIG_EXAMPLE_SCAN_LIST_SIZE) && (i < ap_count); i++) {
    memcpy(ssid_list + 33 * i, ap_info[i].ssid, sizeof(ap_info[i].ssid));
    ESP_LOGI(TAG, "SSID %d: %s", i, ap_info[i].ssid);
  }
}

void app_main(void)
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // TODO: 3. SSID scanning in STA mode
  ssid_scan_task();

  // TODO: 1. Start the softAP mode
  wifi_init_softap();

  // TODO: 4. mDNS init (if there is time left)

  // TODO: 2. Start the web server
  start_webserver();
}