#ifndef LED_BLINK_H
#define LED_BLINK_H
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

void blink_led(int times, int blink_delay);
int init_led(void);

#endif