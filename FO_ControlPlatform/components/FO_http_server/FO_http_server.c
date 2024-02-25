/*
 * Created on Wed Nov 28 2022
 * by Uran Cabra, uran.cabra@enchele.com
 */

#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "mdns.h"
#include "cJSON.h"
#include "common.h"
#include "FO_http_server.h"

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t server = NULL;
static app_settings_t* app_settings;
static bool updating = false;

static bool server_resetting = false;

void send_data_ws(){

    if(updating) return;

    char state[301];
    char in_state[301];
    char data[301];
    char to_be_sent[MAX_MESSAGE_LENGTH+1];
    memset(state, 0, 301);
    memset(in_state, 0, 301);
    memset(data, 0, 301);
    memset(to_be_sent, 0, MAX_MESSAGE_LENGTH);
    
    cJSON* state_json = NULL;
    cJSON* in_state_json = NULL;
    cJSON* data_json = NULL;

    parse_state_message(state, 300);
    parse_in_state_message(in_state, 300);
    parse_data_message(data, 300);

    in_state_json = cJSON_Parse(in_state);
    data_json = cJSON_Parse(data);
    state_json = cJSON_Parse(state);

    if(state_json == NULL)
    state_json = cJSON_CreateObject();
    cJSON_AddItemToObject(state_json, "stub", cJSON_CreateString("error"));

    if(in_state_json == NULL)
    in_state_json = cJSON_CreateObject();
    cJSON_AddItemToObject(in_state_json, "stub", cJSON_CreateString("error"));

    if(data_json == NULL)    
    data_json = cJSON_CreateObject();
    cJSON_AddItemToObject(data_json, "stub", cJSON_CreateString("error"));  


    cJSON* complete_json = cJSON_CreateObject();
    
    
    cJSON_AddItemToObject(complete_json, "state", state_json);
    cJSON_AddItemToObject(complete_json, "data", data_json);
    cJSON_AddItemToObject(complete_json, "in_state", in_state_json);
    cJSON_AddStringToObject(complete_json, "server_resetting",server_resetting ? "true" : "false");

    char* info = cJSON_PrintUnformatted(complete_json);

    if(strlen(info) > MAX_MESSAGE_LENGTH){
        ESP_LOGE(TAG, "[send_data ws] The gerenated json length is greater than allowed. JSON length: %d", strlen(info));
    }else{
        strncpy(to_be_sent, info, MAX_MESSAGE_LENGTH);
        ESP_LOGD(TAG, "The json to be sent: %s", to_be_sent);
        send_ws_message(to_be_sent);
    }
    
    cJSON_Delete(complete_json);
    cJSON_free(info);
}



/****************Serve Web Site On Chip************************/
static esp_err_t on_default_url(httpd_req_t *req)
{
  //ESP_LOGI(TAG, "URL: %s", req->uri);
  uint8_t binary_file = false;

  esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/web",
      .partition_label = "web",
      .max_files = 5,
      .format_if_mount_failed = false};
  esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);

  char path[600];
  if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/hotspot-detect.html") == 0
      || strcmp(req->uri, "/generate_204") == 0 || strcmp(req->uri, "/gen_204") == 0)
  {
    strcpy(path, "/web/index.html");
  }
  else
  {
    sprintf(path, "/web%s", req->uri);
  }
  char *ext = strrchr(path, '.');
  if(ext != NULL){
    if (strcmp(ext, ".css") == 0)
    {
      httpd_resp_set_type(req, "text/css");
    }
    if (strcmp(ext, ".js") == 0)
    {
      httpd_resp_set_type(req, "text/javascript");
    }
    if (strcmp(ext, ".png") == 0)
    {
      httpd_resp_set_type(req, "image/png");
      binary_file = true;
    }
    if (strcmp(ext, ".svg") == 0)
    {
      httpd_resp_set_type(req, "image/svg");
      binary_file = true;
    }
  }else{
    snprintf(path,600, "/web%s.html",req->uri);
    binary_file = false;
  }
  FILE *file = binary_file  ?  fopen(path, "rb") : fopen(path, "r");
  
  if (file == NULL)
  {
    strcpy(path, "/web/index.html");
    file = fopen(path, "r");
    binary_file = false;
  }

  if(file == NULL){
    esp_vfs_spiffs_unregister("web");
    return ESP_FAIL;
  }


  char lineRead[256];

  if( binary_file ){
      while (fread(lineRead, sizeof(char), sizeof(lineRead), file)){
        httpd_resp_send_chunk(req, lineRead, sizeof(lineRead));
      }
  }else{
    while (fgets(lineRead, sizeof(lineRead), file)){
      httpd_resp_sendstr_chunk(req, lineRead);
    }
  }

  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_sendstr_chunk(req, NULL);
  
  fclose(file);
  esp_vfs_spiffs_unregister("web");
  
  return ESP_OK;
}
/********************************************************/

