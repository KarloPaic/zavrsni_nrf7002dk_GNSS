#include "gps.h"

#include <stdio.h> 
#include <stdlib.h> 
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h> //uart functions and macros
#include <zephyr/sys/util.h> //utility macros(SYS_FOREVER_US)

#define UART_BUF_COUNT 2

#define UART_BUF_SIZE 83   // 82 max length of one nmea sentence

static uint8_t *uart_buffers[UART_BUF_COUNT];
static int uart_buf_idx = 0;
#define UART_DEVICE_NODE DT_NODELABEL(uart1)   //defines macro for UART device node, specifically uart1
const struct device *uart = DEVICE_DT_GET(UART_DEVICE_NODE); //fetch UART device using macro, stores in pointer uart used to comunicate with UART hardware

#define GTPA_DATA_BUFFER_SIZE 1024
static uint8_t gtpa_data_buffer[GTPA_DATA_BUFFER_SIZE];
static size_t gtpa_data_buffer_pos = 0;
char latest_nmea_sentence[128];
K_SEM_DEFINE(gps_data_sem, 0, 1);  //binary semaphore

//amount of commas in a NMEA sentence 
#define NMEA_COMMA_AMOUNT 16

//global data containers for latitude and longitude values
double lat_dec = 0;
double lon_dec = 0;
double user_speed = 0;
double altitude = 0;


static bool validate_checksum(const char *nmea){
    //fint the * marking the beggining of checksum portion
    const char *checksum_str = strchr(nmea, '*');
    if(!checksum_str){
        printk("Invalid checksum\n");
        return false;
    }
    //put the two characters after * representing checksum and a terminator into a buffer
    char checksum_buf[3] = {checksum_str[1], checksum_str[2], '\0'};
    
    //convert checksum characters into base 16
    uint8_t converted_checksum = (uint8_t)strtol(checksum_buf, NULL, 16);

    //calculate checksum - XOR of all characters between $ and *
    uint8_t calculated_checksum = 0;
    for(const char *p = nmea + 1; p < checksum_str; ++p){
        calculated_checksum ^= (uint8_t)(*p);
    }
    printk("Calculated checksum: %02X\n", calculated_checksum);

    //compare checksums, if they match sentence is not corrupted
    if(calculated_checksum != converted_checksum){
        printk("Checksum mismatch: expected %02X, got %02X\n", converted_checksum, calculated_checksum);
        return false;
    }
    return true;
}

float nmea_to_decimal(const char *coord, const char *dir, bool is_latitude){
    float deg = 0;
    float min = 0;
    
    //latitude ddmm.mmmm longitude dddmm.mmmm
    //spliting d and m sections into deg and min floats
    if(is_latitude){
        //2 digits for deg
        char deg_str[3] = {0};
        strncpy(deg_str, coord, 2);
        deg = atof(deg_str);
        min = atof(coord + 2);
    }else{
        //3 digits for deg
        char deg_str[4] = {0};
        strncpy(deg_str, coord, 3);
        deg = atof(deg_str);
        min = atof(coord + 3);
    }
    //formula for converting into decimal
    float decimal = deg + (min / 60);
    // fot lat N is positive, S is negative
    // for lon E is positive, S is negative
    if(dir[0] == 'S' || dir[0] == 'W'){
        decimal = -decimal;
    }

    return decimal;
}

//function to parse nmea sentences
bool nmea_parser(const char *nmea){
    //if sum doesnt match checksum value sentence is corrupted
    if(!validate_checksum(nmea)){
        printk("Invalid checksum. Skipping\n");
        return false;
    }

    char par_buf[128];
    //copy data into modifiable buffer
    strncpy(par_buf, nmea, sizeof(par_buf) - 1);
    par_buf[sizeof(par_buf) - 1] = '\0';

    //separate data by commas
    char *nmea_sections[16];
    int i = 0; // also the amount of commas
    //strtok - function do split string by certain character(in this case ,)
    char *nmea_section = strtok(par_buf, ",");
    while(nmea_section != NULL && i < 16){
        nmea_sections[i++] = nmea_section;
        nmea_section = strtok(NULL, ",");
    }

    if(strncmp(nmea, "$GPRMC", 6) == 0){
    
        const char *utc_time = nmea_sections[1];
        const char *status = nmea_sections[2];
        const char *latitude_raw = nmea_sections[3];
        const char *latitude_dir = nmea_sections[4];
        const char *longitude_raw = nmea_sections[5];
        const char *longitude_dir = nmea_sections[6];
        const char *speed = nmea_sections[7];
        const char *course = nmea_sections[8];
        const char *date = nmea_sections[9];

        if(status[0] != 'A'){
            printk("  GPRMC status not valid: %s\n", status[0] == 'A' ? "Active" : "Void");
            return false;
        }
        // converting lat and lon from ddmm.mmmm to proper decimals
        lat_dec = nmea_to_decimal(latitude_raw, latitude_dir, true);
        lon_dec = nmea_to_decimal(longitude_raw, longitude_dir, false);
        user_speed = atof(speed);
        // Print parsed data
        printk("GPRMC Parsed:\n");
        printk("  UTC Time: %s\n", utc_time);
        printk("  Status: %s\n", status[0] == 'A' ? "Active" : "Void");
        printk("  Latitude: %0.6f\n", lat_dec);
        printk("  Longitude: %0.6f\n", lon_dec);
        printk("  Speed (knots): %s\n", speed);
        printk("  Course: %s\n", course);
        printk("  Date: %s\n", date);
        return true;

    }else if(strncmp(nmea, "$GPGGA", 6) == 0){
        //const char *utc_time = nmea_sections[1];
        const char *latitude_raw = nmea_sections[2];
        const char *latitude_dir = nmea_sections[3];
        const char *longitude_raw = nmea_sections[4];
        const char *longitude_dir = nmea_sections[5];
        const char *fix_quality = nmea_sections[6];
        const char *altitude_raw = nmea_sections[9];

        if(fix_quality[0] != '1' && fix_quality[0] != '2'){
            printk("No GPS fix\n");
            return false;
        }

        lat_dec = nmea_to_decimal(latitude_raw, latitude_dir, true);
        lon_dec = nmea_to_decimal(longitude_raw, longitude_dir, false);
        altitude = atof(altitude_raw); // turn alt into float
        
        printk("GPGGA Parsed:\n");
        printk("  Latitude: %0.6f\n", lat_dec);
        printk("  Longitude: %0.6f\n", lon_dec);
        printk("  Altitude: %0.3f m\n", altitude);
        printk("  Fix Quality: %s\n", fix_quality);
        return true;
    }
        
    return false;
    
}




