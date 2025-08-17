#ifndef CONFIG_H
#define CONFIG_H

#define NUM_RUNWAYS 3
#define NUM_GATES 5
#define TOWER_CAPACITY 2
#define MAX_PLANES 50

#define TOTAL_SIMULATION_TIME 240
#define ALERT_WAIT_TIME 60
#define FALL_WAIT_TIME 90

typedef enum {
    DOMESTIC,
    INTERNATIONAL
} FlightType;

#endif