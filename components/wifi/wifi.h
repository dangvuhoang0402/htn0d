#ifndef WIFI_H
#define WIFI_H
#include "attendance_device.h"
#include <stdint.h>

#define EXAMPLE_ESP_WIFI_SSID      "ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

void Start_connection(Attendance_Device_t *device);
void send_post_login_request(Attendance_Device_t *device);
void send_post_request(uint8_t uid[], Attendance_Device_t *device,int amount);
void send_post_test_request(Attendance_Device_t *device);
void send_check_card(uint8_t uid[], Attendance_Device_t *device) ; 
char *get_success();
char *get_code();
char* get_payment_amount();
char* get_payment_reason();
char* get_payment_status();
char* get_message();
char* get_card_balance();
#endif // WIFI_H