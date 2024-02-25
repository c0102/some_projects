#ifndef DB_HANDLER_H
#define DB_HANDLER_H
#include <sqlite3.h>
//connect and open database
//the function return a sqlite3 pointer
//the only function parameter is used to name the data base.
sqlite3* connect_open_db(const char*);


// write to the db
// parameters:
// 1.sqlite3 pointer
// 2.table name
// 3.Not given yet probably a struct
int write_to_db(sqlite3*, void*);



#endif
