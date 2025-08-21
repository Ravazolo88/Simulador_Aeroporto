#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

void log_init(const char* filename);
void log_message(const char* format, ...);
void log_close();

#endif