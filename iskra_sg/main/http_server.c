/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

#ifdef WEB_SERVER_ENABLED

#ifdef SECURE_HTTP_SERVER
#include <esp_https_server.h>
#endif

/* Max length a file path can have on storage */
//#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
//#define SCRATCH_BUFSIZE  (8192)

#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (1024*8)

#if 0
typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;
#endif

//extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
extern char cert_pem[2048];
extern char update_url[];
extern nvs_settings_t nvs_settings_array[];
extern const esp_app_desc_t *app_desc;
extern const char* upgradeStatusStr[];
extern uint8_t eeprom_size;

static const char *TAG = "http_server";
static int get_commands_count = 0;
static char param[1024];

void simple_ota_example_task(void * pvParameter);
void ota_spiffs_task(void * pvParameter);
char *get_settings_json(void);

static void send_error_JSON(httpd_req_t *req, char *command);

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

/* decode URL (replace%2f to / ...) and copy result to out buffer */
int ishex(int x)
{
	return	(x >= '0' && x <= '9')	||
		(x >= 'a' && x <= 'f')	||
		(x >= 'A' && x <= 'F');
}

int url_decode(const char *s, char *dec)
{
	char *o;
	const char *end = s + strlen(s);
	int c;

	for (o = dec; s <= end; o++) {
		c = *s++;
		if (c == '+') c = ' ';
		else if (c == '%' && (	!ishex(*s++)	||
					!ishex(*s++)	||
					!sscanf(s - 2, "%2x", &c)))
			return -1;

		if (dec) *o = c;
	}

	return o - dec;
}

#if 0 //limited number of handlers
/* Handler to redirect incoming GET request for /index.html to / */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "301 Permanent Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");      //zurb
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}
#endif

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path */
static esp_err_t http_resp_update_html(httpd_req_t *req)
{
	/* Send HTML file header */
	//httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"light_style.css\"></head><body>");
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\
	    <html>\
	    <head>\
	    <style type=\"text/css\">\
		body {\
  		width: 480px;\
  		height: 320px;\
		}\
		</style>\
	    <title>Iskra SG</title>\
	    </head>\
	    <body>");

	httpd_resp_sendstr_chunk(req, "	<form action=\"/get_command\">\
	         <fieldset>\
	         <legend>Firmware Upgrade</legend>\
	          <br>Upgrade file URL:\
	          <br>\
	          <input type=\"text\" name=\"upgrade_url\" id=\"upgrade_url\" style=\"width:100%;\">\
	          <br>\
	          <br>\
	          <label for=\"server_certificate\">Server certificate filename:</label>\
	          <input type=\"text\" id=\"server_certificate\" name=\"server_certificate\" style=\"width:100%;\">\
	          <br>\
	          </fieldset>\
	          <br>\
	          <input type=\"hidden\" name=\"command\" id=\"command\" value =\"ota_upgrade\">\
	          <input type=\"submit\" value=\"Start Upgrade\" >\
	        </form>\
	    </body>\
	    </html>");

	    /* Send empty chunk to signal HTTP response completion */
	    httpd_resp_sendstr_chunk(req, NULL);
	    return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path */
