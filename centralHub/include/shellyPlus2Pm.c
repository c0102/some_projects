#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "shellyPlus2Pm.h"
#include "db_handler.h"
#include <cJSON.h>
#include <time.h>

char* extracShellyPlus2PmID(char* source){
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



int shellyPlus2Pm_handle(char* topic, char* payload){

    if((strstr(topic, "status/switch:0")!=NULL || strstr(topic, "status/switch:1")!=NULL) && strstr(topic, "read")==NULL){
        
        char* switchId = (strstr(topic, "status/switch:0")!=NULL)?"0":"1";

        char *id = extracShellyPlus2PmID(topic);
		printf("\n#### %s ##### \n", "shellyPLUS2PM");

        printf("\nid: %s\n", id);
		printf("\ndata: %s\n", payload);

		cJSON *root = cJSON_Parse(payload);

		if (root == NULL) {
			// Handle error, cJSON_Parse failed
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    		free(id);
			return 0;	
		}

		    // Extracting values
    	cJSON *output = cJSON_GetObjectItemCaseSensitive(root, "output");
    	cJSON *apower = cJSON_GetObjectItemCaseSensitive(root, "apower");
    	cJSON *voltage = cJSON_GetObjectItemCaseSensitive(root, "voltage");
    	cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");

    	cJSON *aenergy = cJSON_GetObjectItemCaseSensitive(root, "aenergy");
    	cJSON *total_energy = cJSON_GetObjectItemCaseSensitive(aenergy, "total");

    	cJSON *temperature = cJSON_GetObjectItemCaseSensitive(root, "temperature");
    	cJSON *tC = cJSON_GetObjectItemCaseSensitive(temperature, "tC");


		if (output && apower && voltage && current && total_energy && tC) {
        	// Print the extracted values
        	char* state = cJSON_IsTrue(output) ? "true" : "false";
        	double activePower =  apower->valuedouble;
        	double vol = voltage->valuedouble;
        	double curr = current->valuedouble;
        	double energy =  total_energy->valuedouble;
			double temp = tC->valuedouble;

			const char* insert_data_sql_format = "INSERT INTO shellyplus2pm (id, id_switch, state, active_power, voltage, current, energy, temperature) VALUES ('%s', '%s', '%s', '%lf', '%lf', '%lf', '%lf', '%lf');";
			int size = snprintf(NULL, 0, insert_data_sql_format, id, switchId ,state, activePower, vol, curr, energy, temp);
			char* insert_data_sql = malloc(size+1);
			snprintf(insert_data_sql, size+1, insert_data_sql_format, id, switchId, state, activePower, vol, curr, energy, temp);

			shellyPlus2Pm_t shellyplus2pmData = {
					.tableSchema = "CREATE TABLE IF NOT EXISTS shellyplus2pm("
			       "id TEXT,"
			       "timestamp DATE DEFAULT (datetime('now','localtime')),"
                   "id_switch TEXT,"
			       "state TEXT,"
			       "motion_time TEXT,"
			       "active_power REAL,"
			       "voltage REAL,"
			       "current REAL,"
			       "energy REAL,"
			       "temperature REAL);",

					.dataEntry = insert_data_sql,
					.operation = 'W'
			};

			sqlite3* key = connect_open_db("myDB");
			if (write_to_db(key, &shellyplus2pmData)==1){

			free(id);
			free(insert_data_sql);
			printf("\n### free heap after writing in db motion ####\n");
			cJSON_Delete(root);
			return 1;
			}else{

				free(id);
				free(insert_data_sql);
				printf("\n### free heap after writing in db motion ####\n");
				cJSON_Delete(root);
				return 0;
			}
		
		} 	
		else {
        	printf("Error extracting values from JSON\n");
			cJSON_Delete(root);
			free(id);
			return 0;
    	}

    // Clean up cJSON resources
    }else if(strstr(topic, "read")!= NULL){

        char* switchId = (strstr(topic, "read/switch:0")!=NULL)?"0":"1";

		char* id = extracShellyPlus2PmID(topic);
		cJSON *root = cJSON_Parse(payload);
		if (root == NULL) {
			const char *error_ptr = cJSON_GetErrorPtr();
			if (error_ptr != NULL) {
				fprintf(stderr, "Error before: %s\n", error_ptr);
			}
			free(id);
			// Handle parsing error
			return 0;
			}

		cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
		cJSON *start_time = cJSON_GetObjectItemCaseSensitive(root, "start_time");
		cJSON *end_time = cJSON_GetObjectItemCaseSensitive(root, "end_time");

		if (cJSON_IsString(data) && cJSON_IsString(start_time) && cJSON_IsString(end_time)) {
			const char *column = data->valuestring;
			const char *startDate = start_time->valuestring;
			const char *endDate = end_time->valuestring;
			char sql[300];  // Adjust the size based on your needs
			snprintf(sql, sizeof(sql), "SELECT id_switch, %s,timestamp FROM shellyplus2pm WHERE id_switch='%s' AND timestamp BETWEEN '%s' AND '%s'", column, switchId, startDate, endDate);

			shellyPlus2Pm_t shellyplus2pmData = {
					.tableSchema = "CREATE TABLE IF NOT EXISTS shellyplus2pm("
			       "id TEXT,"
			       "timestamp DATE DEFAULT (datetime('now','localtime')),"
                   "id_switch TEXT,"
			       "state TEXT,"
			       "motion_time TEXT,"
			       "active_power REAL,"
			       "voltage REAL,"
			       "current REAL,"
			       "energy REAL,"
			       "temperature REAL);",

					.dataEntry = sql,
					.operation = 'R'
			};


			sqlite3* key = connect_open_db("myDB");
			if (write_to_db(key, &shellyplus2pmData)==1){
            	free(id); 
		    	cJSON_Delete(root);
                printf("\n### free heap after reading ####\n");
				return 1;
				} 
			else {
        		fprintf(stderr, "Could not open and send data\n");
        		free(id);
				cJSON_Delete(root);
	return 0;
    }	

		}

	}
}



