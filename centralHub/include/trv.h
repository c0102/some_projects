#ifndef TRV_H
#define TRV_H


typedef struct trv_ {
	char *tableSchema;
	char *dataEntry;
	char operation;
} trv_t;


int trv_handle(char*, char*);


#endif