static esp_err_t http_resp_dir_html(httpd_req_t *req, char *file_ext)
{
    char fullpath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    DIR *dir = NULL;
    struct dirent *entry;
    struct stat entry_stat;

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(fullpath, ((struct file_server_data *)req->user_ctx)->base_path);

    /* Concatenate the requested directory path */
    //zurb: use base path for spiffs
    //strcat(fullpath, req->uri);   //zurb
    strcat(fullpath, "/");   //zurb
    MY_LOGI(TAG, "Root dir : %s", fullpath);
    MY_LOGI(TAG, "URI : %s", req->uri);
    dir = opendir(fullpath);
    const size_t entrypath_offset = strlen(fullpath);

    if (!dir) {
        MY_LOGE(TAG, "Failed to stat dir : %s", fullpath);
        statistics.errors++;
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Send HTML file header */
    //httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"light_style.css\"></head><body>");
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\
    <html>\
    <head>\
    <link rel=\"stylesheet\" type=\"text/css\" href=\"light_style.css\">\
    <title>Iskra SG</title>\
    </head>\
    <body>");

    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    //httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Send main table start and header row */
    httpd_resp_sendstr_chunk(req,
    		"<table id=\"t01\"><tbody>"
    		"<tr style=\"font-weight: bold; font-size: 200%; font-family: Verdana;text-align: center;\">"
    		"<td> <img src=\"iskra_logo1.jpg\" alt=\"Iskra\"></td>"
			"<td></td>"
			"<td>SG File Browser</td>"
			"</tr>"
    		);

    /* Send one row to match other web pages design */
    httpd_resp_sendstr_chunk(req,
    		"<tr><th></th><th></th><th></th></tr>"
    		"<tr style=\"height: 10px;\"><td></td></tr><tr>"
    );

    /* Send left vertical menu */
    httpd_resp_sendstr_chunk(req,
    		"<td valign=\"top\">"
    		"<div class=\"vertical-menu\">"
    		"<a href=\"/index.html\" >SG Status</a>"
    		"<a href=\"/settings_general.html\">SG Settings</a>"
    		"<a href=\"/measurements.html\">Measurements</a>"
    		"<a href=\"/counters.html\">Energy Counters</a>"
    		"<a href=\"/graph.html\">Load Profile</a>"
    		"<a href=\"/bicom.html\">Bicom control</a>"
    		"<a href=\"/edit\" class=\"active\">SG File Browser</a>"
    		"<a href=\"/update.html\">SG Upgrade</a>"
    		"</div>"
    		"</td>"
    		"<td></td>"
    		"<td valign=\"top\">"
    );
    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
    		"<table class=\"fixed\" border=\"1\">"
    		"<col width=\"300px\" /><col width=\"100px\" /><col width=\"100px\" /><col width=\"80px\" />"
    		"<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
    		"<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strncpy(fullpath + entrypath_offset, entry->d_name, sizeof(fullpath) - entrypath_offset);
        if (stat(fullpath, &entry_stat) == -1) {
            MY_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            statistics.errors++;
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        MY_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
        if(*file_ext != '*') //* is without filtering
        {
        	//MY_LOGI(TAG, "file:%s size:%d ext:%s size:%d", entry->d_name, strlen(entry->d_name), file_ext, strlen(file_ext));
        	if(!IS_FILE_EXT(entry->d_name, file_ext))//return only files with selected mask
        		continue;
        }

        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\""); 
        httpd_resp_sendstr_chunk(req, "/"); //httpd_resp_sendstr_chunk(req, req->uri);    //zurb
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
        httpd_resp_sendstr_chunk(req, "/"); //httpd_resp_sendstr_chunk(req, req->uri); zurb
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</td></tbody></table>");

    /* Finish the main table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req)
{
    //MY_LOGI(TAG, "File type : %s", req->uri);
    if (IS_FILE_EXT(req->uri, ".pdf")) return httpd_resp_set_type(req, "application/pdf");
    else if (IS_FILE_EXT(req->uri, ".html")) return httpd_resp_set_type(req, "text/html");
    else if (IS_FILE_EXT(req->uri, ".jpeg")) return httpd_resp_set_type(req, "image/jpeg");      
    else if (IS_FILE_EXT(req->uri, ".css")) return httpd_resp_set_type(req, "text/css");    
    else if (IS_FILE_EXT(req->uri, ".js")) return httpd_resp_set_type(req, "application/javascript");
    else if (IS_FILE_EXT(req->uri, ".png")) return httpd_resp_set_type(req, "image/png");
    else if (IS_FILE_EXT(req->uri, ".jpg")) return httpd_resp_set_type(req, "image/jpeg");
    else if (IS_FILE_EXT(req->uri, ".gz")) return httpd_resp_set_type(req, "application/x-gzip");
    else if (IS_FILE_EXT(req->uri, ".ico")) return httpd_resp_set_type(req, "image/x-icon");
    
    /* This is a limited set only */
    /* For any other type always set as plain text */
    MY_LOGW(TAG, "Sending as text/plain : %s", req->uri);
    return httpd_resp_set_type(req, "text/plain");
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t http_resp_file(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);

    //MY_LOGI(TAG, "URI : %s", req->uri);
    /* Concatenate the requested file path */
    if(strcmp(req->uri, "/") == 0) //index.html handler
    	strcat(filepath, "/index.html");
    else
    	strcat(filepath, req->uri);

    if (stat(filepath, &file_stat) == -1) {
        MY_LOGW(TAG, "Failed to stat file : %s. Trying with gz", filepath);
        //zurb gz support        
        strcat(filepath, ".gz");        
        if (stat(filepath, &file_stat) == -1) {                
          /* Respond with 404 Not Found */
          MY_LOGE(TAG, "Failed to stat file : %s", filepath);
          statistics.errors++;
          //httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
          http_resp_update_html(req); //if file not found, redirect to update page
          return ESP_FAIL;
        }
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        MY_LOGE(TAG, "Failed to read existing file : %s", filepath);
        statistics.errors++;
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    //MY_LOGI(TAG, "Sending file : %s (%ld bytes), uri:%s...", filepath, file_stat.st_size, req->uri);
    if(strcmp(req->uri, "/") == 0) //index.html handler
    	httpd_resp_set_type(req, "text/html");
    else
    	set_content_type_from_file(req);
    
    //zurb: add encoding
    if (IS_FILE_EXT(filepath, ".gz"))
      httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    if(file_stat.st_size > SCRATCH_BUFSIZE)
    {
    	do {
    		/* Read file in chunks into the scratch buffer */
    		chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
    		if(chunksize == 0)//bugfix
    			break;
    		/* Send the buffer contents as HTTP response chunk */
    		if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
    			fclose(fd);
    			MY_LOGE(TAG, "File sending failed!");
    			statistics.errors++;
    			/* Abort sending file */
    			httpd_resp_sendstr_chunk(req, NULL);
    			/* Respond with 500 Internal Server Error */
    			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
    			return ESP_FAIL;
    		}

    		/* Keep looping till the whole file is sent */
    	} while (chunksize != 0);

    	/* Respond with an empty chunk to signal HTTP response completion */
    	httpd_resp_send_chunk(req, NULL, 0);
    }
    else //all in one chunk
    {
    	chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
    	if (httpd_resp_send(req, chunk, chunksize) != ESP_OK) {
    		fclose(fd);
    		MY_LOGE(TAG, "File sending failed!");
    		statistics.errors++;
    		/* Respond with 500 Internal Server Error */
    		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
    		return ESP_FAIL;
    	}
    }

    /* Close file after sending complete */
    fclose(fd);
    //MY_LOGI(TAG, "File sending complete");

    return ESP_OK;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
	//statistics.http_requests++;

    // Check if the target is a directory
    //if (req->uri[strlen(req->uri) - 1] == '/') {
    if (strcmp(req->uri,  "/edit") == 0) {  //zurb
        // In so, send an html with directory listing
        MY_LOGI(TAG, "edit req:%s", req->uri); //zurb

        http_resp_dir_html(req, "*"); //all files
    }
    else if (strcmp(req->uri,  "/cert") == 0) {  //zurb
            // In so, send an html with directory listing
            MY_LOGI(TAG, "cert req:%s", req->uri); //zurb
            http_resp_dir_html(req, ".pem"); //only pem files
        }
    else {
        // Else send the file
        //MY_LOGI(TAG, "File req:%s", req->uri); //zurb
        http_resp_file(req);
    }
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    //statistics.http_requests++;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = req->uri + sizeof("/upload") - 1;

    /* Filename cannot be empty or have a trailing '/' */
    if (strlen(filename) == 0 || filename[strlen(filename) - 1] == '/') {
        MY_LOGE(TAG, "Invalid file name : %s", filename);
        statistics.errors++;
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file name");
        return ESP_FAIL;
    }

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);

    /* Concatenate the requested file path */
    strcat(filepath, filename);
    if (stat(filepath, &file_stat) == 0) {
        MY_LOGE(TAG, "File already exists : %s", filepath);
        statistics.errors++;
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        MY_LOGE(TAG, "File too large : %d bytes", req->content_len);
        statistics.errors++;
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        MY_LOGE(TAG, "Failed to create file : %s", filepath);
        statistics.errors++;
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    MY_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        MY_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            MY_LOGE(TAG, "File reception failed!");
            statistics.errors++;
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            MY_LOGE(TAG, "File write failed!");
            statistics.errors++;
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    MY_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    if (IS_FILE_EXT(filename, ".pem"))
    	httpd_resp_set_hdr(req, "Location", "/cert");
    else
    	httpd_resp_set_hdr(req, "Location", "/edit");

    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    //statistics.http_requests++;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = req->uri + sizeof("/upload") - 1;
    MY_LOGI(TAG, "Delete file name : %s", filename);

    /* Filename cannot be empty or have a trailing '/' */
    if (strlen(filename) == 0 || filename[strlen(filename) - 1] == '/') {
        MY_LOGE(TAG, "Invalid file name : %s", filename);
        statistics.errors++;
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file name");
        return ESP_FAIL;
    }

    /* Retrieve the base path of file storage to construct the full path */
    strcpy(filepath, ((struct file_server_data *)req->user_ctx)->base_path);
    MY_LOGI(TAG, "Delete filepath : %s", filepath);

    /* Concatenate the requested file path */
    strcat(filepath, filename);
    if (stat(filepath, &file_stat) == -1) {
        MY_LOGE(TAG, "File does not exist : %s", filename);
        statistics.errors++;
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    MY_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    if (IS_FILE_EXT(filename, ".pem"))
    	httpd_resp_set_hdr(req, "Location", "/cert");
    else
    	httpd_resp_set_hdr(req, "Location", "/edit");

    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

#if 0
/* An HTTP GET handler */
esp_err_t settings_parse_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
#if 0
    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            MY_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            MY_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            MY_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }
#endif

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
    	int i;
    	buf = malloc(buf_len);
    	if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
    		MY_LOGI(TAG, "Found URL query => %s", buf);
    		//char param[100];
    		/* Get value of expected key from query string */
    		for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
    			if (httpd_query_key_value(buf, nvs_settings_array[i].name, param, sizeof(param)) == ESP_OK) {
    				MY_LOGI(TAG, "Found URL query parameter => %s=%s", nvs_settings_array[i].name, param);
    				if(nvs_settings_array[i].type == NVS_STRING)
    					strcpy(nvs_settings_array[i].value, param);
    				else if(nvs_settings_array[i].type == NVS_INT8)
    					*(int8_t *)nvs_settings_array[i].value = atoi(param);
    				else if(nvs_settings_array[i].type == NVS_UINT8)
    					*(uint8_t *)nvs_settings_array[i].value = atoi(param);
    				else if(nvs_settings_array[i].type == NVS_INT16)
    					*(int16_t *)nvs_settings_array[i].value = atoi(param);
    				else if(nvs_settings_array[i].type == NVS_INT32)
    					*(int32_t *)nvs_settings_array[i].value = atoi(param);
    			}
    		}
    	}
    	free(buf);

    	httpd_resp_set_status(req, "204 No Content");
    	httpd_resp_send(req, NULL, 0);  // Response body can be empty

    	err_t err = save_settings_nvs(RESTART);
    	if(err != ESP_OK)
    		MY_LOGE(TAG, "Save settings Failed:%d", err);
    }
