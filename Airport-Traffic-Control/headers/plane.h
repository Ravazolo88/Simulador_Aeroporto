#ifndef PLANE_H
#define PLANE_H

#include <time.h>
#include <stdbool.h>
#include "config.h"

typedef enum { 
    ACTIVE, 
    CRASHED 
} PlaneStatus;

typedef struct {
    int id;
    FlightType type;
    time_t init_time_wait; 
    int priority; 
    bool on_alert; // Indica se o avião está em estado de alerta
    PlaneStatus status; 
} PlaneData;


void* plane_routine(void* arg);

#endif