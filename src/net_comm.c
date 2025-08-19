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
//server params
#include "config.h"

//wifi ready support
static K_SEM_DEFINE(wifi_ready_sem, 0, 1);
static bool wifi_ready_status = false;

//combining 4 event types to call in a callback
#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT | NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE)
static struct net_mgmt_event_callback wifi_cb; //event listener for wifi connection
bool connected = false; 

//suport for scanning for wifi networks
K_SEM_DEFINE(scan_sem, 0, 1); // semaphor for scanning
#define SCAN_TIMEOUT 10000
#define MAX_OPEN_RESULTS 10
static struct wifi_scan_result open_results[MAX_OPEN_RESULTS]; //container for info of networks found during wifi scanning
static int open_result_count = 0;
static bool open_network_available = false; //flag to check if an open network i available 


K_SEM_DEFINE(gps_data_ready, 0, 1);




//zephyr is event driven, it connects to wifi in the background
//later it shows the result through a callback


//logging system setup
LOG_MODULE_REGISTER(net_comm, LOG_LEVEL_INF); //main - module name, LoG_LEVEL_INF - info and above


//saving scanned networks into open results
static void scan_result_handler(struct net_mgmt_event_callback *cb){
    const struct wifi_scan_result *entry = (const struct wifi_scan_result *)cb->info;
	//char mac_buf[sizeof("xx:xx:xx:xx:xx:xx")];


    if(entry->security == WIFI_SECURITY_TYPE_NONE){
        if(open_result_count < MAX_OPEN_RESULTS){
            open_results[open_result_count] = *entry;
            open_result_count++;
            open_network_available = true;
            if (open_result_count == 1) {
		        printk("Num | SSID                             | RSSI | Channel | BSSID\n");
	        }

            printk("%-3d | %-32s | %-4d | %-7d\n",
		        open_result_count,
		        entry->ssid,
		        entry->rssi,
		        entry->channel);

        }else{
            printk("Open result list full, Skipping: %s\n", entry->ssid);
        }
    }
}


//function for scanning for open networks
static int scan_for_open_networks(struct net_if *iface){
    
    //reset amount of found networks
    open_result_count = 0;
    struct wifi_scan_params params = { 0 };

    //actuall scanning
    if(net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params, sizeof(params)) != 0){
        LOG_ERR("Wi-Fi scan request failed");
        return -1;
    }

    k_sem_take(&scan_sem, K_MSEC(SCAN_TIMEOUT));
    return 0;

}

static int connect_to_open_wifi(struct net_if *iface){
    //if flag is set to false no open network if found
    if(!open_network_available){
        LOG_WRN("No open networks found");
        return -ENOENT;
    }

    for (size_t i = 0; i < open_result_count; i++){
    
        //parameters for an open network
        struct wifi_connect_req_params params = {
            .ssid = open_results[i].ssid,
            .ssid_length = strlen(open_results[i].ssid),
            .security = WIFI_SECURITY_TYPE_NONE,
            .channel = WIFI_CHANNEL_ANY,
            .timeout = SYS_FOREVER_MS,
        };

        LOG_INF("Connecting to open network: %s", open_results[i].ssid);
        //connecting
        net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
        //wait up to 10 seconds for connection to be confirmed
        int tries = 0;
        //loop waiting untill wifi connects or times out
        while (!connected && tries++ < 10) {
            printk("Trying to connect to network %d: %d", i, tries);
            k_sleep(K_SECONDS(1));
        }

        if(connected){ 
            LOG_INF("Connected to: %s", open_results[i].ssid);
            break;
        }
    }
    
    open_network_available = false; // setting flag back to false for the next time

    return 0;
}
//connecting to wifi network in with defined params
static void connect_to_known_wifi(struct net_if *iface){
    
	net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0); 
    
    
    LOG_INF("Waiting for Wi-Fi connection result...");
    // Wait up to 10 seconds for connection to be confirmed
    int tries = 0;
    //loop waiting untill wifi connects or times out
    while (!connected && tries++ < 10) {
        printk("Trying to connect to known network: %d", tries);
        k_sleep(K_SECONDS(1));
    }

    if (!connected) {
        blink_led(5, 100);
        LOG_WRN("Connection to known Wi-Fi failed or timed out");
    } else {
        LOG_INF("Connected to known Wi-Fi successfully");
    }
}

void connect_to_wifi_with_backup(struct net_if *iface){
    connect_to_known_wifi(iface);

    //if device manages to connect to known wifi no need to continue
    if(connected){
        return;
    }
    

    LOG_INF("Falling back to open networks == 0");
    /* SCANNING OVERALL
    WORKS USING A net_mgmt  in scan_for_open_networks meaning in need to a callback like the the wifi event handler,
    2 events this uses are scanning result and scanning done,
    scanning done calls the scan_done_handler which checks the status of the scan,
    scanning result calls scan_result_handler which  which is used to check for open networks
    in all the found networks and stores the for use when connecting_to them,
    after all that if any opens network are found connect_to_open_wifi goes ahead and connects to one similar to how connecting to known works*/
    //if scanning successfully finds any networks continue to connecting to open network
    if(scan_for_open_networks(iface) == 0){
        connect_to_open_wifi(iface);
    }
}

