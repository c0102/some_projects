#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "hts2.h"
#include "db_handler.h"
#include <cJSON.h>


char* extractHTS2ID(char* source){
	char *start = source;
	char *end = strchr(source,'/');
	if(end != NULL){

		size_t length =  end - start;
		char *destination = (char*) malloc(length+1);
		if(destination !=NULL){

			strncpy(destination,start, length);
			destination[length] = '\0';
		}
		else{

		fprintf(stderr, "Memory allocation failed\n");
		exit(EXIT_FAILURE);
		}


	return destination;
	}
	else{

		char *destination = (char*)malloc(1);
		if (destination != NULL) {
			destination[0] = '\0';
			return destination;
		}
		else {

			 fprintf(stderr, "Memory allocation failed\n");
			exit(EXIT_FAILURE);
					}

	}

}

int hts2_handle(char* topic, char* payload){

	if(strstr(topic,"events/rpc")!=NULL && strstr(topic, "read")==NULL){
                char* id = extractHTS2ID(topic);

		printf("\n#### %s ##### \n", "shellyPLUSHT");

		printf("\nid: %s\n", id);
		printf("\ndata: %s\n", payload);

		printf("\n#### %s ##### \n", "shellyPLUSHT");

		cJSON *root = cJSON_Parse(payload);

    		if (root == NULL) {
        	// Handle error, cJSON_Parse failed
        		printf("Error before: [%s]\n", cJSON_GetErrorPtr());
        		return 0;
    		}

    cJSON *methodNode = cJSON_GetObjectItemCaseSensitive(root, "method");
    char *methodValue =""; 
    if (cJSON_IsString(methodNode)) {
        methodValue = methodNode->valuestring;
        printf("Method: %s\n", methodValue);

    }

    if(strstr(methodValue,"NotifyFullStatus")){

// if notify //
    double temperature = 0.0;
    cJSON *temperatureNode = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "params"), "temperature:0");
    if (cJSON_IsObject(temperatureNode)) {
	temperature = cJSON_GetObjectItemCaseSensitive(temperatureNode, "tC")->valuedouble;
	printf("Temperature (Celsius): %lf\n", temperature);
	}

    double humidity = 0;
    cJSON *humidityNode = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "params"), "humidity:0");
    if (cJSON_IsObject(humidityNode)) {
        humidity = cJSON_GetObjectItemCaseSensitive(humidityNode, "rh")->valuedouble;
        printf("Humidity: %.1f\n", humidity);
    }


    int batteryPercent = 0;
    cJSON *batteryNode = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "params"), "devicepower:0");
    if (cJSON_IsObject(batteryNode)) {
        cJSON *batteryPercentNode = cJSON_GetObjectItemCaseSensitive(batteryNode, "battery");
        if (cJSON_IsObject(batteryPercentNode)) {
            batteryPercent = cJSON_GetObjectItemCaseSensitive(batteryPercentNode, "percent")->valueint;
            printf("Battery Percent: %d\n", batteryPercent);
        }
    }



const char* insert_data_sql_format = "INSERT INTO hts2 (id, temperature, humidity, battery) VALUES ('%s', %lf, %lf, %d);";

int size = snprintf(NULL, 0, insert_data_sql_format, id, temperature, humidity, batteryPercent);
char *insert_data_sql = malloc(size + 1);
snprintf(insert_data_sql, size+1, insert_data_sql_format, id, temperature, humidity, batteryPercent);


    hts2_t htsData = {
	    .tableSchema = "CREATE TABLE IF NOT EXISTS hts2 ("
		           "id TEXT,"
			   "timestamp DATE DEFAULT (datetime('now','localtime')),"
			   "temperature REAL,"
			   "humidity REAL,"
			   "battery INTEGER);",
	    .dataEntry = insert_data_sql,
	    .operation = 'W'
    };

        sqlite3* key = connect_open_db("myDB");
	        if (write_to_db(key, &htsData)==1){

		free(id);
		free(insert_data_sql);
		cJSON_Delete(root);
		printf("\n### free heap after writing####\n");

		return 1;
		}else{

                free(id);
                free(insert_data_sql);
                cJSON_Delete(root);

		return 0;
		}

// end if notify //
    }
free(id);
cJSON_Delete(root);

// end if topic//
}
	else if(strstr(topic, "read")!= NULL){
	char* id = extractHTS2ID(topic);
		cJSON *root = cJSON_Parse(payload);
    		if (root == NULL) {
        		const char *error_ptr = cJSON_GetErrorPtr();
        		if (error_ptr != NULL) {
            			fprintf(stderr, "Error before: %s\n", error_ptr);
        		}
        		// Handle parsing error
       			return 0;
    		}


		cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    		cJSON *start_time = cJSON_GetObjectItemCaseSensitive(root, "start_time");
    		cJSON *end_time = cJSON_GetObjectItemCaseSensitive(root, "end_time");

		if (cJSON_IsString(data) && cJSON_IsString(start_time) && cJSON_IsString(end_time)) {
        		// Print the parsed values
			const char *column = data->valuestring;
			const char *startDate = start_time->valuestring;
			const char *endDate = end_time->valuestring;

			char sql[300];  // Adjust the size based on your needs
			// Format the SQL string with dynamic column name and date values
			snprintf(sql, sizeof(sql), "SELECT %s,timestamp FROM hts2 WHERE timestamp BETWEEN '%s' AND '%s'", column, startDate, endDate);


    hts2_t htsData = {
	    .tableSchema = "CREATE TABLE IF NOT EXISTS hts2 ("
		           "id TEXT,"
			   "timestamp DATE DEFAULT (datetime('now','localtime')),"
			   "temperature REAL,"
			   "humidity REAL,"
			   "battery INTEGER);",
	    .dataEntry = sql,
	    .operation = 'R'
    };





     sqlite3* key = connect_open_db("myDB");
                if (write_to_db(key, &htsData)==1){
                     free(id); 
		    cJSON_Delete(root);
                printf("\n### free heap after reading ####\n");
	return 1;

    } else {
        fprintf(stderr, "Could not open and send data\n");
        	free(id);
		cJSON_Delete(root);
	return 0;
    }	}


else{
	fprintf(stderr, "HERE Invalid JSON format\n");

	free(id);
	cJSON_Delete(root);


		return 0;
	}

}


return 1;
}

