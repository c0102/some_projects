/*
 * Created on Thu Jun 02 2022
 * by Uran Cabra, uran.cabra@enchele.com
 * Copyright (c) 2022 Enchele Inc.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "parse.h"
#include "server.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <enchele.h>


static const char *TAG = "SERVER";

static app_settings_t* app_settings = NULL;
extern QueueHandle_t Message_queue;
extern SemaphoreHandle_t settings_mutex;


static void urlDecode(char *dStr)
{
  // NOTE(uran): taken from https://github.com/swarajsatvaya/UrlDecoder-C

  char eStr[] = "00"; /* for a hex code */
  int i;              /* the counter for the string */
  for (i = 0; i < strlen(dStr); ++i)
  {
    if (dStr[i] == '%')
    {
      if (dStr[i + 1] == 0)
        return;

      if (isxdigit(dStr[i + 1]) && isxdigit(dStr[i + 2]))
      {
        /* combine the next to numbers into one */
        eStr[0] = dStr[i + 1];
        eStr[1] = dStr[i + 2];

        /* convert it to decimal */
        long int x = strtol(eStr, NULL, 16);

        /* remove the hex */
        memmove(&dStr[i + 1], &dStr[i + 3], strlen(&dStr[i + 3]) + 1);

        dStr[i] = x;
      }
    }
    else if (dStr[i] == '+')
    {
      dStr[i] = ' ';
    }
  }
}

static esp_err_t on_get_aps_url(httpd_req_t *req){
  
  ESP_LOGI(TAG, "URL: %s", req->uri);
  
  char resp[CONFIG_MESSAGE_LENGTH];
  memset(resp, 0, CONFIG_MESSAGE_LENGTH);
  
  /* TODO(uran): This is temporary. Make it so the site has the thingname loaded in local session storage 
  *              and give us the whole message that way
  */
  
  


  cJSON *root = cJSON_CreateObject();
  
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "GET APS: in app_settings: %s", app_settings->thingname);
    cJSON_AddStringToObject(root, "id", app_settings->thingname);
  xSemaphoreGive(settings_mutex);

  cJSON_AddStringToObject(root, "from", "http");
  cJSON_AddStringToObject(root, "cmd", "get_aps");
  char *json_mess = cJSON_Print(root);
  
  ESP_ERROR_CHECK(parse_message(resp, json_mess, strlen(json_mess)));
  
  ESP_LOGI(TAG, " Queue message recieved, message.response: %s", resp);
  if (strlen(resp) > 0)
  {
    ESP_LOGI(TAG, "AP response: %s", resp);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, resp));
  }
  else
  {
    ESP_LOGI(TAG, "Didn't respond, response length 0, response: %s", resp);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send_404(req));
  }

  cJSON_Delete(root);
  cJSON_free(json_mess);
  return ESP_OK;
}

static esp_err_t on_get_settings_url(httpd_req_t *req){
  
  ESP_LOGI(TAG, "URL: %s", req->uri);
  
  char resp[CONFIG_MESSAGE_LENGTH];
  memset(resp, 0, CONFIG_MESSAGE_LENGTH);
 
  /* TODO(uran): This is temporary. Make it so the site has the thingname loaded in local session storage 
  *              and give us the whole message that way
  */


  cJSON *root = cJSON_CreateObject();
  
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "GET settings: in app_settings: %s", app_settings->thingname);
    cJSON_AddStringToObject(root, "id", app_settings->thingname);
  xSemaphoreGive(settings_mutex);
  
  cJSON_AddStringToObject(root, "from", "http");
  cJSON_AddStringToObject(root, "cmd", "get_settings");
  char *json_mess = cJSON_Print(root);
  
  ESP_ERROR_CHECK(parse_message(resp, json_mess, strlen(json_mess)));
  
  // strcpy(message.body, json_mess);
  // if (xQueueSend(Message_queue, &message, portMAX_DELAY) != pdTRUE)
  // {
  //   ESP_LOGE(TAG, "Could't send message to parse!!");
  // }

  // ESP_LOGI(TAG, "Message send to parser, message address: %p", &message);

  // while (!xQueueReceive(message.resp_queue, (void *)&resp_message, portMAX_DELAY))
  // {
  //   ESP_LOGI(TAG, " GET APS: Waiting for response");
  // }

  ESP_LOGI(TAG, " Queue message recieved, message.response: %s", resp);
  if (strlen(resp) > 0)
  {
    ESP_LOGI(TAG, "Get Settings response: %s", resp);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, resp));
  }
  else
  {
    ESP_LOGI(TAG, "Didn't respond, response length 0, response: %s", resp);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send_404(req));
  }

  //vQueueDelete(message.resp_queue);
  cJSON_Delete(root);
  cJSON_free(json_mess);
  return ESP_OK;
}

