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
#include "iot_button.h"
#include "button_gpio.h"
#include "config.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "soft-ap.h"
#include "sta-ap.h"
#include "http-server.h"

#include "../mdns/include/mdns.h"

static const char *TAG_SCAN = "scan";
static const char *TAG = "main";

void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGI(TAG, "MDNS Init failed: %d\n", err);
        return;
    }
    ESP_LOGI(TAG, "MDNS Init done");

    //set hostname
    mdns_hostname_set("setup-gorghe");
    //set default instance
    mdns_instance_name_set("ESP32 Provisioning");
}

static void mdns_task(void *pvParameters)
{
    start_mdns_service();

    vTaskDelete(NULL);
}

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
  ESP_LOGI(TAG_SCAN, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  
  set_size_ssid(ap_count);
  ESP_LOGI(TAG_SCAN, "Total APs scanned = %u %u", ap_count, number);

  for (int i = 0; (i < CONFIG_EXAMPLE_SCAN_LIST_SIZE) && (i < ap_count); i++) {
    memcpy(ssid_list + 33 * i, ap_info[i].ssid, sizeof(ap_info[i].ssid));
    ESP_LOGI(TAG_SCAN, "SSID %d: %s", i, ap_info[i].ssid);
  }
}

void btn_long_press_cb(void* arg, void* user_data)
{
  // Reset the WiFi configuration (NVS)
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (ret == ESP_OK) {
    ret = nvs_erase_key(nvs_handle, "ssid");
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SSID erase failed %d", ret);
    }

    ret = nvs_erase_key(nvs_handle, "password");
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Password erase failed %d", ret);
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Commit erase failed %d", ret);
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "NVS erase done. Restarting...");
    
    esp_restart();
  } else {
    ESP_LOGE(TAG, "NVS open failed %d", ret);
  }
}

void init_button()
{
  button_config_t btn_cfg = {0};
  button_gpio_config_t gpio_cfg = {
    .gpio_num = 2,
    .active_level = 0
  };
  button_handle_t btn;
  esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);
  assert(ret == ESP_OK);

  button_event_args_t args = {
    .long_press.press_time = 5000,
  };

  iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, &args, btn_long_press_cb, NULL);
  ESP_LOGI(TAG, "Button init done");
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

  // Read the configuration from NVS
  nvs_handle_t nvs_handle;
  ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (ret == ESP_OK) {
    bool flag = true;
    char ssid[33];
    char password[65];

    size_t length = 33;
    ret = nvs_get_str(nvs_handle, "ssid", NULL, &length);
    if (ret != ESP_OK)
    {
      flag = false;
      ESP_LOGI(TAG, "SSID not found %d", ret);
    }

    ssid[length - 1] = '\0';

    length = 65;
    ret = nvs_get_str(nvs_handle, "password", password, &length);
    if (ret != ESP_OK) {
      flag = false;
      ESP_LOGI(TAG, "Password not found %d", ret);
    }
    password[length - 1] = '\0';

    if (flag) {
      init_button();
      // Init the WiFi in STA mode
      sta_set_ssid(ssid);
      sta_set_password(password);
      wifi_init_sta();
    } else {
      wifi_init_softap(true);
    }
  } else {
    ESP_LOGI(TAG, "NVS not found %d", ret);
    wifi_init_softap(true);
  }

  // TODO: 4. mDNS init (if there is time left)
  xTaskCreate(&mdns_task, "mdns_task", 4096, NULL, 5, NULL);

  // TODO: 2. Start the web server
  start_webserver();
}