#if 0
    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        MY_LOGI(TAG, "Request headers lost");
    }
#endif

    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty

    return ESP_OK;
}
#endif

/* Handler for ota */
static esp_err_t ota_handler(httpd_req_t *req, char*  buf, size_t buf_len)
{
	esp_err_t ret;
	MY_LOGI(TAG, "http OTA request...");
	statistics.http_requests++;

#if 0
	/* Redirect onto root to prevent update loop */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
	ret = httpd_resp_sendstr(req, "Starting OTA...");
	if (ret != ESP_OK)
		MY_LOGE(TAG, "%s httpd_resp_sendstr:%d", __FUNCTION__, ret);
#endif

	memset(update_url, 0, UPDATE_URL_SIZE);
	memset(cert_pem, 0, sizeof(cert_pem));

	char cert_filename[80];
	//char param[2048];
	/* Get value of expected key from query string */
	if (httpd_query_key_value(buf, "upgrade_url", param, sizeof(param)) == ESP_OK) {
		MY_LOGI(TAG, "%s URL query parameter => upgrade_url=%s size:%d", __FUNCTION__, param, strlen(param));
		url_decode(param, update_url);
		//strcpy(update_url, param);
		MY_LOGI(TAG, "URL decoded parameter => upgrade_url=%s size:%d", update_url, strlen(update_url));
	}
	if (httpd_query_key_value(buf, "server_certificate", param, sizeof(param)) == ESP_OK) {
		MY_LOGI(TAG, "%s URL query parameter => server_certificate=%s size:%d", __FUNCTION__, param, strlen(param));
		//url_decode(param, cert_filename);
		strcpy(cert_filename, "/spiffs/"); //add root dir
		strcat(cert_filename, param);
		MY_LOGI(TAG, "Server_certificate filename=%s", cert_filename);
	}

	if(strlen(cert_filename)> 4 && strlen(update_url) > 10)
	{
		struct stat entry_stat;

		if (stat(cert_filename, &entry_stat) == -1) {
			MY_LOGE(TAG, "Failed to stat %s", cert_filename);
			statistics.errors++;
			status.upgrade.status = CERT_FILE_NOT_FOUND;
			goto upgrade_failed;
		}

		MY_LOGI(TAG, "Found %s (%ld bytes)", cert_filename, entry_stat.st_size);

		ret = read_file(cert_filename, cert_pem, sizeof(cert_pem));
		if(ret != ESP_OK)
		{
			MY_LOGE(TAG, "Failed to stat %s", cert_filename);
			statistics.errors++;
			status.upgrade.status = CERT_FILE_NOT_FOUND;
			goto upgrade_failed;
		}

		if(strlen(cert_pem)> 100)
		{
			//char msg[100];

			strcpy(status.app_status, "Upgrading");
			led_set(LED_RED, LED_BLINK_FAST);
			MY_LOGI(TAG, "http OTA started. URL: %s", update_url);
			//sprintf(msg, "Firmware upgrade Started\nUrl:%s", update_url);
			//mqtt_publish(0, mqtt_topic[0].info, msg, 0, 0, 0);
			//mqtt_publish(1, mqtt_topic[1].info, msg, 0, 0, 0);
			//vTaskDelay(1000 / portTICK_PERIOD_MS);

			//xEventGroupSetBits(ethernet_event_group, HTTP_STOP); //stop http server to release some memory
			xEventGroupSetBits(ethernet_event_group, MQTT_STOP); //stop mqtt client to release some memory
			xEventGroupSetBits(ethernet_event_group, TCP_SERVER_STOP); //stop tcp server to release some memory
			xEventGroupSetBits(ethernet_event_group, BICOM_TASK_STOP); //stop bicom task
			xEventGroupSetBits(ethernet_event_group, ADC_TASK_STOP); //stop adc task
			xEventGroupSetBits(ethernet_event_group, COUNT_TASK_STOP); //stop counter task
			xEventGroupSetBits(ethernet_event_group, MODBUS_SLAVE_STOP); //stop modbus slave

			/* Ensure to disable any WiFi power save mode, this allows best throughput
			 * and hence timings for overall OTA operation.
			 */
			if(settings.connection_mode == WIFI_MODE)
				esp_wifi_set_ps(WIFI_PS_NONE);

			vTaskDelay(1000 / portTICK_PERIOD_MS);

			if(strstr(update_url, "/storage") != NULL) //if /storage is URL, then it is spiffs upgrade (https://10.96.7.90:8070/storage.bin)
				xTaskCreate(&ota_spiffs_task, "ota_spiffs_task", 8192, NULL, 24, NULL);
			else
				xTaskCreate(&simple_ota_example_task, "ota_app_task", 8192, NULL, 24, NULL);
		}
		else
		{
			MY_LOGE(TAG, "Cert size too short:%d", strlen(cert_pem));
			statistics.errors++;
			status.upgrade.status = CERT_FILE_TOO_SHORT;
			goto upgrade_failed;
		}
	}
	else
	{
		if(strlen(cert_filename) < 5)
		{
			MY_LOGE(TAG, "Certificate filename \"%s\" too short:%d", cert_filename, strlen(cert_filename));
			statistics.errors++;
			status.upgrade.status = CERT_NAME_TOO_SHORT;
		}
		if(strlen(update_url) < 10)
		{
			MY_LOGE(TAG, "URL \"%s\" too short:%d", update_url, strlen(update_url));
			statistics.errors++;
			status.upgrade.status = URL_TOO_SHORT;
		}
		goto upgrade_failed;
	}

	/* Redirect onto root to prevent update loop */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
	//ret = httpd_resp_sendstr(req, "Starting OTA...");
	ret = httpd_resp_sendstr(req, "<head> <meta http-equiv=\"refresh\" content=\"0;url=/\">	</head>");
	if (ret != ESP_OK)
		MY_LOGE(TAG, "%s httpd_resp_sendstr:%d", __FUNCTION__, ret);

	MY_LOGI(TAG, "%s finished", __FUNCTION__);
	return ESP_OK;

	upgrade_failed:
	status.upgrade.start = time(NULL);
	status.upgrade.end = status.upgrade.start;
	save_i32_key_nvs("upgrade_start", status.upgrade.start);
	save_i32_key_nvs("upgrade_end", status.upgrade.end);
	save_i8_key_nvs("upgrade_status", status.upgrade.status);
	strcpy(status.app_status, "Normal");

	return ESP_FAIL;
}

