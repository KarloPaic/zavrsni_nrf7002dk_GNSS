// gps.h
#ifndef GPS_H
#define GPS_H

#include <stdio.h> 
#include <stdlib.h> 
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h> //uart functions and macros
#include <zephyr/sys/util.h> //utility macros(SYS_FOREVER_US)

#define UART_BUF_COUNT 2
#define UART_BUF_SIZE 83  // 82 max length of one NMEA sentence
#define GTPA_DATA_BUFFER_SIZE 1024

extern const struct device *uart;

extern double lat_dec;
extern double lon_dec;
extern double altitude;
extern double user_speed;

extern char latest_nmea_sentence[128];

extern struct k_sem gps_data_sem;

int uart_init(void);
bool nmea_parser(const char *nmea);
void configure_gtpa013(const char *command);
float nmea_to_decimal(const char *coord, const char *dir, bool is_latitude);

#endif