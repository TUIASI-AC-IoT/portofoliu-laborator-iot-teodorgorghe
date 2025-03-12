/* WiFi station Example

	 This example code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <driver/gpio.h>

#define GPIO_OUTPUT_IO 4
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO)

#define CONFIG_ESP_WIFI_SSID "lab-iot"
#define CONFIG_ESP_WIFI_PASS "IoT-IoT-IoT"
#define CONFIG_ESP_MAXIMUM_RETRY 5
#define CONFIG_LOCAL_PORT 10001

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

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

bool wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
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
					.ssid = CONFIG_ESP_WIFI_SSID,
					.password = CONFIG_ESP_WIFI_PASS,
			},
	};
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
						 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASS);
		return true;
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
						 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASS);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	return false;
}

void udp_send_led(struct ip4_addr *dest_ip, int port)
{
	static bool toggle = false;
	char payload[8] = "GPIO4=0";
  if (toggle == 1) {
  	payload[6] = '1';
  }

	// Init socket
	int addr_family = 0;
  int ip_protocol = 0;
	struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = dest_ip->addr;
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);
  addr_family = AF_INET;
  ip_protocol = IPPROTO_IP;

	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
	if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    return;
  }
	ESP_LOGI(TAG, "Socket created");

	struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

	int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
  	ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
  } else {
		ESP_LOGI(TAG, "Message sent");
		toggle = !toggle;
	}

	shutdown(sock, 0);
	close(sock);
}

void start_mdns_service()
{
	// initialize mDNS service
	esp_err_t err = mdns_init();
	if (err)
	{
		printf("MDNS Init failed: %d\n", err);
		return;
	}

	// set hostname
	mdns_hostname_set("esp32-gorghe");
	// set default instance
	mdns_instance_name_set("ESP32 Thing");
}

void add_mdns_services()
{
	mdns_service_add(NULL, "_control_led", "_udp", CONFIG_LOCAL_PORT, NULL, 0);
	mdns_service_instance_name_set("_control_led", "_udp", "Dummy esp32 service description");
}

struct ip4_addr resolve_mdns_host(const char *host_name)
{
	ESP_LOGI(TAG, "Query A: %s.local", host_name);

	struct ip4_addr addr;
	addr.addr = 0;

	int err = mdns_query_a(host_name, 2000, (esp_ip4_addr_t *)&addr);
	if (err)
	{
		if (err == ESP_ERR_NOT_FOUND)
		{
			ESP_LOGI(TAG, "Host was not found!");
			return addr;
		}
		ESP_LOGI(TAG, "Query Failed");
		return addr;
	}

	return addr;
}

size_t mdns_result_len(mdns_result_t *results)
{
	size_t l = 0;
	mdns_result_t *r = results;
	while (r)
	{
		l++;
		r = r->next;
	}
	return l;
}

void mdns_select_and_send(mdns_result_t *results, int i)
{
	mdns_result_t *r = results;
	mdns_ip_addr_t *a = NULL;
	size_t l = 0;
	while (r)
	{
		if (l == i)
		{
			a = r->addr;
			while (a)
			{
				if (a->addr.type == ESP_IPADDR_TYPE_V4)
				{
					// It should send here to the another board
					udp_send_led((struct ip4_addr *)&a->addr.u_addr.ip4, r->port);
					return;
				}
				a = a->next;
			}
			ESP_LOGI(TAG, "No IPV4 on that service!");
			return;
		}
		l++;
		r = r->next;
	}
}

static void mdns_result_rand_send(mdns_result_t *results)
{
	size_t length = mdns_result_len(results);

	int rand_number = rand() % length;
	mdns_select_and_send(results, rand_number);
}

static void mdns_find_and_send_service(const char* service_name, const char* instance_name)
{
	mdns_result_t *results = NULL;
	ESP_LOGI(TAG, "Querying service %s.%s.local", service_name, instance_name);
	int err = mdns_query_ptr(service_name, instance_name, 5000, 10, &results);
	if (err)
	{
		ESP_LOGI(TAG, "Query Failed");
		return;
	}

	mdns_result_rand_send(results);
	mdns_query_results_free(results);
}

static void mdns_task(void *pvParameters)
{
	start_mdns_service();
	add_mdns_services();

	while (1)
	{
		mdns_find_and_send_service("_control_led", "_udp");
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

static void udp_task(void *pvParameters)
{
	char rx_buffer[128];
	char addr_str[128];
	int addr_family = 0;
	int ip_protocol = 0;

	struct sockaddr_in local_addr;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(CONFIG_LOCAL_PORT);
	ip_protocol = IPPROTO_IP;
	addr_family = AF_INET;

	while (1)
	{
		int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
		if (sock < 0)
		{
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket created");

		int err = bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
		if (err < 0)
		{
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		}
		ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_LOCAL_PORT);

		while (1)
		{
			struct sockaddr source_addr;
			socklen_t socklen = sizeof(source_addr);
			int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, &source_addr, &socklen);

			// Error occurred during receiving
			if (len < 0)
			{
				ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
				break;
			}
			// Data received
			else
			{
				inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
				rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
				// ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
				// ESP_LOGI(TAG, "%s", rx_buffer);
				if (strcmp(rx_buffer, "GPIO4=1") == 0)
				{
					gpio_set_level(GPIO_OUTPUT_IO, 0);
				}
				else if (strcmp(rx_buffer, "GPIO4=0") == 0)
				{
					gpio_set_level(GPIO_OUTPUT_IO, 1);
				}
				else
				{
					ESP_LOGI(TAG, "Invalid message %s", rx_buffer);
				}
			}

			vTaskDelay(200 / portTICK_PERIOD_MS);
		}

		if (sock != -1)
		{
			ESP_LOGE(TAG, "Shutting down socket and restarting...");
			shutdown(sock, 0);
			close(sock);
		}
	}
	vTaskDelete(NULL);
}

void app_main(void)
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	bool connected = wifi_init_sta();

	// zero-initialize the config structure.
	gpio_config_t io_conf = {};
	// disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	// set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	// bit mask of the pins that you want to set
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	// disable pull-down mode
	io_conf.pull_down_en = 0;
	// disable pull-up mode
	io_conf.pull_up_en = 0;
	// configure GPIO with the given settings
	gpio_config(&io_conf);

	if (connected)
	{
		xTaskCreate(udp_task, "udp_task", 4096, NULL, 5, NULL);
		xTaskCreate(mdns_task, "mdns_task", 4096, NULL, 5, NULL);
	}
}