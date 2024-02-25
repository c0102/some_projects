#include<sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include "shellyplugs.h"
#include "trv.h"
#include <cJSON.h>



int callback(void *data, int argc, char **argv, char **azColName) {
  cJSON *resultaArray = (cJSON *) data;
  cJSON *resultaObject = cJSON_CreateObject();
    for (int i = 0; i < argc; i++) {
    printf("---%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   cJSON_AddStringToObject(resultaObject, azColName[i], argv[i]);
             }
        printf("\n");
 cJSON_AddItemToArray(resultaArray, resultaObject);
        return 0;
}



sqlite3* connect_open_db(const char* dbName){
sqlite3 *db = NULL;
int rc = sqlite3_open("test.db", &db);

return db;
}


int write_to_db(sqlite3* db , void* dataStruct){
int rc = 0;

//Handle for different types
trv_t* dataFromDevice = (trv_t *) dataStruct;


if(dataFromDevice->operation == 'R'){
char *zErrMsg = 0;
printf("\n%s\n",dataFromDevice->dataEntry);
//sqlite3_exec(db, dataFromDevice->dataEntry, callback, 0, &zErrMsg);

 cJSON *jsonObject = cJSON_CreateObject();
 if (jsonObject == NULL) {
   fprintf(stderr, "Failed to create JSON object\n");
    return 1;
    }



    cJSON *resultaArray = cJSON_AddArrayToObject(jsonObject, "resulta");
    if (resultaArray == NULL) {
        fprintf(stderr, "Failed to create 'resulta' array\n");
        cJSON_Delete(jsonObject);
        return 1;
    }





rc = sqlite3_exec(db, dataFromDevice->dataEntry, callback, (void *)resultaArray, &zErrMsg);


    char *jsonString = cJSON_Print(jsonObject);
    if (jsonString == NULL) {
        fprintf(stderr, "Failed to convert cJSON object to JSON string\n");
        cJSON_Delete(jsonObject);
        return 0;
    }

    // Print the JSON string
    printf("%s\n", jsonString);

    // Free memory
    cJSON_Delete(jsonObject);
    free(jsonString);



 if (rc != SQLITE_OK) {

	   fprintf(stderr, "SQL error: %s\n", zErrMsg);
	     sqlite3_free(zErrMsg);

	      }

 sqlite3_close(db);

printf("\n%s\n","before returning");
return 1;
}
else{


// SQL query to create the EnergyMeter table
    const char *create_table_sql = dataFromDevice->tableSchema;
    char *insert_data_sql = dataFromDevice->dataEntry;
    // Execute the SQL query to create the table
    rc = sqlite3_exec(db, create_table_sql, 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return rc;
    } else {
        printf("Table created successfully\n");
    }



    // Execute the SQL query
    rc = sqlite3_exec(db, insert_data_sql, 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to insert data: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Data inserted successfully\n");
   }

    // Free the allocated memory
    //free(insert_data_sql);

    // Close the database
    sqlite3_close(db);

    return  1;
}
}

