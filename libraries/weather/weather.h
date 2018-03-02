#ifndef WEATHER_H_
#define WEATHER_H_

#include "lwip/err.h"

typedef struct
{
    err_t error_code;
    int temp;
    int humidity;
    char city[100];
}weather_struct_t;

typedef void (*newData_clbk_f)(weather_struct_t *data);

void WEATHER_RegisterDataNotify(newData_clbk_f);

void WEATHER_Update();

#endif /* WEATHER_H_ */