static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    int err;

	//switch picking what to do based on the event type
    switch (evt->type) {
		//when data is ready to be read
    case UART_RX_RDY:

        //better saving of data which checks for end of sentence and resets buffer position on end
        for(size_t i = 0; i < evt->data.rx.len; i++){
            if(gtpa_data_buffer_pos < GTPA_DATA_BUFFER_SIZE - 1){
                gtpa_data_buffer[gtpa_data_buffer_pos++] = evt->data.rx.buf[evt->data.rx.offset + i];

                if(evt->data.rx.buf[evt->data.rx.offset + i] == '\n'){
                    //end of sentence detected, terminate it and print out data and reset buffer
                    gtpa_data_buffer[gtpa_data_buffer_pos] = '\0';
                    
                    
                   
                    memcpy(latest_nmea_sentence, gtpa_data_buffer, gtpa_data_buffer_pos); 
                     
                    printk("[%u]Full NMEA sentence: %s", k_uptime_get_32(), gtpa_data_buffer);

                    //nmea_parser((const char *)gtpa_data_buffer);
                    k_sem_give(&gps_data_sem);

                    gtpa_data_buffer_pos = 0;
                }
            }else{
                printk("App buffer overflow\n");
                gtpa_data_buffer_pos = 0;
            }
        }
        break;
		//when uart driver requests a buffer for recieving data
    case UART_RX_BUF_REQUEST:
        //switch from currently active to the other buffer
        uart_buf_idx = (uart_buf_idx + 1) % UART_BUF_COUNT;
        uint8_t *next_buf = uart_buffers[uart_buf_idx];

        err = uart_rx_buf_rsp(uart, next_buf, UART_BUF_SIZE);
        if (err) {
            printk("uart_rx_buf_rsp error: %d\n", err);
        }
        break;
     
    case UART_RX_DISABLED:
        printk("UART RX Disabled, re-enabling...\n");
        
        err = uart_rx_enable(uart, uart_buffers[0], UART_BUF_SIZE, SYS_FOREVER_US);
        if (err) {
            printk("uart_rx_enable error: %d\n", err);
        }

        break;
    default:
        break;
    }
}

//function for sending commands to gps module
void configure_gtpa013(const char* command){

    size_t len = strlen(command);
    int ret = uart_tx(uart, command, len, SYS_FOREVER_US);
    k_sleep(K_MSEC(1000));
    if (ret) {
        printk("Failed to send GPS command (err %d)\n", ret);
    } else {
        printk("Sent GPS command: %s\n", command);
    }
}

int uart_init(void)
{
    int err;

    int ret = uart_callback_set(uart, uart_cb, NULL);
    if (ret) {
        printk("Failed to set UART callback: %d\n", ret);
        return -1;
    }

    //allocate both buffers
    for (int i = 0; i < UART_BUF_COUNT; i++) {
        uart_buffers[i] = k_malloc(UART_BUF_SIZE);
        if (!uart_buffers[i]) {
            printk("Failed to allocate UART buffer %d\n", i);
            return -1;
        }
    }
    k_msleep(1000);
    //enable uart rx with the first buffer
    err = uart_rx_enable(uart, uart_buffers[0], UART_BUF_SIZE, SYS_FOREVER_US);
    if (err) {
        printk("uart_rx_enable failed: %d\n", err);
    }
    return 0;
}