#if 0 //zurb
/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
#endif

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *getStatusJSON()
{
	char *string = NULL;
	char temp_str[25];

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//add statuses
	cJSON_AddItemToObject(json_root, "description", cJSON_CreateString(settings.description));
	cJSON_AddItemToObject(json_root, "location", cJSON_CreateString(settings.location));
	cJSON_AddItemToObject(json_root, "serial_number", cJSON_CreateString(factory_settings.serial_number));
	cJSON_AddItemToObject(json_root, "model_type", cJSON_CreateString(factory_settings.model_type));
	cJSON_AddItemToObject(json_root, "hw_ver", cJSON_CreateNumber(factory_settings.hw_ver+'A'));
	cJSON_AddItemToObject(json_root, "eeprom_size", cJSON_CreateNumber(eeprom_size/8));
	cJSON_AddItemToObject(json_root, "connection_mode", cJSON_CreateString(settings.connection_mode==ETHERNET_MODE?"Ethernet":"WiFi"));
	if(settings.connection_mode!=ETHERNET_MODE)
	{
		cJSON_AddItemToObject(json_root, "ssid", cJSON_CreateString(settings.wifi_ssid));
		cJSON_AddItemToObject(json_root, "wifi_rssi", cJSON_CreateNumber(status.wifi_rssi));
	}
	cJSON_AddItemToObject(json_root, "ip_address", cJSON_CreateString(status.ip_addr));
	sprintf(temp_str, "%02X:%02X:%02X:%02X:%02X:%02X",
			status.mac_addr[0], status.mac_addr[1], status.mac_addr[2], status.mac_addr[3], status.mac_addr[4], status.mac_addr[5]);
	cJSON_AddItemToObject(json_root, "mac_address", cJSON_CreateString(temp_str));
	cJSON_AddItemToObject(json_root, "sw_version", cJSON_CreateString(app_desc->version));
	cJSON_AddItemToObject(json_root, "fs_ver", cJSON_CreateString(status.fs_ver));
	cJSON_AddItemToObject(json_root, "local_time", cJSON_CreateString(status.local_time));
	cJSON_AddItemToObject(json_root, "timestamp", cJSON_CreateNumber(status.timestamp));
	printUptime(status.uptime, temp_str);
	cJSON_AddItemToObject(json_root, "uptime", cJSON_CreateString(temp_str));
	cJSON_AddItemToObject(json_root, "app_status", cJSON_CreateString(status.app_status));
	cJSON_AddItemToObject(json_root, "upgrade_status", cJSON_CreateString(upgradeStatusStr[status.upgrade.status]));
	cJSON_AddItemToObject(json_root, "flash_size", cJSON_CreateNumber(spi_flash_get_chip_size()/ (1024 * 1024)));
	sprintf(temp_str, "0x%04X", status.settings_crc);
	cJSON_AddItemToObject(json_root, "CRC", cJSON_CreateString(temp_str));
	sprintf(temp_str, "0x%04X", status.nvs_settings_crc);
	cJSON_AddItemToObject(json_root, "NVS_CRC", cJSON_CreateString(temp_str));
	sprintf(temp_str, "0x%08X", status.error_flags);
	cJSON_AddItemToObject(json_root, "Error_flags", cJSON_CreateString(temp_str));
	cJSON_AddItemToObject(json_root, "Errors", cJSON_CreateNumber(status.errors));

	const char *mqtt_state_strings[] = {"Error", "Connected", "Disconnected", "Subscribed", "Unsubscribed", "Published", "Data Received", "Server cert missing" };
	if(settings.publish_server[0].push_protocol == MQTT_PROTOCOL)
		cJSON_AddItemToObject(json_root, "mqtt1_state", cJSON_CreateString(mqtt_state_strings[status.mqtt[0].state]));
	else
		cJSON_AddItemToObject(json_root, "mqtt1_state", cJSON_CreateString("Disabled"));
	if(settings.publish_server[1].push_protocol == MQTT_PROTOCOL)
		cJSON_AddItemToObject(json_root, "mqtt2_state", cJSON_CreateString(mqtt_state_strings[status.mqtt[1].state]));
	else
		cJSON_AddItemToObject(json_root, "mqtt2_state", cJSON_CreateString("Disabled"));

	cJSON_AddItemToObject(json_root, "bicom_device_count", cJSON_CreateNumber(status.bicom_device_count));
	cJSON_AddItemToObject(json_root, "measurement_device_count", cJSON_CreateNumber(status.measurement_device_count));

	cJSON_AddItemToObject(json_root, "dig_count", cJSON_CreateNumber(status.dig_in_count));
#ifdef ENABLE_PT1000
	if(settings.adc_enabled && status.pt1000_temp > -100)
		sprintf(temp_str, "%d.%d",status.pt1000_temp/10, abs(status.pt1000_temp%10));
	else
		sprintf(temp_str, "N/A");
	cJSON_AddItemToObject(json_root, "pt1000_temp", cJSON_CreateString(temp_str));
#else
	cJSON_AddItemToObject(json_root, "pt1000_temp", cJSON_CreateString("N/A"));
#endif

	cJSON_AddItemToObject(json_root, "heap", cJSON_CreateNumber(esp_get_free_heap_size()));
	cJSON_AddItemToObject(json_root, "heap_min", cJSON_CreateNumber(esp_get_minimum_free_heap_size()));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *getDevicesJSON()
{
	char *string = NULL;
	char temp_str[25];

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "total_meas", cJSON_CreateNumber(status.measurement_device_count));
	cJSON_AddItemToObject(json_root, "total_bicom", cJSON_CreateNumber(status.bicom_device_count));

#ifdef ENABLE_RS485_PORT
	uint8_t i;
	for(i = 0; i < RS485_DEVICES_NUM; i++)
	{
		if(settings.rs485[i].type > 0)
		{
			sprintf(temp_str, "rs485_description_%d", i + 1);
			cJSON_AddItemToObject(json_root, temp_str, cJSON_CreateString(settings.rs485[i].description));
		}
	}
#endif

	for(i = 0; i < RS485_DEVICES_NUM + 2; i++)
	{
		char temp_serial[20];
		char temp_addr[20];
		uint8_t modbus_address;
		if(i == 0)//left IR
		{
			sprintf(temp_str, "device_lir_model");
			sprintf(temp_serial, "device_lir_serial");
			sprintf(temp_addr, "device_lir_addr");
			modbus_address = settings.ir_counter_addr;
		}
		else if(i == 1)//right IR
		{
			sprintf(temp_str, "device_rir_model");
			sprintf(temp_serial, "device_rir_serial");
			sprintf(temp_addr, "device_rir_addr");
			modbus_address = 0;
		}
		else //RS485 devices
		{
			sprintf(temp_str, "device_%d_model", i - 1);//RS485 devices starts with index 1
			sprintf(temp_serial, "device_%d_serial", i - 1);
			sprintf(temp_addr, "device_%d_addr", i - 1);
			modbus_address = settings.rs485[i - 2].address;
		}
		cJSON_AddItemToObject(json_root, temp_str, cJSON_CreateString(status.detected_devices[i].model));
		cJSON_AddItemToObject(json_root, temp_serial, cJSON_CreateString(status.detected_devices[i].serial));
		cJSON_AddItemToObject(json_root, temp_addr, cJSON_CreateNumber(modbus_address));
	}

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *getBicomsJSON()
{
	char *string = NULL;
	char temp_str[30];

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	cJSON_AddItemToObject(json_root, "bicoms total", cJSON_CreateNumber(status.bicom_device_count));

//#ifdef ENABLE_RIGHT_IR_PORT
//	get_ir_bicom_state();
//#else
//	cJSON_AddItemToObject(json_root, "ir_bicom_description", cJSON_CreateString("N/A"));
//#endif
	//always send ir bicom status (web page)
	get_ir_bicom_state();
	cJSON_AddItemToObject(json_root, "ir_bicom_state", cJSON_CreateNumber(status.bicom_state));

//#ifdef ENABLE_RS485_PORT
	uint8_t i;
	int web_index = 1; //on web site 485 bicom starts with index 1
	for(i = 0; i < status.bicom_device_count; i++)
	{
		if(status.bicom_device[i].uart_port == RIGHT_IR_UART_PORT)//IR bicom
		{
			cJSON_AddItemToObject(json_root, "ir_bicom_description", cJSON_CreateString(settings.ir_relay_description));
		}
		else if(settings.rs485[status.bicom_device[i].index].type == RS485_BISTABILE_SWITCH)
		{
			sprintf(temp_str, "bicom_485_%d_index", web_index);
			cJSON_AddItemToObject(json_root, temp_str, cJSON_CreateNumber(status.bicom_device[i].index + 1));
			sprintf(temp_str, "bicom_485_%d_state", web_index);
			status.bicom_device[i].status = get_bicom_485_state(status.bicom_device[i].index);
			cJSON_AddItemToObject(json_root, temp_str, cJSON_CreateNumber(status.bicom_device[i].status));
			sprintf(temp_str, "rs485_description_%d", web_index);
			cJSON_AddItemToObject(json_root, temp_str, cJSON_CreateString(settings.rs485[status.bicom_device[i].index].description));
			vTaskDelay(100 / portTICK_PERIOD_MS);
			web_index++;
		}
	}
//#endif

	cJSON_AddItemToObject(json_root, "local_time", cJSON_CreateString(status.local_time));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print status JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

char *getStatisticsJSON()
{
	char temp_str[25];
	char *string = NULL;

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//add statuses
	printUptime(status.uptime, temp_str);
	cJSON_AddItemToObject(json_root, "uptime", cJSON_CreateString(temp_str));

	cJSON_AddItemToObject(json_root, "cpu_usage", cJSON_CreateNumber(status.cpu_usage));
	cJSON_AddItemToObject(json_root, "heap", cJSON_CreateNumber(esp_get_free_heap_size()));
	cJSON_AddItemToObject(json_root, "heap_min", cJSON_CreateNumber(esp_get_minimum_free_heap_size()));
	cJSON_AddItemToObject(json_root, "errors", cJSON_CreateNumber(statistics.errors));
	cJSON_AddItemToObject(json_root, "http_requests", cJSON_CreateNumber(statistics.http_requests));
	cJSON_AddItemToObject(json_root, "mqtt_publish", cJSON_CreateNumber(statistics.mqtt_publish));
	cJSON_AddItemToObject(json_root, "push", cJSON_CreateNumber(statistics.push));
	cJSON_AddItemToObject(json_root, "push_errors", cJSON_CreateNumber(statistics.push_errors));
	cJSON_AddItemToObject(json_root, "push_ack", cJSON_CreateNumber(statistics.push_ack));
	cJSON_AddItemToObject(json_root, "modbus_slave", cJSON_CreateNumber(statistics.modbus_slave));
	cJSON_AddItemToObject(json_root, "tcp_rx_packets", cJSON_CreateNumber(statistics.tcp_rx_packets));
	cJSON_AddItemToObject(json_root, "tcp_tx_packets", cJSON_CreateNumber(statistics.tcp_tx_packets));
	cJSON_AddItemToObject(json_root, "rs485_rx_packets", cJSON_CreateNumber(statistics.rs485_rx_packets));
	cJSON_AddItemToObject(json_root, "rs485_tx_packets", cJSON_CreateNumber(statistics.rs485_tx_packets));
	cJSON_AddItemToObject(json_root, "left_ir_rx_packets", cJSON_CreateNumber(statistics.left_ir_rx_packets));
	cJSON_AddItemToObject(json_root, "left_ir_tx_packets", cJSON_CreateNumber(statistics.left_ir_tx_packets));
	cJSON_AddItemToObject(json_root, "right_ir_rx_packets", cJSON_CreateNumber(statistics.right_ir_rx_packets));
	cJSON_AddItemToObject(json_root, "right_ir_tx_packets", cJSON_CreateNumber(statistics.right_ir_tx_packets));
	cJSON_AddItemToObject(json_root, "local_time", cJSON_CreateString(status.local_time));
	cJSON_AddItemToObject(json_root, "timestamp", cJSON_CreateNumber(status.timestamp));

#ifdef EEPROM_STORAGE
	cJSON_AddItemToObject(json_root, "power_up_counter", cJSON_CreateNumber(status.power_up_counter));
	cJSON_AddItemToObject(json_root, "upgrade_counter", cJSON_CreateNumber(status.upgrade.count));
	cJSON_AddItemToObject(json_root, "eeprom_errors", cJSON_CreateNumber(statistics.eeprom_errors));
#endif

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print statistics JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

static void send_status_JSON(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = getStatusJSON();
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_status_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}

static void send_settings_JSON(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = get_settings_json();
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_settings_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	if(msg != NULL)
		free(msg);
}

static void send_devices_JSON(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = getDevicesJSON();
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_devices_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	if(msg != NULL)
		free(msg);
}

static void send_bicoms_JSON(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = getBicomsJSON();
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_bicoms_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	if(msg != NULL)
		free(msg);
}

static void send_counters_JSON(httpd_req_t *req, int uart_port, int modbus_address, int number)
{
//	if(active_uart_port == -1 || active_modbus_address == -1)
//	{
//		MY_LOGW(TAG, "No active port for modbus readings");
//		return;
//	}

	httpd_resp_set_type(req, "application/json");

	char *msg = getEnergyCountersJSON(uart_port, modbus_address, number);
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_counters_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	if(msg != NULL)
		free(msg);
}

static void send_measurements_JSON(httpd_req_t *req, int uart_port, int modbus_address, int number)
{
//	if(active_uart_port == -1 || active_modbus_address == -1)
//	{
//		MY_LOGW(TAG, "No active port for modbus readings");
//		return;
//	}

	httpd_resp_set_type(req, "application/json");

	char *msg = getMeasurementsJSON(uart_port, modbus_address, number);
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_measurements_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}

static void set_settings_JSON(httpd_req_t *req, char*  buf, size_t buf_len)
{
	int i;
	//char param[100];
	char decoded[100];

	for(i = 0; nvs_settings_array[i].name[0] != 0; i++) {
		if (httpd_query_key_value(buf, nvs_settings_array[i].name, param, sizeof(param)) == ESP_OK) {
			MY_LOGI(TAG, "%s URL query parameter => %s=%s", __FUNCTION__, nvs_settings_array[i].name, param);
			if(nvs_settings_array[i].type == NVS_STRING)
			{
				url_decode(param, decoded); //strings are url encoded
				strcpy(nvs_settings_array[i].value, decoded);
			}
			else if(nvs_settings_array[i].type == NVS_INT8)
				*(int8_t *)nvs_settings_array[i].value = atoi(param);
			else if(nvs_settings_array[i].type == NVS_UINT8)
				*(uint8_t *)nvs_settings_array[i].value = atoi(param);
			else if(nvs_settings_array[i].type == NVS_INT16)
				*(int16_t *)nvs_settings_array[i].value = atoi(param);
			else if(nvs_settings_array[i].type == NVS_UINT16)
				*(uint16_t *)nvs_settings_array[i].value = atoi(param);
			else if(nvs_settings_array[i].type == NVS_INT32)
				*(int32_t *)nvs_settings_array[i].value = atoi(param);
		}
	}//for

	//debug_console is not part of settings
	if (httpd_query_key_value(buf, "debug_console", param, sizeof(param)) == ESP_OK) {
		MY_LOGI(TAG, "%s URL query parameter => %s=%s", __FUNCTION__, "debug_console", param);
		debug_console = atoi(param) + DEBUG_CONSOLE_INTERNAL;//add offset
		save_i8_key_nvs("debug_console", debug_console); //save debug_console setting
	}

	httpd_resp_set_status(req, "204 No Content");
	httpd_resp_send(req, NULL, 0);  // Response body can be empty

	err_t err = save_settings_nvs(RESTART);
	if(err != ESP_OK)
	{
		MY_LOGE(TAG, "Save settings Failed:%d", err);
		statistics.errors++;
	}
}

static void send_statistics_JSON(httpd_req_t *req)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = getStatisticsJSON();
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "send_statistics_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}

#ifdef EEPROM_STORAGE
static void energy_log_JSON(httpd_req_t *req, int recorder)
{
	if(settings.energy_log[recorder] == 0 || eeprom_size == 0)//disabled
		return;

	if(recorder > 0 && eeprom_size == 16)
			return;

	httpd_resp_set_type(req, "application/json");
	char *msg = EE_read_energy_day_records_JSON(recorder);
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "energy_log_JSON msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}
#endif //#ifdef EEPROM_STORAGE

static void get_cert_files(httpd_req_t *req, char*  buf, size_t buf_len)
{
	httpd_resp_set_type(req, "application/json");
	char *msg = get_spiffs_files_json(".pem");
	if(msg == NULL)
		return;

	if(strlen(msg) > JSON_SETTINGS_SIZE)
	{
		MY_LOGE(TAG, "msg size: %d > %d", strlen(msg), JSON_SETTINGS_SIZE);
		statistics.errors++;
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Size too long");
	}
	else
		httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}

static esp_err_t get_command_handler(httpd_req_t *req)
{
	char*  buf;
	size_t buf_len;

	statistics.http_requests++;

	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		if (buf == NULL)
		{
			MY_LOGE(TAG, "Malloc failed");
			statistics.errors++;
			return ESP_ERR_NO_MEM;
		}

		//prevent nesting of requests
		if(get_commands_count++ > 10)
		{
			MY_LOGE(TAG, "Too many requests: %d", get_commands_count);
			statistics.errors++;
			get_commands_count--;
			free(buf);
			return ESP_ERR_NO_MEM;
		}

		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
			MY_LOGD(TAG, "%s Found URL query => %s cnt:%d", __FUNCTION__, buf, get_commands_count);
			//char param[100];
			if (httpd_query_key_value(buf, "command", param, sizeof(param)) == ESP_OK)
			{
				if(strcmp(param, "get_status") == 0)
					send_status_JSON(req);
				else if(strcmp(param, "get_settings") == 0)
					send_settings_JSON(req);
				else if(strcmp(param, "get_devices") == 0)
					send_devices_JSON(req);
				else if(strcmp(param, "get_bicoms") == 0)
					send_bicoms_JSON(req);
				else if((strcmp(param, "get_measurements") == 0 || strcmp(param, "get_counters") == 0 )
						&& status.modbus_device[0].uart_port >= 0)
				{
					int number = 0; //default
					int measurements = 1; //measurements command
					if(strcmp(param, "get_counters") == 0)
						measurements = 0; //counters command
					if (httpd_query_key_value(buf, "number", param, sizeof(param)) == ESP_OK)
					{
						number = atoi(param);
						if(number < 0 || number > status.measurement_device_count || status.modbus_device[number].uart_port < 0)
						{
							MY_LOGE(TAG, "%s number: %d out of range or device not available", __FUNCTION__, number);
							send_error_JSON(req, param);//json with model = 0 for empty web page
							number = 0; //set to default and send measurements of 0
						}
					}
					else if (httpd_query_key_value(buf, "index", param, sizeof(param)) == ESP_OK)
					{
						int i_index = atoi(param);
						if(i_index > 0)
						{
							number = getNumberFromIndex(i_index - 1);
							if(number < 0)
							{
								//MY_LOGE(TAG, "%s number: %d out of range or device not available", __FUNCTION__, number);
								send_error_JSON(req, param);//json with model = 0 for empty web page
								number = 0; //set to default and send measurements of 0
							}
						}
					}

					if(measurements)
						send_measurements_JSON(req, status.modbus_device[number].uart_port, status.modbus_device[number].address, number);
					else
						send_counters_JSON(req, status.modbus_device[number].uart_port, status.modbus_device[number].address, number);
				}

				else if(strcmp(param, "set_settings") == 0)
					set_settings_JSON(req, buf, buf_len);
				else if(strcmp(param, "get_statistics") == 0)
					send_statistics_JSON(req);
				else if(strcmp(param, "ota_upgrade") == 0)
					ota_handler(req, buf, buf_len);
				else if(strcmp(param, "get_cert_files") == 0)
					get_cert_files(req, buf, buf_len);
#ifdef EEPROM_STORAGE
				else if(strcmp(param, "get_energy_log") == 0)
				{
					int recorder = 0;
					int recorder_error = 0;
					if (httpd_query_key_value(buf, "recorder", param, sizeof(param)) == ESP_OK)
					{
						recorder = atoi(param);
						if(recorder < 0 || recorder > NUMBER_OF_RECORDERS - 1 || settings.energy_log[recorder] == 0)
						{
							MY_LOGE(TAG, "%s recorder: %d or disabled out of range", __FUNCTION__, recorder);
							send_error_JSON(req, param);//json with model = 0 for empty web page
							recorder = 0; //set to default and send measurements of 0
							recorder_error++;
						}
					}
					if(recorder_error == 0)
						energy_log_JSON(req, recorder);
				}
#endif
				else
				{
					send_error_JSON(req, param);//json with model = 0 for empty web page
				}
			}
		}
		free(buf);
		get_commands_count--;
	}

	return ESP_OK;
}

/* Handler for POST commands */
static esp_err_t post_command_handler(httpd_req_t *req)
{
	//statistics.http_requests++;

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((struct file_server_data *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    MY_LOGI(TAG, "command: %s", buf);

    //char param[100];
    if (httpd_query_key_value(buf, "command", param, sizeof(param)) == ESP_OK)
    {
    	MY_LOGI(TAG, "%s URL query parameter => %s=%s", __FUNCTION__, "command", param);
#ifdef ENABLE_RIGHT_IR_PORT
    	if(strcmp(param, "TOGGLE") == 0)
    		bicom_toggle();
    	if(strcmp(param, "ON") == 0)
    		set_ir_bicom_state(1);
    	if(strcmp(param, "OFF") == 0)
    		set_ir_bicom_state(0);
#endif
#ifdef BICOM_RS485_ENABLED
    	int rs485_bicom_index = 0;
    	if(status.bicom_device[0].uart_port == RIGHT_IR_UART_PORT)
    		rs485_bicom_index = 1; //rs485 bicoms are after ir bicom
    	if(strcmp(param, "485_1_TOGGLE") == 0)
    		bicom_485_toggle(status.bicom_device[rs485_bicom_index].index);
    	if(strcmp(param, "485_1_ON") == 0)
    		set_bicom_485_device_state(status.bicom_device[rs485_bicom_index].index, 1);
    	if(strcmp(param, "485_1_OFF") == 0)
    		set_bicom_485_device_state(status.bicom_device[rs485_bicom_index].index, 0);
    	if(strcmp(param, "485_2_TOGGLE") == 0)
    		bicom_485_toggle(status.bicom_device[rs485_bicom_index + 1].index);
    	if(strcmp(param, "485_2_ON") == 0)
    		set_bicom_485_device_state(status.bicom_device[rs485_bicom_index + 1].index, 1);
    	if(strcmp(param, "485_2_OFF") == 0)
    		set_bicom_485_device_state(status.bicom_device[rs485_bicom_index + 1].index, 0);
#endif
    }

    //HTTP.send(204, "text/plain", "");    //send response to POST, so we dont need redirection
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty

    return ESP_OK;
}

#if 0
static esp_err_t measurements_handler(httpd_req_t *req)
{
    //char resp[] = { "{\"Unit\":\"WTS00011\", \"Type\":\"WTS100\", \"Temperature\":\"26.7\"}" };
    //{"Unit":"WTS00011","Type":"WTS100","Location":"Razvoj 3","Description":"Kranj","Temperature":"26.7","Temperature_min":"24.0","Temperature_max":"26.6","Humidity":"21","Wifi_signal_level":"-77","Interval":"600","Timestamp":"1553690008","Uptime":"0d19h25'"}
    //MY_LOGI(TAG, "JSON: %s", resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, measurements_json, strlen(measurements_json));                  
    return ESP_OK;
}
#endif

/* Function to start the file server */
httpd_handle_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;
    esp_err_t ret;

    /* Validate file storage base path */
    if (!base_path || strcmp(base_path, "/spiffs") != 0) {
        MY_LOGE(TAG, "File server presently supports only '/spiffs' as base path");
        return NULL; //return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        MY_LOGE(TAG, "File server already started");
        return NULL; //return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        MY_LOGE(TAG, "Failed to allocate memory for server data");
        return NULL; //return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    /* first configure http */            
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = settings.http_port;
    config.task_priority = tskIDLE_PRIORITY+8;
    //config.recv_wait_timeout = 30; //10.6.2020: try to fix httpd hang
    config.lru_purge_enable = true;//todo probaj še ta fix

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    config.stack_size = HTTPD_STACK_SIZE;

#ifdef SECURE_HTTP_SERVER
    /* https config */
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();                    

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    
    conf.httpd = config; //Underlying HTTPD server config
    
    // Start the httpd server
    MY_LOGI(TAG, "Starting HTTPS server");

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        MY_LOGI(TAG, "Error starting server!");
        statistics.errors++;
        return NULL;
    }
#else    
    MY_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        MY_LOGE(TAG, "Failed to start file server!");
        statistics.errors++;
        return NULL;
    }
#endif //secure    

#if 0 //zurb
    /* Register handler for index.html which should redirect to / */
    httpd_uri_t index_html = {
        //.uri       = "/index.html",
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_html_get_handler,
        .user_ctx  = NULL
    };
    ret = httpd_register_uri_handler(server, &index_html);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }
#endif

    /* Handler for wifi_data URI */
    httpd_uri_t get_command = {
        .uri       = "/get_command",
        .method    = HTTP_GET,
        .handler   = get_command_handler,
        .user_ctx  = NULL
    };
    ret = httpd_register_uri_handler(server, &get_command);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }
#if 1
    /* URI handler for graph/1.svg */
    httpd_uri_t drawGraph = {
    		.uri       = "/graph/*",
			.method    = HTTP_GET,
        .handler   = drawGraph_handler,
		.user_ctx  = NULL
    };
    ret = httpd_register_uri_handler(server, &drawGraph);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }
