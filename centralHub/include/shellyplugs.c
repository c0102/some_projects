#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "shellyplugs.h"
#include "db_handler.h"
#include "cJSON.h"

char* extractShellyplugsID(const char* source){

	const char *start = strchr(source, '/');
	const char *end = strchr(start + 1, '/');
	
	if(start != NULL && end != NULL){
		size_t length = end - (start + 1);
		char *destination = (char*)malloc(length + 1);
		if (destination != NULL) {
 			strncpy(destination, start + 1, length);
			destination[length] = '\0';
		
		}else{

			fprintf(stderr, "Memory allocation failed\n");
			exit(EXIT_FAILURE);	    
		
		}

		return destination;
	}else{
	
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

int inserInDB(char* id, char* property, char* value){

const char* insert_data_sql_format = "INSERT INTO shellyplugs (id, property, value) VALUES ('%s', '%s', '%s');";
        int size = snprintf(NULL, 0, insert_data_sql_format, id, property, value);
        char *insert_data_sql = malloc(size + 1);
        snprintf(insert_data_sql, size+1, insert_data_sql_format, id, property, value);

        		shellyplugs_t shellyplugsData = {		
		.tableSchema = "CREATE TABLE IF NOT EXISTS shellyplugs ("
			       "id TEXT,"
			       "timestamp DATE DEFAULT (datetime('now','localtime')),"
			       "property TEXT,"
			       "value TEXT);",
		.dataEntry = insert_data_sql,
		.operation = 'W'		
		};

        sqlite3* key = connect_open_db("myDB");
	        if (write_to_db(key, &shellyplugsData)==1){
		free(insert_data_sql);
		printf("\n### free heap after writing####\n");
        return 1;
		}else{
            return 0;
        }

}




int shellyplugs_handle(char* topic , char* payload){

    if(strstr(topic,"/relay/0/power")!=NULL){
        char *property = "power";
        char* value = payload;
        char *id = extractShellyplugsID(topic);
        printf("\nid: %s\n", id);
		printf("\npower: %s\n", payload);

        if(inserInDB(id, property, value) == 1){

            printf("\n%s\n","--SUCCESS shellyplug---");
        }
        else{

            printf("\n%s\n","--ERROR shellyplug---");
        }

		free(id);

    }
    else if(strstr(topic,"/relay/0/energy")!=NULL){
        char *property = "energy";
        char* value = payload;
        char *id = extractShellyplugsID(topic);
        printf("\nid: %s\n", id);
		printf("\nenergy: %s\n", payload);
        inserInDB(id, property, value);
        free(id);

    }
    else if(strstr(topic,"/relay/0")!=NULL){
        char *property = "state";
        char* value = payload;
        char *id = extractShellyplugsID(topic);
        printf("\nid: %s\n", id);
		printf("\nstate: %s\n", payload);
        inserInDB(id, property, value);
        free(id);
    }
        else if(strstr(topic,"/temperature")!=NULL){
        char *property = "internalTemp";
        char* value = payload;
        char *id = extractShellyplugsID(topic);
        printf("\nid: %s\n", id);
		printf("\ninternatlTemp: %s\n", payload);
        inserInDB(id, property, value);
        free(id);

    }
        else if(strstr(topic,"/overtemperature")!=NULL){
        char *property = "overTemp";
        char* value = payload;
        char *id = extractShellyplugsID(topic);
        printf("\nid: %s\n", id);
		printf("\nOverTemp: %s\n", payload);
        inserInDB(id, property, value);
        free(id);

    }
    else if(strstr(topic,"/read")!=NULL){
    char *id = extractShellyplugsID(topic);
    
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
	
	char sql[300];
	snprintf(sql, sizeof(sql), "SELECT value,timestamp FROM shellyplugs WHERE property='%s' AND timestamp BETWEEN '%s' AND '%s'", column, startDate, endDate);

            		shellyplugs_t shellyplugsData = {		
		.tableSchema = "CREATE TABLE IF NOT EXISTS shellyplugs ("
			       "id TEXT,"
			       "timestamp DATE DEFAULT (datetime('now','localtime')),"
			       "property TEXT,"
			       "value TEXT);",
		.dataEntry = sql,
		.operation = 'R'		
		};

             sqlite3* key = connect_open_db("myDB");
                if (write_to_db(key, &shellyplugsData)==1){
                     free(id); 
			        cJSON_Delete(root);
                printf("\n### free heap after reading ####\n");
                return 1;

    } else {
        fprintf(stderr, "Could not open and send data\n");
        free(id); 
        cJSON_Delete(root);
        return 0;
    }
    
    }


    }



    return 1;
}