static esp_err_t on_default_url(httpd_req_t *req)
{
  ESP_LOGI(TAG, "URL: %s", req->uri);
  esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false};
  ESP_ERROR_CHECK(esp_vfs_spiffs_register(&esp_vfs_spiffs_conf));

  char path[500];

  if (strcmp(req->uri, "/") == 0)
  {
    strcpy(path, "/spiffs/index.html");
  }
  else
  {
    //TODO(uran): Check for the length of the incoming uri
    sprintf(path, "/spiffs%8.490s", req->uri);
    char *ext = strrchr(path, '.');
    if(ext != NULL){
      if (strcmp(ext, ".css") == 0)
      {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "text/css"));
      }
      if (strcmp(ext, ".js") == 0)
      {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "text/javascript"));
      }
      if (strcmp(ext, ".png") == 0)
      {
        ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "image/png"));
      }
    }else{
      strcat(path, ".html");
    }
  }
  FILE *file = fopen(path, "r");
  if (file == NULL)
  {
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send_404(req));
    ESP_ERROR_CHECK(esp_vfs_spiffs_unregister(NULL));
    return ESP_OK;
  }
  char line[256];
  while (fgets(line, sizeof(line), file))
  {

    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr_chunk(req, line));
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr_chunk(req, NULL));
  
  fclose(file);
  ESP_ERROR_CHECK(esp_vfs_spiffs_unregister(NULL));
  return ESP_OK;
}

static esp_err_t on_toggle_led_url_get(httpd_req_t *req){

  ESP_LOGI(TAG, "URL: %s", req->uri);

  char buf[CONFIG_MESSAGE_LENGTH + 1];
  memset(buf, 0, CONFIG_MESSAGE_LENGTH + 1);
  
  char message[CONFIG_MESSAGE_LENGTH];
  memset(message, 0, CONFIG_MESSAGE_LENGTH);

  char resp[CONFIG_MESSAGE_LENGTH];
  memset(resp, 0, CONFIG_MESSAGE_LENGTH);

  size_t buf_len = httpd_req_get_url_query_len(req) + 1;
  
  if (buf_len > 1)
  {
    if (buf_len > CONFIG_MESSAGE_LENGTH)
    {
      ESP_LOGE(TAG, "ERROR - The query length is too long, query is: %d bytes log, max is : %d", buf_len, CONFIG_MESSAGE_LENGTH);
      ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
      ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));
      return ESP_OK;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
    {
      ESP_LOGI(TAG, "Found URL query => %s", buf);
      if (httpd_query_key_value(buf, "cmd", message, CONFIG_MESSAGE_LENGTH) == ESP_OK)
      {
        //ESP_LOGI(TAG, "Found URL query parameter => cmd=%s", message.body);
        urlDecode(message);
        
        ESP_ERROR_CHECK(parse_message(resp, message, strlen(message)));
        
        //ESP_LOGI(TAG, "vURL query parameter after URL DECODE => cmd=%s", message.body);
        // if (xQueueSend(Message_queue, &message, portMAX_DELAY) != pdTRUE)
        // {
        //   ESP_LOGE(TAG, "Could't send message to parse!!");
        // }
      }
      else
      {
        ESP_LOGI(TAG, "cmd query not found!");
      }
    }
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));
  //vQueueDelete(message.resp_queue);
  return ESP_OK;
}