#endif

    /* URI handler for post commands */
    httpd_uri_t post_command = {
            .uri       = "/post_command",
            .method    = HTTP_POST,
            .handler   = post_command_handler,
            .user_ctx  = server_data
        };
    ret = httpd_register_uri_handler(server, &post_command);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
    		.uri       = "/*",  // Match all URIs of type /path/to/file (except index.html)
			.method    = HTTP_GET,
			.handler   = download_get_handler,
			.user_ctx  = server_data    // Pass server data as context
    };
    ret = httpd_register_uri_handler(server, &file_download);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }

    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    ret = httpd_register_uri_handler(server, &file_upload);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    ret = httpd_register_uri_handler(server, &file_delete);
    if (ret != ESP_OK) {
    	MY_LOGE(TAG, "Failed to register uri handler (%s)", esp_err_to_name(ret));
    	//return ESP_FAIL;
    }

	return server;
}

static void http_server_socket_thread(void *arg)
{
	static httpd_handle_t server = NULL;

	MY_LOGI(TAG,"http_server task started \n");

	//----- WAIT FOR IP ADDRESS -----
	MY_LOGI(TAG, "waiting for SYSTEM_EVENT_ETH_GOT_IP ...");
	xEventGroupWaitBits(ethernet_event_group, GOT_IP_BIT, false, true, portMAX_DELAY);
	MY_LOGI(TAG, "SYSTEM_EVENT_ETH_GOT_IP !!!!!!!!!");

	while(1)
	{
		EventBits_t uxBits;

		/* Start the web server */
		if (server == NULL) {
			server = start_file_server("/spiffs");
			MY_LOGI(TAG, "restart file server!");
		}
		//vTaskDelay(10 / portTICK_PERIOD_MS);

		uxBits = xEventGroupWaitBits(ethernet_event_group, HTTP_STOP, false, true, 100 / portTICK_PERIOD_MS);
		if( ( uxBits & HTTP_STOP ) == HTTP_STOP )
		{
			MY_LOGI(TAG, "STOP_HTTP_SERVER!");
			if (server) {
				/* Stop the httpd server */
				httpd_stop(server);
			}

			vTaskDelete( NULL );
		}
	}
}