/*
 * Handle OTA file upload
 */
static esp_err_t update_post_handler(httpd_req_t *req)
{
  updating = true;
	char buf[1000];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));
  ESP_LOGI(TAG, "update hit");
	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			ESP_LOGI(TAG, "[update] timeout");
      continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
      ESP_LOGI(TAG, "[update] error");
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK) {
      ESP_LOGI(TAG, "[update] success!");
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}
  ESP_LOGI(TAG, "[update] Firmware update complete, rebooting now!");
	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

	vTaskDelay(500 / portTICK_PERIOD_MS);
	esp_restart();

	return ESP_OK;
}



static esp_err_t on_get_state_url(httpd_req_t *req){
   
   char message[MAX_MESSAGE_LENGTH];
   memset(message, 0, MAX_MESSAGE_LENGTH);

   if (req->content_len >= MAX_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_get_state_url] POST request content length exceeds max len!");
    return ESP_FAIL;
   }


   int ret = httpd_req_recv(req, message, req->content_len);

   parse_state_message(message, MAX_MESSAGE_LENGTH);

   if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "aplication/json"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, message));

  ESP_LOGI(TAG, " [parse state] sent message: \n%s", message);
  return ESP_OK;

}


static esp_err_t on_get_data_url(httpd_req_t *req){
   
   char message[MAX_MESSAGE_LENGTH];
   memset(message, 0, MAX_MESSAGE_LENGTH);

   if (req->content_len >= MAX_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_get_data_url] POST request content length exceeds max len!");
    return ESP_FAIL;
   }


   int ret = httpd_req_recv(req, message, req->content_len);

   parse_data_message(message, MAX_MESSAGE_LENGTH);

   if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "aplication/json"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, message));

  ESP_LOGI(TAG, " [parse state] sent message: \n%s", message);
  return ESP_OK;

}

static esp_err_t on_change_settings_url(httpd_req_t *req){

   char message[MAX_MESSAGE_LENGTH];
   memset(message, 0, MAX_MESSAGE_LENGTH);

   if (req->content_len >= MAX_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_get_settings] POST request content length exceeds max len!");
    return ESP_FAIL;
   }

   int ret = httpd_req_recv(req, message, req->content_len);
   
   if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT){
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  change_settings(message, MAX_MESSAGE_LENGTH);

  //ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_hdr(req, "Connection", "open"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "aplication/json"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, message));

  ESP_LOGD(TAG, " [get_settings_url] sent message: \n%s", message);
  return ESP_OK;
}

static esp_err_t on_get_settings_url(httpd_req_t *req){
  
   ESP_LOGI(TAG,"[get_settings_url]");
   char message[MAX_MESSAGE_LENGTH];
   memset(message, 0, MAX_MESSAGE_LENGTH);

   if (req->content_len >= MAX_MESSAGE_LENGTH){
    ESP_LOGE(TAG, "[on_get_settings] POST request content length exceeds max len!");
    return ESP_FAIL;
   }

   int ret = httpd_req_recv(req, message, req->content_len);

   parse_settings_message(message, MAX_MESSAGE_LENGTH);
   
   if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  httpd_resp_set_hdr(req, "Content-Type", "aplication/json");
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "aplication/json"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_sendstr(req, message));

  ESP_LOGD(TAG, " [get_settings_url] sent message: \n%s", message);
  return ESP_OK;

}

static esp_err_t on_toggle_switch_url(httpd_req_t *req){

  char message[MAX_MESSAGE_LENGTH];
  memset(message, 0, MAX_MESSAGE_LENGTH);

  if (req->content_len >= MAX_MESSAGE_LENGTH)
  {
    ESP_LOGE(TAG, "[on_toggle_switch_url] POST request content length exceeds max len!");
    return ESP_FAIL;
  }

  int ret = httpd_req_recv(req, message, req->content_len);

  if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));

  ESP_LOGI(TAG, "received message: %s", message);

  parse_switch_message(message);
  send_data_ws();
  return ESP_OK;

}

static esp_err_t on_manual_control_url(httpd_req_t *req){
  char message[MAX_MESSAGE_LENGTH];
  memset(message, 0, MAX_MESSAGE_LENGTH);

  if (req->content_len >= MAX_MESSAGE_LENGTH)
  {
    ESP_LOGE(TAG, "[on_toggle_switch_url] POST request content length exceeds max len!");
    return ESP_FAIL;
  }

  int ret = httpd_req_recv(req, message, req->content_len);

  if (ret <= 0){
    /* Check if timeout occurred */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_status(req, "204 NO CONTENT"));
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_send(req, NULL, 0));

  ESP_LOGI(TAG, "received message: %s", message);

  parse_manual_control_message(message);
  return ESP_OK;



}


