#ifndef SHELLYPLUS2PM_H
#define SHELLYPLUS2PM_H



typedef struct shellyPlus2Pm_
{
    char* tableSchema;
    char* dataEntry;
    char operation;

}shellyPlus2Pm_t;


int shellyPlus2Pm_handle(char*, char*);


#endif