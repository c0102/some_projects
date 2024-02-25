#ifndef SHELLYPLUS1PM_H
#define SHELLYPLUS1PM_H



typedef struct shellyPlus1Pm_
{
    char* tableSchema;
    char* dataEntry;
    char operation;

}shellyPlus1Pm_t;


int shellyPlus1Pm_handle(char*, char*);


#endif