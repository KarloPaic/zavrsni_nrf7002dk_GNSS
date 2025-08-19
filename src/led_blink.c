#include "led_blink.h"
#include <zephyr/kernel.h>
//gpio/led header
#include <zephyr/drivers/gpio.h>
//1 long - after led init
//2 blinks -location sent
//1 long - connected to wifi
//5 short - failed to connect/disconnected

//defines LED0_NODE from the led0 alias 
#define LED0_NODE DT_ALIAS(led0)

//grab gpio config for led0, gpio_dt_spec contains structures to control the led
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

//function controlling led blinking
void blink_led(int times, int blink_delay){
    for (int i = 0; i < times; i++)
    {
        gpio_pin_set_dt(&led0, 1); //set led to on
        k_msleep(blink_delay);
        gpio_pin_set_dt(&led0, 0); //set led to off
        k_msleep(blink_delay);
    }
    
}

int init_led(void){
    //led ready check
    if(!gpio_is_ready_dt(&led0)){
        printk("Led is not ready. Stopping...");
        return -1;
    }
    //start led OFF
    int ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    if(ret < 0){
        printk("Failed to intially config led");
        return -1;
    }
    return 0;
}