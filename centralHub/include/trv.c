#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "trv.h"
#include "db_handler.h"
#include <cJSON.h>
char* extractTRVID(const char* source){

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






int trv_handle(char* topic, char* payload){
       
		char* id = extractTRVID(topic);
	if(strstr(topic, "info")!=NULL && (strstr(topic, "read") == NULL)){

		printf("\nid: %s\n", id);
		printf("\ndata: %s\n", payload);


// JSON PARSING //

		cJSON *js = cJSON_Parse(payload);
        if(js == NULL){
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
		printf("Error before: %s\n", error_ptr);
		}else {
		printf("%s","Error: cJSON_Parse failed.\n");
		}
		printf("%s","\n\nnot working\n\n");
		return 0;
		  }

 // Parse thermostats array
// We'll be reading the pos and tmp value from sensor
//
double TRVpos = 0;
double trvSensTemp = 0.0;
double batLvl = 0.0;
char *calibrated = "false";

cJSON *thermostatsArray = cJSON_GetObjectItem(js,"thermostats");
cJSON *bat = cJSON_GetObjectItem(js,"bat");
cJSON *cal = cJSON_GetObjectItem(js,"calibrated");
if (thermostatsArray != NULL && cJSON_IsArray(thermostatsArray) && bat !=NULL && cal !=NULL) {
	cJSON* level = cJSON_GetObjectItem(bat, "value");
	if(level !=NULL && cJSON_IsNumber(level)){
		batLvl = level->valuedouble;
		calibrated = cJSON_IsTrue(cal)?"true":"false";

	}

int numThermostats = cJSON_GetArraySize(thermostatsArray);
for (int i = 0; i < numThermostats; ++i) {
	cJSON *thermostat = cJSON_GetArrayItem(thermostatsArray, i);

	cJSON *pos = cJSON_GetObjectItem(thermostat, "pos");
	cJSON *tmp = cJSON_GetObjectItem(thermostat, "tmp");
	

	if (pos != NULL && cJSON_IsNumber(pos) &&
	tmp != NULL && cJSON_IsObject(tmp)) {

	cJSON *tmpValue = cJSON_GetObjectItem(tmp, "value");
        if (tmpValue != NULL && cJSON_IsNumber(tmpValue)) {
	 trvSensTemp = tmpValue->valuedouble;
	 }

	}

	TRVpos = pos->valuedouble;
}

printf("\n%s\n","__________");

printf("\n pos %lf \n", TRVpos);
printf("\n temp %lf  \n", trvSensTemp);


printf("\n%s\n","__________");

}
cJSON_Delete(js);
 // Parse thermostats array
 

//SQL SCHEMA and DATA entry

const char* insert_data_sql_format = "INSERT INTO trv (id, pos, iTemp, battery, calibrated) VALUES ('%s', %lf, %lf, %lf, '%s');";

int size = snprintf(NULL, 0, insert_data_sql_format, id, TRVpos, trvSensTemp, batLvl, calibrated);
char *insert_data_sql = malloc(size + 1);
snprintf(insert_data_sql, size+1, insert_data_sql_format, id, TRVpos, trvSensTemp, batLvl, calibrated);

trv_t trvData = {
	.tableSchema = "CREATE TABLE IF NOT EXISTS trv ("
		       "id TEXT,"
		       "timestamp DATE DEFAULT (datetime('now','localtime')),"				       
		       "pos REAL,"
	           "itemp REAL,"
			   "battery REAL,"
			   "calibrated TEXT);",	
	.dataEntry = insert_data_sql,
	.operation = 'W'
};

        sqlite3* key = connect_open_db("myDB");
	        if (write_to_db(key, &trvData)==1){

		free(id);
		free(insert_data_sql);
		printf("%s","\n### free heap after writing####\n");

		}

                
//SQL SCHEMA and DATA entry

 
		return 1;
	}

else if(strstr(topic, "read")!= NULL){
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
	snprintf(sql, sizeof(sql), "SELECT %s,timestamp FROM trv WHERE timestamp BETWEEN '%s' AND '%s'", column, startDate, endDate);
	
	trv_t trvDataRead = {
		.tableSchema = "CREATE TABLE IF NOT EXISTS trv ("
		       "id TEXT,"
		       "timestamp DATE DEFAULT (datetime('now','localtime')),"				       
		       "pos REAL,"
	           "itemp REAL,"
			   "battery REAL,"
			   "calibrated TEXT);",	
        	.dataEntry = sql,
        	.operation = 'R'
};

     sqlite3* key = connect_open_db("myDB");
                if (write_to_db(key, &trvDataRead)==1){
                     free(id); 
			cJSON_Delete(root);
                printf("%s","\n### free heap after reading ####\n");

    } else {
        fprintf(stderr, "Could not open and send data\n");
    }
      
    // Clean up cJSON structures
}
else{
	fprintf(stderr, "HERE Invalid JSON format\n");
		return 0;
	}
// JSON PARSING //

}


}


