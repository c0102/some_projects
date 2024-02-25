#ifndef MOTION_H
#define MOTION_H

typedef struct motion_ {

	char* tableSchema;
	char* dataEntry;
	char operation;

} motion_t;


int motion_handle(char*, char*);


#endif
