#ifndef SHELLYPLUGS_H
#define SHELLYPLUGS_H

typedef struct shellyplugs_{

	char* tableSchema;
	char* dataEntry;
	char operation;

}shellyplugs_t;


int shellyplugs_handle(char*, char*);

#endif
