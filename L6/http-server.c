#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"
#include "index_html.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/event_groups.h"

#include "esp_http_server.h"

static const char *TAG = "wifi station";

char ssid_list[CONFIG_EXAMPLE_SCAN_LIST_SIZE][33];
size_t size_ssid = 0;

char* get_ssid_list() {
    return (char*) ssid_list;
}

void set_size_ssid(size_t size) {
    size_ssid = size;
}

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */

    char buffer[2048];
    set_index_html_content(buffer, get_ssid_list(), size_ssid);
    httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[1024];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }
    // Null-terminate the buffer
    content[ret] = '\0';

    // Get the SSID and password from the POST request
    char ssid[33];
    char password[65];
    
    char* token;
	token  = strtok(content, "&");
    if (token == NULL) {
        return ESP_FAIL;
    }

	sscanf(token, "ssid=%s", ssid);
	token = strtok(NULL, "&");

    if (token == NULL) {
        return ESP_FAIL;
    }

	sscanf(token, "ipass=%s", password);

    // Save the SSID and password to the NVS
    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) setting NVS value ssid", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) setting NVS value password", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) committing NVS values", esp_err_to_name(err));
        return ESP_FAIL;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "SSID and password saved to NVS. Restarting...");
    esp_restart();

    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/index.html",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri      = "/results.html",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}