
#include <stdio.h> //standard c lib
#include <stdlib.h> //standard c lib
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h> //uart functions and macros

#include <zephyr/sys/util.h> //utility macros(SYS_FOREVER_US)


#include <zephyr/net/net_if.h> //network interfaces 
#include <zephyr/net/net_mgmt.h> //api for controlling net events
#include <zephyr/net/net_event.h> //defines net events
#include <zephyr/net/wifi_mgmt.h> //wifi management scan, connect
 
#include <net/wifi_mgmt_ext.h> //needed for wifi credentials //rxtended API for working with WIFI credentials
#include <net/wifi_ready.h> // macros for wifi interface readyness checking

//SOCKET headers
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>

//led file header
#include "led_blink.h"
#include "gps.h"
#include "net_comm.h"

//logging system setup
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF); //main - module name, LoG_LEVEL_INF - info and above 


int main(void)
{

    printk("main started\n");
    printk("beginning\n");

    if(init_led() != 0){
        return 0;
    }
    blink_led(1, 2000);
    
    struct net_if *iface = net_if_get_default(); //get primary network interface
    if (iface == NULL) {
        LOG_ERR("Wi-Fi interface not found!");
        return 0;
    }

    if(wifi_init(iface) != 0) return 0;

        
    connect_to_wifi_with_backup(iface);
    

	k_sleep(K_MSEC(5000)); // DELAY JER SE INACE PRECOMPLETE I WIFI SETUP COMPLETE ISPISU  NAKON UART TEXTOVA ZBOG NEKOG RAZLOGA
    
    printk("Starting GNSS listener...\n");
	//check if uart device is ready
    if (!device_is_ready(uart)) {
        printk("UART device not ready!\n");
        return 0;
    }
    
    k_sleep(K_MSEC(2000)); // Give GPS time to boot
    configure_gtpa013("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"); // set only RMC nmea sentence and GGA
    k_sleep(K_MSEC(2));

    //configure_gtpa013("$PMTK220,1000*1F\r\n");// se output interval
    //k_sleep(K_MSEC(10000));

    if(uart_init() != 0) return 0;

    printk("UART RX interrupt listener started\n");

    k_sleep(K_MSEC(5000));
        
    while (1) {
        k_sem_take(&gps_data_sem, K_FOREVER);

        // parse + send latest
        if (nmea_parser(latest_nmea_sentence)) {
            send_to_traccar(lat_dec, lon_dec, user_speed, altitude);
        }

        //if connected is set to false(in case a dicsonnect happened), attempt to reconnect
        if(!connected){
            printk("Attemping to recconnect wifi.");
            connect_to_wifi_with_backup(iface);
        }
        


        k_msleep(1);
    }
        
        
    
}