void http_server_init()
{
  sys_thread_t ret;

  ret = sys_thread_new("HTTP", http_server_socket_thread, NULL, WEBSERVER_THREAD_STACKSIZE, WEBSERVER_THREAD_PRIO);
  if(ret == NULL)
    MY_LOGI(TAG,"*** ERROR HTTP Server start failed ***\n\r");
}

static char *getErrorJSON(char *command)
{
	char *string = NULL;
	cJSON *header = NULL;

	cJSON *json_root = cJSON_CreateObject();
	if (json_root == NULL)
	{
		goto end;
	}

	//create header object
	header = cJSON_CreateObject();
	if (header == NULL)
	{
		goto end;
	}
	cJSON_AddItemToObject(json_root, "header", header); //header name
	cJSON_AddItemToObject(header, "cmd", cJSON_CreateString(command));
	cJSON_AddItemToObject(header, "local_time", cJSON_CreateString(status.local_time));
	cJSON_AddItemToObject(header, "model", cJSON_CreateString("0"));

	string = cJSON_Print(json_root);
	if (string == NULL)
	{
		MY_LOGE(TAG, "Failed to print counters JSON");
		statistics.errors++;
	}

	end:
	cJSON_Delete(json_root);

	//MY_LOGI(TAG, "%s", string);
	return string;
}

static void send_error_JSON(httpd_req_t *req, char *command)
{
	httpd_resp_set_type(req, "application/json");

	char *msg = getErrorJSON(command);
	if(msg == NULL)
		return;
	httpd_resp_send(req, msg, strlen(msg));

	free(msg);
}

#endif //#ifdef CONFIG_WEB_SERVER_ENABLED