//callback for wifi ready
void wifi_ready_cb(bool ready){
    LOG_INF("Wi-Fi ready state changed: %s", ready ? "READY" : "NOT READY");
    wifi_ready_status = ready;
    k_sem_give(&wifi_ready_sem);
}


//function runs when when wifi connect event happens
void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface/*, void *info*/){
    LOG_INF("Mgmt event: 0x%x", mgmt_event);

    
    //cb - pointer to callback, contains event info
    //mgmt_event - event code
    //iface - network interface

    //takes info from cb and casts into wifi_status struct
    //wifi_status  zephyr struct for reporting wifi event results
    //const struct wifi_status *status = cb->info; //OLD

    switch (mgmt_event)
    {
    case NET_EVENT_WIFI_CONNECT_RESULT://if wifi connection result happened(succes or fail)
        //const struct wifi_status *status = info; //event handler includes info parameter directly
		const struct wifi_status *status =(const struct wifi_status *) cb->info;
        if(status->status == 0 && net_if_is_up(iface)){ //if success (status = 0 ->success)
            //set connected flag to true and log - LOG_DEFAULT_LEVEL must be 3
            connected = true;
            LOG_INF("Connected to wifi and interface is up.");
            blink_led(1, 800);
        } else{
            LOG_ERR("Failed to connect (%d), connect event fired off but interface is off", status->status);
            blink_led(5, 100);
        }
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT://if disconnection is detected set connect to false
        connected = false;
        LOG_WRN("Wifi disconnected.");
        blink_led(5, 100);
        break;
    case NET_EVENT_WIFI_SCAN_RESULT:
        scan_result_handler(cb);
        break;
    case NET_EVENT_WIFI_SCAN_DONE:
        //tells about successfullnes of wifi scanning
        //gets status from callback
        const struct wifi_status *status_scan = (const struct wifi_status *)cb->info;
        if(status_scan->status == 0){
            printk("Scan completed successfully\n");
        } else{
            printk("Scan failed with status %d\n", status_scan->status);
        }
        //open_result_count = 0;
        //semaphore to synchronise with scan for open networks
        k_sem_give(&scan_sem);
        break;
    
    default:
        break;
    }
    
}




//function which formats a request, opens a socket and send the request to a server
void send_to_traccar(double lat, double lon, double speed, double alt){
    //int speed = 10;
    
    // declares structure to hold srever's IP and PORT in IPv4
    struct sockaddr_in server_addr;
    int sock;
    //buffer to hold the htttp GET request string
    char request[512];
    
    //formats a HTTP GET string contaiining id, lat, lon, speed and alt
    snprintf(request, sizeof(request),
         "GET /?id=%s&lat=%0.6f&lon=%0.6f&speed=%0.02f&altitude=%0.02f HTTP/1.0\r\n\r\n",
         DEVICE_ID, lat, lon, speed, alt);
    
    //creates a new TCP socket AF_INTE:IPv4, SOCK_STREAM:TCP, IPPROTO_TCP-  specifies TCP protocol
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printf("Failed to create socket\n");
        return;
    }

    //set server address
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(SERVER_PORT); //convertscto network byte order and stores it
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); //converts IP into a 32 bit binary in network byte order

    //attempts to connect socket to Traccar server at specified IP and PORT
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to connect to server\n");
        close(sock);
        return;
    }
    //prints full http request for DEBUGGING
    printf("Final request: %s\n", request);

    //sends HTTP request string over the established TCP socked
    if(send(sock, request, strlen(request), 0) < 0){
        printf("Failed to send location\n");
    }else {
        printf("Location sent successfully\n");
        blink_led(2, 200);
    }
    
    //close socket
    close(sock);
}


int wifi_init(struct net_if *iface){

    //wifi callback registration, tells zephyr to listen for a wifi connection event and call wifi event handler when it happens
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, WIFI_EVENTS);
    net_mgmt_add_event_callback(&wifi_cb);

    //registering wifi ready callback
    wifi_ready_callback_t cb;
    cb.wifi_ready_cb = wifi_ready_cb;
    int ret = register_wifi_ready_callback(cb, iface);
    if(ret != 0){
        LOG_ERR("Failed to register wifi ready cb: %d", ret);
        return -1;
    }
    //waiting for wifi to be ready
    LOG_INF("Waiting for Wi-Fi to become ready");
    k_sem_take(&wifi_ready_sem, K_FOREVER);
    if(!wifi_ready_status){
        LOG_ERR("Wi-Fi is not ready. Continuing"); 
    }
    return 0;
}

