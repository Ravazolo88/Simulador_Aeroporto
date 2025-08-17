#ifndef LOGGER_H
#define LOGGER_H

int logger_init(const char* filename);
void write_log(const char* message);
void logger_close(void);

#endif