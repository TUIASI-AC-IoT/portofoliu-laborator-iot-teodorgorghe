#ifndef _SOFT_AP_H_
#define _SOFT_AP_H_

#define EXAMPLE_ESP_WIFI_SSID      "ESP32-prov-gorghe"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL   9
#define EXAMPLE_MAX_STA_CONN       4

void wifi_init_softap(bool netif_init);

#endif