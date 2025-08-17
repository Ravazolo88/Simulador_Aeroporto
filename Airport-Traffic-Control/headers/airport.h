#ifndef AIRPORT_H
#define AIRPORT_H

#include "plane.h"
#include "utils.h"


void airport_init(PriorityQueue* landing_q, PriorityQueue* disembarking_q, PriorityQueue* takeoff_q);
void airport_destroy(void);

void airport_request_landing_resources(PlaneData* data);
void airport_release_landing_resources();

void airport_request_disembarking_resources(PlaneData* data);
void airport_release_disembarking_resources();

void airport_request_takeoff_resources(PlaneData* data);
void airport_release_takeoff_resources();


#endif // AIRPORT_H 
 
