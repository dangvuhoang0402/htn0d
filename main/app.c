#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <esp_log.h>
#include <esp_log_internal.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"

#include "pn532.h"
#include "wifi.h"
#include "HD44780.c"

#define portTICK_RATE_MS (1000 / configTICK_RATE_HZ)
#define PN532_SCK 18
#define PN532_MISO 19
#define PN532_MOSI 23
#define PN532_SS 5

#define LCD_ADDR 0x27
#define SDA_PIN  21
#define SCL_PIN  22
#define LCD_COLS 20
#define LCD_ROWS 4

uint8_t success;
uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
char uid_str[14 + 1];
uint8_t uidLength;
uint32_t blockNumber = 1;
uint8_t keyNumber = 1; // Use key A
uint8_t keyData[6]= {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t dataToWrite[16] = { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49 };
uint8_t dataFromBlock[16];
uint8_t dataToSend[12];
int amount =50000;
char *token = NULL;  // Global variable to store the token
char *name = NULL;  // Global variable to store the name
char* payment_status= NULL;  // Global variable to store the time
char *payment_reason=NULL;
char *payment_amount=NULL;

static const char *TAG = "APP";
static pn532_t nfc;
uint8_t dataBuffer[16];
static char *response_buffer = NULL;  // Buffer to hold the response
static int total_read_len = 0;  // Total length of the response

static int s_retry_num = 0;

Attendance_Device_t attendance_device;

void init(){
    pn532_spi_init(&nfc, PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
    pn532_begin(&nfc);

    uint32_t versiondata = pn532_getFirmwareVersion(&nfc);
    if (!versiondata)
    {
        ESP_LOGI(TAG, "Didn't find PN53x board");
        while (1)
        {
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
    // Got ok data, print it out!
    ESP_LOGI(TAG, "Found chip PN5 %lx", (versiondata >> 24) & 0xFF);
    ESP_LOGI(TAG, "Firmware ver. %lu.%lu", (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);

    pn532_SAMConfig(&nfc);

    ESP_LOGI(TAG, "Waiting for an ISO14443A Card ...");
}

void scan_and_read(){
    success = pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0);
    if (success)
    {
        // Display some basic information about the card
        ESP_LOGI(TAG, "Found an ISO14443A card");
        ESP_LOGI(TAG, "UID Length: %d bytes", uidLength);
        for (uint8_t i = 0; i < uidLength; i++)
        {
            sprintf(&uid_str[i * 2], "%02X", uid[i]);
        }
        ESP_LOGI(TAG,"UID Value: %s", uid_str);
        if (attendance_device.token == NULL)
        {
            send_post_login_request(&attendance_device);
        }
        send_check_card(uid,&attendance_device);
        if (strcmp(get_message(), "Success") != 0)
        {
            LCD_clearScreen();
            LCD_setCursor(2,1);
            LCD_writeStr("The chua dang ky");
            vTaskDelay(1000 / portTICK_RATE_MS);
            return;
        }
            LCD_clearScreen();
            LCD_setCursor(4,1);
            LCD_setCursor(4,2);
            LCD_writeStr("Dang xu ly...");
            send_post_request(uid,&attendance_device,amount);
            char* payment_status= get_payment_status();
            if(strcmp(payment_status, "Failed") == 0) {
                LCD_clearScreen();
                LCD_setCursor(4,1);
                LCD_writeStr("Payment failed");
                LCD_setCursor(4,2);
                char* payment_reason= get_payment_reason();
                LCD_writeStr(payment_reason);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }
            else {
                LCD_clearScreen();
                LCD_setCursor(4,1);
                LCD_writeStr("Payment success");
                LCD_setCursor(4,2);
                char amountStr[50];  // Adjust size as needed
                char* payment_amount= get_payment_amount();
                sprintf(amountStr, "Amount: %s", payment_amount);
                LCD_writeStr(amountStr);
                send_check_card(uid,&attendance_device);
                get_card_balance();
                LCD_setCursor(4,3);
                char balanceStr[50];  // Adjust size as needed
                char* balance= get_card_balance();
                sprintf(balanceStr, "Balance: %s", balance);
                LCD_writeStr(balanceStr);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }
        } 
    else
    {
        // PN532 probably timed out waiting for a card
        ESP_LOGI(TAG, "Timed out waiting for a card");
    }
}

void test_pcb(){
    success = pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, uid, &uidLength, 0);
    if (success)
    {
        // Display some basic information about the card
        ESP_LOGI(TAG, "Found an ISO14443A card");
        ESP_LOGI(TAG, "UID Length: %d bytes", uidLength);
        for (uint8_t i = 0; i < uidLength; i++)
        {
            sprintf(&uid_str[i * 2], "%02X", uid[i]);
        }
        ESP_LOGI(TAG,"UID Value: %s", uid_str);
    }
}

void app_main() {
    LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);
    LCD_setCursor(4, 1);
    LCD_writeStr("May Thanh Toan");
    // Start_connection(&attendance_device);
    // if (attendance_device.connected == 0)
    // {
    //     ESP_LOGI(TAG, "Not connected to the server");
    //     LCD_setCursor(4, 2);
    //     LCD_writeStr("Khong co Wifi");
    //     while (1){
    //         vTaskDelay(1000 / portTICK_RATE_MS);
    //     };
    // }
    // LCD_setCursor(3, 2);
    // LCD_writeStr("Da ket noi Wifi");
    // init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    while (1)
    {                 
        LCD_clearScreen();
        LCD_setCursor(3, 1);
        LCD_writeStr("Dat the vao may");    
        LCD_setCursor(4, 2);
        LCD_writeStr("de giao dich");
        LCD_setCursor(4, 3);
        char amountStr[50];  // Adjust size as needed
        sprintf(amountStr, "Tri Gia: %d", amount);
        LCD_writeStr(amountStr);
        // scan_and_read(); //read block[block_number value]
        test_pcb();
    }
}