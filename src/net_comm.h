#ifndef NET_COMM_H
#define NET_COMM_H

#include "net_comm.h"
#include <stdio.h> //standard c lib
#include <stdlib.h> //standard c lib
#include <zephyr/kernel.h>

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
#include <zephyr/data/json.h>


//led file header
#include "led_blink.h"
//server config
#include "config.h"

extern bool connected;

void wifi_ready_cb(bool ready);
void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface);
void connect_to_wifi_with_backup(struct net_if *iface);
void send_to_traccar(double lat, double lon, double speed, double alt);
int wifi_init(struct net_if *iface);


#endif
