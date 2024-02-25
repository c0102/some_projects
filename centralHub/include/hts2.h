#ifndef HTS2_H
#define HTS2_H

typedef struct hts2_ {
	char* tableSchema;
	char* dataEntry;
	char operation;
} hts2_t;

int hts2_handle(char*, char*);


#endif
