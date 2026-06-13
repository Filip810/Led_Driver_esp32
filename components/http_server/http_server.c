#include "http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "led_control.h"

static const char *TAG = "HTTP";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

/* ── helpers  */

static int read_body(httpd_req_t *req, char *buf, size_t max)
{
    if (req->content_len == 0)
    {
        buf[0] = '\0';
        return 0;
    }
    if (req->content_len >= max)
        return -1;

    int n = httpd_req_recv(req, buf, req->content_len);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
    return n;
}

static void json_response(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
}

/* ── GET  */

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start,
                    (ssize_t)(index_html_end - index_html_start - 1));
    return ESP_OK;
}

/* ── GET /state  */

static esp_err_t handle_get_state(httpd_req_t *req)
{
    led_rgb_state_t s;
    led_rgb_get_state(&s);

    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"on\":%s,\"r\":%d,\"g\":%d,\"b\":%d,"
             "\"brightness\":%d,\"timer_remaining\":%d}",
             s.on ? "true" : "false",
             s.r, s.g, s.b,
             s.brightness,
             s.timer_remaining_s < 0 ? 0 : (int)s.timer_remaining_s);

    json_response(req, buf);
    return ESP_OK;
}

/* ── POST /color  { "r":0-255, "g":0-255, "b":0-255 } */

static esp_err_t handle_post_color(httpd_req_t *req)
{
    char buf[128];
    if (read_body(req, buf, sizeof(buf)) < 0)
        return ESP_FAIL;

    cJSON *j = cJSON_Parse(buf);
    if (!j)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    cJSON *jr = cJSON_GetObjectItem(j, "r");
    cJSON *jg = cJSON_GetObjectItem(j, "g");
    cJSON *jb = cJSON_GetObjectItem(j, "b");

    if (!cJSON_IsNumber(jr) || !cJSON_IsNumber(jg) || !cJSON_IsNumber(jb))
    {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing r/g/b");
        return ESP_FAIL;
    }

    led_rgb_set_color((uint8_t)jr->valueint,
                      (uint8_t)jg->valueint,
                      (uint8_t)jb->valueint);
    cJSON_Delete(j);
    json_response(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ── POST /brightness  { "value":0-100 } */

static esp_err_t handle_post_brightness(httpd_req_t *req)
{
    char buf[64];
    if (read_body(req, buf, sizeof(buf)) < 0)
        return ESP_FAIL;

    cJSON *j = cJSON_Parse(buf);
    if (!j)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    cJSON *jv = cJSON_GetObjectItem(j, "value");
    if (!cJSON_IsNumber(jv))
    {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing value");
        return ESP_FAIL;
    }

    led_rgb_set_brightness((uint8_t)jv->valueint);
    cJSON_Delete(j);
    json_response(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ── POST /power  { "on":true|false } */

static esp_err_t handle_post_power(httpd_req_t *req)
{
    char buf[64];
    if (read_body(req, buf, sizeof(buf)) < 0)
        return ESP_FAIL;

    cJSON *j = cJSON_Parse(buf);
    if (!j)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    cJSON *jon = cJSON_GetObjectItem(j, "on");
    if (!cJSON_IsBool(jon))
    {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing on");
        return ESP_FAIL;
    }

    led_rgb_set_power(cJSON_IsTrue(jon));
    cJSON_Delete(j);
    json_response(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ── POST /timer  { "seconds": N }  (N=0 stops timer) */

static esp_err_t handle_post_timer(httpd_req_t *req)
{
    char buf[64];
    if (read_body(req, buf, sizeof(buf)) < 0)
        return ESP_FAIL;

    cJSON *j = cJSON_Parse(buf);
    if (!j)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
        return ESP_FAIL;
    }

    cJSON *js = cJSON_GetObjectItem(j, "seconds");
    if (!cJSON_IsNumber(js))
    {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing seconds");
        return ESP_FAIL;
    }

    int secs = js->valueint;
    cJSON_Delete(j);

    if (secs > 0)
    {
        led_rgb_timer_start((uint32_t)secs);
    }
    else
    {
        led_rgb_timer_stop();
    }

    json_response(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ── server init */

void http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 8;

    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = handle_get_root},
        {.uri = "/state", .method = HTTP_GET, .handler = handle_get_state},
        {.uri = "/color", .method = HTTP_POST, .handler = handle_post_color},
        {.uri = "/brightness", .method = HTTP_POST, .handler = handle_post_brightness},
        {.uri = "/power", .method = HTTP_POST, .handler = handle_post_power},
        {.uri = "/timer", .method = HTTP_POST, .handler = handle_post_timer},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
    {
        httpd_register_uri_handler(server, &routes[i]);
    }

    ESP_LOGI(TAG, "HTTP server ready on port %d", cfg.server_port);
}
