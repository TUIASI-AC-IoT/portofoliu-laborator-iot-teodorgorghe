#include "sta-ap.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "config.h"
#include <string.h>
#include "esp_log.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

char ssid[33];
char password[64];

void sta_set_ssid(const char *new_ssid)
{
	strcpy(ssid, new_ssid);
}

void sta_set_password(const char *pass)
{
	strcpy(password, pass);
}

static void event_handler(void *arg, esp_event_base_t event_base,
													int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY)
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta(void)
{
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	s_wifi_event_group = xEventGroupCreate();

	if (strlen(ssid) == 0)
	{
		ESP_LOGE(TAG, "SSID not set");
		return;
	}

	if (strlen(password) == 0)
	{
		ESP_LOGE(TAG, "Password not set");
		return;
	}

	ESP_ERROR_CHECK(esp_netif_init());
	
  // Deinitialize the existing network interface if it exists
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif != NULL) {
		esp_netif_destroy(netif);
  }

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
																											ESP_EVENT_ANY_ID,
																											&event_handler,
																											NULL,
																											&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
																											IP_EVENT_STA_GOT_IP,
																											&event_handler,
																											NULL,
																											&instance_got_ip));

	wifi_config_t wifi_config = {
			.sta = {
					.ssid = "",
					.password = "",
			},
	};

	strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
																				 WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
																				 pdFALSE,
																				 pdFALSE,
																				 portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
						 ssid, password);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
						 ssid, password);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}