/*TODO(uran): Duhet me e lon veq njonin prej funksioneve me poshte, 
              se e bojne gati identik punen e njejte*/

static esp_err_t on_toggle_led_url(httpd_req_t *req){

  char message[CONFIG_MESSAGE_LENGTH];
  memset(message, 0, CONFIG_MESSAGE_LENGTH);
  
  char resp[CONFIG_MESSAGE_LENGTH];
  memset(resp, 0, CONFIG_MESSAGE_LENGTH);

  if(req->content_len >= CONFIG_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_toggle_led_url] POST request content length exceeds max len!");
    return ESP_FAIL;
  }

  int ret = httpd_req_recv(req, message, req->content_len);

  if (ret <= 0) {  
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }

        return ESP_FAIL;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));

  
  
  cJSON *root = cJSON_Parse(message);

  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "TOGGLE LED: in app_settings: %s", app_settings->thingname);
    cJSON_AddStringToObject(root, "id", app_settings->thingname);
  xSemaphoreGive(settings_mutex);
  
  char *json_mess = cJSON_Print(root);

  memset(message, 0, sizeof(CONFIG_MESSAGE_LENGTH));
  strncpy(message, json_mess, CONFIG_MESSAGE_LENGTH);

  ESP_ERROR_CHECK(parse_message(resp, message, strnlen(message, CONFIG_MESSAGE_LENGTH)));
  
  cJSON_free(json_mess);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t on_toggle_relay_url(httpd_req_t *req){

  char message[CONFIG_MESSAGE_LENGTH];
  memset(message, 0, CONFIG_MESSAGE_LENGTH);
  
  char resp[CONFIG_MESSAGE_LENGTH];
  memset(resp, 0, CONFIG_MESSAGE_LENGTH);

  if(req->content_len >= CONFIG_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_toggle_relay_url] POST request content length exceeds max length");
    return ESP_FAIL;
  }

  int ret = httpd_req_recv(req, message, req->content_len);
  if (ret <= 0) {  
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }

        return ESP_FAIL;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));

  cJSON *root = cJSON_Parse(message);
  
  xSemaphoreTake(settings_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "TOGGLE Relay: in app_settings: %s", app_settings->thingname);
    cJSON_AddStringToObject(root, "id", app_settings->thingname);
  xSemaphoreGive(settings_mutex);

  char *json_mess = cJSON_Print(root);
  memset(message, 0, sizeof(CONFIG_MESSAGE_LENGTH));

  strncpy(message, json_mess, CONFIG_MESSAGE_LENGTH);
  
  ESP_ERROR_CHECK(parse_message(resp, message, strnlen(message, CONFIG_MESSAGE_LENGTH)));

  //vQueueDelete(message.resp_queue);
  
  cJSON_free(json_mess);
  cJSON_Delete(root);
  return ESP_OK;
}


void init_server(app_settings_t* settings)
{
  app_settings = settings;
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_ERROR_CHECK(httpd_start(&server, &config));
  ESP_LOGI(TAG, "SERVER Initialized!");

  httpd_uri_t toggle_led_url_get = {
      .uri = "/api/toggle-led-get",
      .method = HTTP_GET,
      .handler = on_toggle_led_url_get};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &toggle_led_url_get));

  httpd_uri_t toggle_led_url = {
      .uri = "/api/toggle-led",
      .method = HTTP_POST,
      .handler = on_toggle_led_url};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &toggle_led_url));
  
  httpd_uri_t toggle_relay_url = {
      .uri = "/api/toggle-relay",
      .method = HTTP_POST,
      .handler = on_toggle_relay_url};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &toggle_relay_url));

  httpd_uri_t get_settings = {
      .uri = "/api/get_settings",
      .method = HTTP_GET,
      .handler = on_get_settings_url};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_settings));

  httpd_uri_t get_aps = {
      .uri = "/api/get_aps",
      .method = HTTP_GET,
      .handler = on_get_aps_url};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_aps));

  httpd_uri_t default_url = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = on_default_url};
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &default_url));
}