/********************Web Socket Server*******************/
#define WS_MAX_SIZE 1024
static int client_session_id;

esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *ws_pkt) {

    static const size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP] = {};

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

    if (ret != ESP_OK) {
        return ret;
    }
    ESP_LOGD(TAG, "There are %d clients", fds);
    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            ESP_LOGD(TAG, "Sending packet to ws client with id: %d ", client_fds[i]);
            ret = httpd_ws_send_frame_async(server, client_fds[i], ws_pkt);
            if(ret == ESP_FAIL){
              httpd_sess_trigger_close(server,client_fds[i]);
            }
       }else {
        httpd_sess_trigger_close(server,client_fds[i]);
       }
    }
    
    return ESP_OK;
}


esp_err_t send_ws_message(char *message)
{
  if (!client_session_id)
  {
    ESP_LOGD(TAG, "no client_session_id, no message will be sent");
    return ESP_FAIL;
  }
  httpd_ws_frame_t ws_message = {
      .final = true,
      .fragmented = false,
      .len = strlen(message),
      .payload = (uint8_t *)message,
      .type = HTTPD_WS_TYPE_TEXT};

  return httpd_ws_send_frame_to_all_clients(&ws_message);
}

static esp_err_t on_web_socket_url(httpd_req_t *req)
{
  client_session_id = httpd_req_to_sockfd(req);
  ESP_LOGI(TAG, " [on websocket url] client with id: %d connected", client_session_id);
  if (req->method == HTTP_GET){
    send_data_ws();
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.payload = malloc(WS_MAX_SIZE);
  httpd_ws_recv_frame(req, &ws_pkt, WS_MAX_SIZE);
  //printf("ws from client: %.*s\n", ws_pkt.len, ws_pkt.payload);
  free(ws_pkt.payload);
  send_data_ws();
  return ESP_OK;
}
/*****************************************************/

void start_mdns_service()
{
  mdns_init();
xSemaphoreTake(app_settings->mutex, portMAX_DELAY);
  mdns_hostname_set(app_settings->ap_name);
xSemaphoreGive(app_settings->mutex);
  mdns_instance_name_set("FO contorl panel");
}


// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}


void server_start(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_open_sockets = 13;
  config.stack_size = 1024*6;
  config.lru_purge_enable = true;
  
  if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG,"Server Started!");
  }
  
  httpd_uri_t web_socket_url = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = on_web_socket_url,
      .is_websocket = true};
  httpd_register_uri_handler(server, &web_socket_url);

  httpd_uri_t toggle_switch_url = {
      .uri = "/api/toggle-switch",
      .method = HTTP_POST,
      .handler = on_toggle_switch_url};
  httpd_register_uri_handler(server, &toggle_switch_url);

  httpd_uri_t manual_control_url = {
      .uri = "/api/manual-control",
      .method = HTTP_POST,
      .handler = on_manual_control_url};
  httpd_register_uri_handler(server, &manual_control_url);


  httpd_uri_t get_settings_url = {
      .uri = "/api/get-settings",
      .method = HTTP_POST,
      .handler = on_get_settings_url};
  httpd_register_uri_handler(server, &get_settings_url);

  httpd_uri_t change_settings_url = {
      .uri = "/api/change-settings",
      .method = HTTP_POST,
      .handler = on_change_settings_url};
  httpd_register_uri_handler(server, &change_settings_url);

  httpd_uri_t update_url = {
      .uri = "/api/update",
      .method = HTTP_POST,
      .handler = update_post_handler};
  httpd_register_uri_handler(server, &update_url);

  httpd_uri_t default_url = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = on_default_url};
  httpd_register_uri_handler(server, &default_url);
  
  ESP_ERROR_CHECK( httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,NULL));
  ESP_ERROR_CHECK( httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler));
}

void server_stop(){
  if(server){
    httpd_stop(server);
    server = NULL;
    ESP_LOGI(TAG,"Server Stopped!");
  }
}

void server_task(void* param){
  while(1){
    if(!server){
      server_start();
    }else{
      server_resetting = true;
      send_data_ws();
      server_stop();
      vTaskDelay(pdMS_TO_TICKS(250));
      server_start();
      server_resetting = false;
      
    }
    vTaskDelay(pdMS_TO_TICKS(3600000));
  }
}

esp_err_t init_server(app_settings_t* settings)
{
  app_settings = settings;
  xTaskCreatePinnedToCore(server_task, "server_task", 1024*8, NULL, 9, NULL, 1);
  start_mdns_service();
  return ESP_OK;
}