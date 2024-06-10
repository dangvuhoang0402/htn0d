#include "wifi.h"
#include <stdio.h>
#include <esp_spiffs.h>
#include "esp_log.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_http_client.h"
#include <stdlib.h>
#include "mbedtls/base64.h"

#include "nvs_flash.h"
#include <ctype.h>

static const char *TAG = "DEVICE";
static char *response_buffer = NULL;
static int s_retry_num = 0;
static int total_read_len = 0; 
static EventGroupHandle_t s_wifi_event_group;
#define MIN(a,b) (((a)<(b))?(a):(b))
#define TOKEN_LENGTH 20  // Define the length of the token
char expected_auth_admin_header[256];
char expected_auth_supporter_header[256];
char* response_success=NULL;
char* response_message=NULL;
char* response_code=NULL;
char* token_test=NULL;
uint8_t uidTest[]="081223000017";
uint8_t uidCardTest[]="100000003";
char* uid_admin_test=NULL;
char* uid_card_admin_test= NULL;
char* deviceId_test=NULL;
char* serviceCode_test=NULL;

Attendance_Device_t* device_to_use;

char* get_message(){
    char* message_str="\"message\"";
    char* message_ptr=strstr(response_buffer,message_str);
    char* message_end=strchr(message_ptr,',');
    if (message_end==NULL) message_end=strchr(message_ptr,'}');
    int message_len=message_end-message_ptr-12;
    char* message=malloc(message_len+1);
    strncpy(message,message_ptr+11,message_len);
    message[message_len]='\0';
    ESP_LOGI(TAG,"Message: %s",message);
    response_message=message;
    return message;

}

char* get_code(){
    char* code_str="\"code\"";
    char* code_ptr=strstr(response_buffer,code_str);
    char* code_end=strchr(code_ptr,',');
    if (code_end==NULL) code_end=strchr(code_ptr,'}');
    int code_len=code_end-code_ptr-8;
    char* code=malloc(code_len+1);
    strncpy(code,code_ptr+8,code_len);
    code[code_len]='\0';
    ESP_LOGI(TAG,"Code: %s",code);
    response_code=code;
    return code;
}

char* get_success(){
    char *success_str = "\"success\"";
    char *success_ptr = strstr(response_buffer, success_str);
    if (success_ptr != NULL) {
        char *success_end = strchr(success_ptr, ',');
        int success_len = success_end - success_ptr - 10;
        
        // Allocate memory for response_success
        response_success = malloc(success_len + 1);
        if (response_success == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response_success");
            return NULL;
        }
        strncpy(response_success, success_ptr + 10, success_len);
        response_success[success_len] = '\0';  // Null-terminate the string
        ESP_LOGI(TAG, "Success:%s", response_success);
    }
    if(strcmp(response_success, "true") == 0) {
        response_message="Success";
    }
    return response_success;
}

void Get_Token(Attendance_Device_t *device){
    ESP_LOGI(TAG,"Response: %s",response_buffer);

    char *token_str = "\"token\":\"";
    char *token_ptr = strstr(response_buffer, token_str);
    if (token_ptr == NULL) {
        ESP_LOGE(TAG, "Token not found in response");
        return;
    }
    char *substring = token_ptr + strlen(token_str);
    char *end_token_str = "\"";
    char *end_token_ptr = strstr(substring, end_token_str);
    if (end_token_ptr == NULL) {
        ESP_LOGE(TAG, "End of token not found in response");
        return;
    }
    int token_length = end_token_ptr - substring;
    device->token = malloc(token_length + 1);  // +1 for the null-terminator
    if (device->token == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for token");
        return;
    }
    strncpy(device->token, substring, token_length);
    device->token[token_length] = '\0';  // Null-terminate the string
}
char* get_payment_status(){
    char* payment_status_str="\"Status\"";
    char* payment_status_ptr=strstr(response_buffer,payment_status_str);
    char* payment_status_end=strchr(payment_status_ptr,',');
    if (payment_status_end==NULL) payment_status_end=strchr(payment_status_ptr,'}');
    int payment_status_len=payment_status_end-payment_status_ptr-10;
    char* payment_status=malloc(payment_status_len+1);
    strncpy(payment_status,payment_status_ptr+9,payment_status_len);
    payment_status[payment_status_len]='\0';
    ESP_LOGI(TAG,"Payment Status: %s",payment_status);
    return payment_status;
}

char* get_payment_reason(){
    char* payment_reason_str="\"Reason\"";
    char* payment_reason_ptr=strstr(response_buffer,payment_reason_str);
    char* payment_reason_end=strchr(payment_reason_ptr,',');
    if (payment_reason_end==NULL) payment_reason_end=strchr(payment_reason_ptr,'}');
    int payment_reason_len=payment_reason_end-payment_reason_ptr-10;
    char* payment_reason=malloc(payment_reason_len+1);
    strncpy(payment_reason,payment_reason_ptr+9,payment_reason_len);
    payment_reason[payment_reason_len]='\0';
    ESP_LOGI(TAG,"Payment Reason: %s",payment_reason);
    return payment_reason;
}

char* get_payment_amount(){
    char* payment_amount_str="\"Amount\"";
    char* payment_amount_ptr=strstr(response_buffer,payment_amount_str);
    char* payment_amount_end=strchr(payment_amount_ptr,',');
    if (payment_amount_end==NULL) payment_amount_end=strchr(payment_amount_ptr,'}');
    int payment_amount_len=payment_amount_end-payment_amount_ptr-10;
    char* payment_amount=malloc(payment_amount_len+1);
    strncpy(payment_amount,payment_amount_ptr+9,payment_amount_len);
    payment_amount[payment_amount_len]='\0';
    ESP_LOGI(TAG,"Payment Amount: %s",payment_amount);
    return payment_amount;
}

char* get_card_balance(){
    char* card_balance_str="\"Balance\"";
    char* card_balance_ptr=strstr(response_buffer,card_balance_str);
    char* card_balance_end=strchr(card_balance_ptr,',');
    if (card_balance_end==NULL) card_balance_end=strchr(card_balance_ptr,'}');
    int card_balance_len=card_balance_end-card_balance_ptr-10;
    char* card_balance=malloc(card_balance_len+1);
    strncpy(card_balance,card_balance_ptr+9,card_balance_len);
    card_balance[card_balance_len]='\0';
    ESP_LOGI(TAG,"Card Balance: %s",card_balance);
    return card_balance;

}

esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch(evt->event_id)
    {
        case HTTP_EVENT_HEADER_SENT:
            // Free the existing response buffer before receiving a new response
            if (response_buffer != NULL) {
                free(response_buffer);
                response_buffer = NULL;
            }
            total_read_len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            // Reallocate the response buffer to hold the new data
            response_buffer = realloc(response_buffer, total_read_len + evt->data_len + 1);
            if (response_buffer == NULL) {
                printf("Failed to allocate memory for response buffer\n");
                total_read_len = 0;
                break;
            }

            // Copy the new data into the response buffer
            memcpy(response_buffer + total_read_len, evt->data, evt->data_len);
            total_read_len += evt->data_len;

            // Null-terminate the response buffer
            response_buffer[total_read_len] = '\0';
            break;
        default:
            break;
    }
    return ESP_OK;
}

void send_post_login_request(Attendance_Device_t *device) {
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    ESP_LOGI(TAG, "API Login URL: %s", device->api_login_url);
    config.url = device->api_login_url;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char post_data[256];  // Adjust the size as needed
    sprintf(post_data, "{\"userName\":\"%s\",\"password\":\"%s\"}", device->partnerCode, device->accessCode);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    ESP_LOGI(TAG, "Sending POST data: %s", post_data);  // Log the POST data
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    if (strcmp(get_message(), "Success") == 0) {
        Get_Token(device);
        ESP_LOGI(TAG, "token: %s", device->token);
    }
    else
    {
        get_code();
    }
}

void send_post_login_test_request(char* login_url,char* accessCode,char* partnerCode){
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    config.url = login_url;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    char post_data[256];  // Adjust the size as needed
    sprintf(post_data, "{\"partnerCode\":\"%s\",\"accessCode\":\"%s\"}", partnerCode, accessCode);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    get_success();
    if (strcmp(response_success, "true") == 0) {
        ESP_LOGI(TAG,"Response: %s",response_buffer);
        char *token_str = "\"token\"";
        char *token_ptr = strstr(response_buffer, token_str);
        char *substring = token_ptr+9;
        char *expiredIn_str = "\",\"expiredIn\":";
        char *expiredIn_ptr = strstr(substring, expiredIn_str);
        int expiredIn_position = expiredIn_ptr - substring;
        token_test = malloc(expiredIn_position + 1);  // +1 for the null-terminator
        strncpy(token_test, substring, expiredIn_position);
        token_test[expiredIn_position] = '\0';  // Null-terminate the string
        ESP_LOGI(TAG, "token: %s", token_test);
    }
    else
    {
        get_code();
    }
}

void send_post_test_admin_request(char* attendance_url,char* deviceId,char* uidCardTest, char* uidTest, char* serviceCode, char* token){
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    config.url = attendance_url;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[256];
    sprintf(post_data, "{\"deviceId\":\"%s\",\"cardUid\":\"%s\",\"uid\":\"%s\",\"serviceCode\":\"%s\"}", deviceId,uidCardTest,uidTest,serviceCode);
    char auth_header[256];  // Adjust size as needed
    sprintf(auth_header, "Bearer %s", token);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Authorization: %s", auth_header);  // Log the Authorization header value
    ESP_LOGI(TAG, "Sending POST data: %s", post_data);  // Log the POST data

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status Code: %d", status_code);
    ESP_LOGI(TAG,"Response: %s",response_buffer);
    esp_http_client_cleanup(client);
    get_success();  
    if(strcmp(response_success,"false")==0){
        get_code();
    }

}

void send_post_request(uint8_t uid[], Attendance_Device_t *device,int amount)  {
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    config.url = device_to_use->api_attendance_url;

    char uidStr[15];  // Each byte will be represented by two hexadecimal digits
    for (int i = 0; i < 4; i++) {
        sprintf(&uidStr[i * 2], "%02x", uid[i]);
    }
    uidStr[14] = '\0';  // Null-terminate the string

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[256];
    sprintf(post_data, "{\"ClientCard\":\"%s\",\"MerchantCard\":\"%s\",\"Amount\":%d}", uidStr, device->merchantCard, amount);
    char auth_header[256];  // Adjust size as needed
    sprintf(auth_header, "Bearer %s", device->token);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Authorization: %s", auth_header);  // Log the Authorization header value
    ESP_LOGI(TAG, "Sending POST data: %s", post_data);  // Log the POST data

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status Code: %d", status_code);
    ESP_LOGI(TAG,"Response: %s",response_buffer);
    esp_http_client_cleanup(client);
}

void send_check_card(uint8_t uid[], Attendance_Device_t *device)  {
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_GET,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    config.url = "http://192.168.1.11:3003/card";

    char uidStr[15];  // Each byte will be represented by two hexadecimal digits
    for (int i = 0; i < 4; i++) {
        sprintf(&uidStr[i * 2], "%02x", uid[i]);
    }
    uidStr[14] = '\0';  // Null-terminate the string

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[256];
    sprintf(post_data, "{\"CardUid\":\"%s\"}", uidStr);
    char auth_header[256];  // Adjust size as needed
    sprintf(auth_header, "Bearer %s", device->token);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Authorization: %s", auth_header);  // Log the Authorization header value
    ESP_LOGI(TAG, "Sending POST data: %s", post_data);  // Log the POST data

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status Code: %d", status_code);
    ESP_LOGI(TAG,"Response: %s",response_buffer);
    esp_http_client_cleanup(client);
}

void send_post_test_request(Attendance_Device_t* device){
    esp_http_client_config_t config = {
        .url = NULL,
        .method = HTTP_METHOD_POST,
        .cert_pem=NULL,
        .event_handler= client_event_post_handler
    };
    config.url = device_to_use->api_attendance_url;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[256];
    sprintf(post_data, "{\"deviceId\":\"%s\",\"cardUid\":\"%s\",\"uid\":\"%s\",\"serviceCode\":\"%s\"}", device->deviceId,uidCardTest,uidTest,device->serviceCode);
    char auth_header[256];  // Adjust size as needed
    sprintf(auth_header, "Bearer %s", device->token);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Authorization: %s", auth_header);  // Log the Authorization header value
    ESP_LOGI(TAG, "Sending POST data: %s", post_data);  // Log the POST data

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status Code: %d", status_code);
    ESP_LOGI(TAG,"Response: %s",response_buffer);
    esp_http_client_cleanup(client);
    get_success();  
    if(strcmp(response_success,"false")==0){
        get_code();
    }
}

void generate_admin_html(Attendance_Device_t* device) {
    char html[2048*2];
    sprintf(html,
        "<body>"
        "<h1>ESP32 Web Server</h1>"
        "<h2>Device Config</h2>"
        "<div style=\"display: flex; justify-content: space-between;\">"
        "<div style=\"flex: 1; margin-left: 10px;\">"
        "<form method=\"POST\" action=\"/admin_config\">"
            "<label for=\"ssid\">SSID:</label><br>"
            "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"%s\"><br>"
            "<label for=\"password\">Password:</label><br>"
            "<input type=\"password\" id=\"password\" name=\"password\" value=\"%s\"><br>"
            "<label for=\"DeviceId\">DeviceId:</label><br>"
            "<input type=\"text\" id=\"DeviceId\" name=\"DeviceId\" value=\"%s\"><br>"
            "<lable for=\"ServiceCode\">ServiceCode:</label><br>"
            "<input type=\"text\" id=\"ServiceCode\" name=\"ServiceCode\" value=\"%s\"><br>"
            "<label for=\"api_login_url\">API Login URL:</label><br>"
            "<input type=\"text\" id=\"api_login_url\" name=\"api_login_url\" value=\"%s\"><br>"
            "<label for=\"api_attendance_url\">API Attendance URL:</label><br>"
            "<input type=\"text\" id=\"api_attendance_url\" name=\"api_attendance_url\" value=\"%s\"><br>"
            "<label for=\"adminPass\">Admin Password:</label><br>"
            "<input type=\"password\" id=\"adminPass\" name=\"adminPass\" value=\"%s\"><br>"
            "<label for=\"supporterPass\">Supporter Password:</label><br>"
            "<input type=\"password\" id=\"supporterPass\" name=\"supporterPass\" value=\"%s\"><br>"
            "<label for=\"accessCode\">Access Code:</label><br>"
            "<input type=\"text\" id=\"accessCode\" name=\"accessCode\" value=\"%s\"><br>"
            "<label for=\"partnerCode\">Partner Code:</label><br>"
            "<input type=\"text\" id=\"partnerCode\" name=\"partnerCode\" value=\"%s\"><br>"
            "<label for=\"merchantCard\">Merchant Card:</label><br>"
            "input type=\"text\" id=\"merchantCard\" name=\"merchantCard\" value=\"%s\"><br>"
            "<input type=\"submit\" value=\"Submit\">"

        "</form>"
        "<form method=\"post\" action=\"/logout\">"
            "<input type=\"submit\" value=\"Get Back\">"
        "</form>"
        "<form method=\"post\" action=\"/test_api_attendance_admin\">"
            "<label for=\"apiAttendanceUrl\">API Attendance URL:</label><br>"
            "<input type=\"text\" id=\"apiAttendanceUrl\" name=\"apiAttendanceUrl\"><br>"
            "<label for=\"deviceId\">deviceId:</label><br>"
            "<input type=\"text\" id=\"deviceId\" name=\"deviceId\"><br>"
            "<label for=\"serviceCode\">serviceCode:</label><br>"
            "<input type=\"text\" id=\"serviceCode\" name=\"serviceCode\"><br>"
            "<label for=\"cardUid\">cardUid:</label><br>"
            "<input type=\"text\" id=\"cardUid\" name=\"cardUid\"><br>"
            "<label for=\"uid\">uid:</label><br>"
            "<input type=\"text\" id=\"uid\" name=\"uid\"><br>"
            "<label for=\"token\">token:</label><br>"
            "<input type=\"text\" id=\"token\" name=\"token\"><br>"
            "<input type=\"submit\" value=\"Test API Attendance\">"
        "</form>"
        "<form method=\"post\" action=\"/test_api_login_admin\">"
            "<label for=\"apiLoginUrl\">API Login URL:</label><br>"
            "<input type=\"text\" id=\"apiLoginUrl\" name=\"apiLoginUrl\"><br>"
            "<label for=\"partnerCode\">partnerCode:</label><br>"
            "<input type=\"text\" id=\"partnerCode\" name=\"partnerCode\"><br>"
            "<label for=\"accessCode\">accessCode:</label><br>"
            "<input type=\"text\" id=\"accessCode\" name=\"accessCode\"><br>"
            "<input type=\"submit\" value=\"Test API Login\">"
        "</form>"
        "</div>"
        "<div style=\"flex: 1; margin-right: 10px;\">"
        "<h3>Current Config:</h3>"
        "<p id=\"current_ssid\">SSID: %s</p>"
        "<p id=\"current_password\">Password: %s</p>"
        "<p id=\"current_DeviceId\">DeviceId: %s</p>"
        "<p id=\"current_ServiceCode\">ServiceCode: %s</p>"
        "<p id=\"current_api_login_url\">API Login URL: %s</p>"
        "<p id=\"current_api_attendance_url\">API Attendance URL: %s</p>"
        "<p id=\"current_adminPass\">Admin Password: %s</p>"
        "<p id=\"current_supporterPass\">Supporter Password: %s</p>"
        "<p id=\"current_accessCode\">Access Code: %s</p>"
        "<p id=\"current_partnerCode\">Partner Code: %s</p>"
        "</div>"
        "</div>"
        "</body>"
        "</html>",
        device->ssid, device->pass, device->deviceId, device->serviceCode, device->api_login_url, device->api_attendance_url, device->adminPass, device->supporterPass, device->accessCode, device->partnerCode,device->merchantCard,
        device->ssid, device->pass, device->deviceId, device->serviceCode, device->api_login_url, device->api_attendance_url, device->adminPass, device->supporterPass, device->accessCode, device->partnerCode
    );
    ESP_LOGI(TAG, "Generated HTML: %s", html);
    FILE *f = fopen("/storage/admin_config.html", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "%s", html);
    fclose(f);
}

void generate_auth_admin_header(char* username, char* admin_pass) {
    char credentials[64];
    sprintf(credentials, "%s:%s", username, admin_pass);
    unsigned char base64_output[128];
    size_t base64_output_length;
    mbedtls_base64_encode(base64_output, sizeof(base64_output), &base64_output_length, (unsigned char*)credentials, strlen(credentials));
    base64_output[base64_output_length] = '\0';
    sprintf(expected_auth_admin_header, "Basic %s", base64_output);
}

void generate_auth_supporter_header(char* username, char* supporter_pass) {
    char credentials[64];
    sprintf(credentials, "%s:%s", username, supporter_pass);
    unsigned char base64_output[128];
    size_t base64_output_length;
    mbedtls_base64_encode(base64_output, sizeof(base64_output), &base64_output_length, (unsigned char*)credentials, strlen(credentials));
    base64_output[base64_output_length] = '\0';
    sprintf(expected_auth_supporter_header, "Basic %s", base64_output);
}

void generate_supporter_html(Attendance_Device_t* device){
    char html[2048*2];
    sprintf(html,
        "<body>"
        "<h1>ESP32 Web Server</h1>"
        "<h2>Current Config</h2>"
        "<div style=\"display: flex; justify-content: space-between;\">"
        "<div style=\"flex: 1; margin-left: 10px;\">"
        "<p id=\"current_ssid\">SSID: %s</p>"
        "<p id=\"current_password\">Password: %s</p>"
        "<p id=\"current_DeviceId\">DeviceId: %s</p>"
        "<p id=\"current_ServiceCode\">ServiceCode: %s</p>"
        "<p id=\"current_api_login_url\">API Login URL: %s</p>"
        "<p id=\"current_api_attendance_url\">API Attendance URL: %s</p>"
        "<p id=\"current_accessCode\">Access Code: %s</p>"
        "<p id=\"current_partnerCode\">Partner Code: %s</p>"
        "</form>"
        "<form method=\"post\" action=\"/logout\">"
        "<input type=\"submit\" value=\"Get Back\">"
        "</form>"
        "</form>"
        "<form method=\"post\" action=\"/supporter_config\">"
        "<input type=\"submit\" value=\"Test API on Device\">"
        "</form>"
        "</div>"
        "</body>"
        "</html>",
        device->ssid, device->pass, device->deviceId, device->serviceCode, device->api_login_url?"Yes":"No", device->api_attendance_url?"Yes":"No", 
        device->accessCode?"Yes":"No", device->partnerCode?"Yes":"No");
    ESP_LOGI(TAG, "Generated HTML: %s", html);
    FILE *f = fopen("/storage/supporter_config.html", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "%s", html);
    fclose(f);
}

char *urlDecode(const char *str) {
  //int d = 0; /* whether or not the string is decoded */

  char *dStr = (char *) malloc(strlen(str) + 1);
  char eStr[] = "00"; /* for a hex code */

  strcpy(dStr, str);

  //while(!d) {
    //d = 1;
    int i; /* the counter for the string */

    for(i=0;i<strlen(dStr);++i) {

      if(dStr[i] == '%') {
        if(dStr[i+1] == 0)
          return dStr;

        if(isxdigit((unsigned char)dStr[i+1]) && isxdigit((unsigned char)dStr[i+2])) {

          //d = 0;

          /* combine the next to numbers into one */
          eStr[0] = dStr[i+1];
          eStr[1] = dStr[i+2];

          /* convert it to decimal */
          long int x = strtol(eStr, NULL, 16);

          /* remove the hex */
          memmove(&dStr[i+1], &dStr[i+3], strlen(&dStr[i+3])+1);

          dStr[i] = x;
        }
      }
      else if(dStr[i] == '+') { dStr[i] = ' '; }
    }
  //}
  return dStr;
}

void get_config(Attendance_Device_t *device){
    FILE* f=fopen("/storage/api_attendance_url.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get API attendance url from file");
    }
    else{
        fseek(f,0,SEEK_END); // Move the file pointer to the beginning of the file (rewind the file)
        if (ftell(f) == 0) {
            ESP_LOGI(TAG, "File is empty");
            fclose(f);
        }
        else
        {
            fseek(f,0,SEEK_SET); // Move the file pointer to the beginning of the file
            char line[512];
            fgets(line,sizeof(line),f);
            fclose(f);
            device->api_attendance_url=strdup(line);   
        }
    }
    f=fopen("/storage/api_login_url.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get API login url from file");
    }
    else {
        fseek(f,0,SEEK_END); // Move the file pointer to the beginning of the file (rewind the file)
        if (ftell(f) == 0) {
            ESP_LOGI(TAG, "File is empty");
            fclose(f);
        }
        else
        {
            fseek(f,0,SEEK_SET); // Move the file pointer to the beginning of the file
            char line[512];
            fgets(line,sizeof(line),f);
            fclose(f);
            device->api_login_url=strdup(line);   
        }
    }
    f=fopen("/storage/ssid.txt","r");
    if (f==NULL){
        ESP_LOGI(TAG, "Failed to get ssid wifi from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->ssid=strdup(line);
    }
    f=fopen("/storage/pass.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get password wifi from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->pass=strdup(line);
    }
    f=fopen("/storage/deviceid.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Device Id from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->deviceId=strdup(line);
    }
    f=fopen("/storage/servicecode.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to Service Code from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->serviceCode=strdup(line);
    }
    f=fopen("/storage/admin_pass.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Admin password from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->adminPass=strdup(line);
    }
    f=fopen("/storage/supporter_pass.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Supporter password from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->supporterPass=strdup(line);
    }  
    f=fopen("/storage/accessCode.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Access Code from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->accessCode=strdup(line);
    }
    f=fopen("/storage/partnerCode.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Partner Code from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->partnerCode=strdup(line);
    }
    f=fopen("/storage/merchantCard.txt","r");
    if(f==NULL){
        ESP_LOGI(TAG, "Failed to get Merchant Card from file");
    }
    else
    {
        char line[64];
        fgets(line,sizeof(line),f);
        fclose(f);
        device->merchantCard=strdup(line);
    }
    ESP_LOGI(TAG,"Get config success");
    device->adminPass=urlDecode(device->adminPass);
    if (device->api_attendance_url) device->api_attendance_url=urlDecode(device->api_attendance_url);
    if (device->api_login_url) device->api_login_url=urlDecode(device->api_login_url);
    device->supporterPass=urlDecode(device->supporterPass);
    device->ssid=urlDecode(device->ssid);
    device->pass=urlDecode(device->pass);
    device->deviceId=urlDecode(device->deviceId);
    device->serviceCode=urlDecode(device->serviceCode);
    device->accessCode=urlDecode(device->accessCode);
    device->partnerCode=urlDecode(device->partnerCode);
    device->merchantCard=urlDecode(device->merchantCard);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t redirect_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/choose_mode");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t redirect = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = redirect_get_handler,
};

/* An HTTP GET handler */
esp_err_t admin_config_get_handler(httpd_req_t *req)
{
    // Check if the Authorization header is present
    char auth_header[100]; // Adjust the size as needed
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) == ESP_OK) {
        // The Authorization header is present. Check if it contains the correct credentials.
        // The credentials should be in the format "Basic base64(username:password)"
        // For "admin:admin", this is "Basic YWRtaW46YWRtaW4="
        ESP_LOGI(TAG, "Authorization header: %s!!!!!!!", auth_header);
        ESP_LOGI(TAG,"Expected Auth Admin Header: %s",expected_auth_admin_header);
        ESP_LOGI(TAG,"String compare: %d",strcmp(auth_header, expected_auth_admin_header));
        if (strcmp(auth_header, expected_auth_admin_header)==0) {
            generate_admin_html(device_to_use);
            // The credentials are correct. Handle the request...
            FILE* f = fopen("/storage/admin_config.html", "r");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                httpd_resp_send_chunk(req, line, strlen(line));
            }
            httpd_resp_send_chunk(req, NULL, 0); // Send empty chunk to signal end of response
            return ESP_OK;
        } else {
            // The credentials are incorrect. Send a 401 Unauthorized response with a WWW-Authenticate header.
            httpd_resp_set_status(req, "401 Unauthorized");
            httpd_resp_set_hdr(req, "WWW-Authenticate","Basic realm=\"Access to the admin area.\",charset=\"UTF-8\"");
            httpd_resp_send(req, NULL, 0);
        }
    } else {
        // The Authorization header is not present. Send a 401 Unauthorized response with a WWW-Authenticate header.
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate","Basic realm=\"Access to the admin area.\",charset=\"UTF-8\"");
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}
httpd_uri_t admin_config_uri = {
    .uri       = "/admin_config",
    .method    = HTTP_GET,
    .handler   = admin_config_get_handler,
    .user_ctx  = NULL
};

/* An HTTP POST handler for the /submit URI */
esp_err_t admin_config_submit_handler(httpd_req_t *req)
{
    char auth_header[100]; // Adjust the size as needed
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) == ESP_OK) {
        // The Authorization header is present. Check if it contains the correct credentials.
        // The credentials should be in the format "Basic base64(username:password)"
        // For "admin:admin", this is "Basic YWRtaW46YWRtaW4="
        ESP_LOGI(TAG, "Authorization header: %s", auth_header);
        if (strcmp(auth_header, expected_auth_admin_header)!=0) {
            // The credentials are incorrect. Send a 401 Unauthorized response with a WWW-Authenticate header.
            httpd_resp_set_status(req, "401 not authorized");
            httpd_resp_set_hdr(req, "WWW-Authenticate","Basic realm=\"Access to the admin area.\",charset=\"UTF-8\"");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }
    char buf[1024];
    int ret, remaining = req->content_len;
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }

        /* Process the data */
        ESP_LOGI(TAG, "Received %.*s", ret, buf);
        
        // Parse the received data
        char *ssid_start = strstr(buf, "ssid=");
        char *password_start = strstr(buf, "password=");
        char *deviceid_start = strstr(buf, "DeviceId=");
        char *servicecode_start = strstr(buf, "ServiceCode=");
        char *api_attendance_url_start = strstr(buf, "api_attendance_url=");
        char *api_login_url_start = strstr(buf, "api_login_url=");
        char *admin_pass_start = strstr(buf, "adminPass=");
        char *supporter_pass_start = strstr(buf, "supporterPass=");
        char *accessCode_start = strstr(buf, "accessCode=");
        char *partnerCode_start = strstr(buf, "partnerCode=");
        char *merchantCard_start = strstr(buf, "merchantCard=");
        if (ssid_start && password_start && deviceid_start && servicecode_start && api_attendance_url_start && api_login_url_start && admin_pass_start && supporter_pass_start && accessCode_start && partnerCode_start) {
            ssid_start += 5;  // Skip past "ssid="
            password_start += 9;  // Skip past "password="
            deviceid_start += 9;  // Skip past "DeviceId="
            servicecode_start += 12;  // Skip past "ServiceCode="
            api_attendance_url_start += 19;  // Skip past "api_attendance_url="
            api_login_url_start += 14;  // Skip past "api_login_url="
            admin_pass_start += 10;  // Skip past "adminPass="
            supporter_pass_start += 14;  // Skip past "supporterPass="
            accessCode_start += 11;  // Skip past "accessCode="
            partnerCode_start += 12;  // Skip past "partnerCode="
            merchantCard_start += 13;  // Skip past "merchantCard="
            // Find the end of the ssid and password
            char *ssid_end = strchr(ssid_start, '&');
            char *password_end = strchr(password_start, '&');
            char *deviceid_end = strchr(deviceid_start, '&');
            char *servicecode_end = strchr(servicecode_start, '&');
            char *api_attendance_url_end = strchr(api_attendance_url_start, '&');
            char *api_login_url_end = strchr(api_login_url_start, '&');
            char *admin_pass_end = strchr(admin_pass_start, '&');
            char *supporter_pass_end = strchr(supporter_pass_start, '&');
            char *accessCode_end = strchr(accessCode_start, '&');
            char *partnerCode_end = strchr(partnerCode_start, '&');
            char *merchantCard_end = strchr(merchantCard_start, '&');
            if (!ssid_end) ssid_end = buf + ret;
            if (!password_end) password_end = buf + ret;
            if (!deviceid_end) deviceid_end = buf + ret;
            if (!servicecode_end) servicecode_end = buf + ret;
            if (!api_attendance_url_end) api_attendance_url_end = buf + ret;
            if (!api_login_url_end) api_login_url_end = buf + ret;
            if (!admin_pass_end) admin_pass_end = buf + ret;
            if (!supporter_pass_end) supporter_pass_end = buf + ret;
            if (!accessCode_end) accessCode_end = buf + ret;
            if (!partnerCode_end) partnerCode_end = buf + ret;
            if (!merchantCard_end) merchantCard_end = buf + ret;
            // Write the ssid to ssid.txt
            FILE *f = fopen("/storage/ssid.txt", "w");
            if (f) {
                size_t written = fwrite(ssid_start, 1, ssid_end - ssid_start, f);
                fclose(f);
                if (written != ssid_end - ssid_start) {
                    ESP_LOGE(TAG, "Failed to write SSID to ssid.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open ssid.txt for writing");
            }

            // Write the password to pass.txt
            f = fopen("/storage/pass.txt", "w");
            if (f) {
                size_t written = fwrite(password_start, 1, password_end - password_start, f);
                fclose(f);
                if (written != password_end - password_start) {
                    ESP_LOGE(TAG, "Failed to write password to pass.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open pass.txt for writing");
            }

            f = fopen("/storage/deviceid.txt", "w");
            if (f) {
                size_t written = fwrite(deviceid_start, 1, deviceid_end - deviceid_start, f);
                fclose(f);
                if (written != deviceid_end - deviceid_start) {
                    ESP_LOGE(TAG, "Failed to write DeviceId to deviceid.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open deviceid.txt for writing");
            }

            f = fopen("/storage/servicecode.txt", "w");
            if(f){
                size_t written = fwrite(servicecode_start, 1, servicecode_end - servicecode_start, f);
                fclose(f);
                if(written != servicecode_end - servicecode_start){
                    ESP_LOGE(TAG, "Failed to write ServiceCode to servicecode.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open servicecode.txt for writing");
            }

            f =fopen("/storage/api_attendance_url.txt","w");
            if(f){
                size_t written = fwrite(api_attendance_url_start, 1, api_attendance_url_end - api_attendance_url_start, f);
                fclose(f);
                if(written != api_attendance_url_end - api_attendance_url_start){
                    ESP_LOGE(TAG, "Failed to write API Attendance URL to api_attendance_url.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open api_attendance_url.txt for writing");
            } 

            f=fopen("/storage/api_login_url.txt","w");
            if(f){
                size_t written = fwrite(api_login_url_start, 1, api_login_url_end - api_login_url_start, f);
                fclose(f);
                if(written != api_login_url_end - api_login_url_start){
                    ESP_LOGE(TAG, "Failed to write API Login URL to api_login_url.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open api_login_url.txt for writing");
            }

            f=fopen("/storage/admin_pass.txt","w");
            if(f){
                size_t written = fwrite(admin_pass_start, 1, admin_pass_end - admin_pass_start, f);
                fclose(f);
                if(written != admin_pass_end - admin_pass_start){
                    ESP_LOGE(TAG, "Failed to write Admin password to admin_pass.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open admin_pass.txt for writing");
            }
            f=fopen("/storage/supporter_pass.txt","w");
            if(f){
                size_t written = fwrite(supporter_pass_start, 1, supporter_pass_end - supporter_pass_start, f);
                fclose(f);
                if(written != supporter_pass_end - supporter_pass_start){
                    ESP_LOGE(TAG, "Failed to write Supporter password to supporter_pass.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open supporter_pass.txt for writing");
            }
            f=fopen("/storage/accessCode.txt","w");
            if(f){
                size_t written = fwrite(accessCode_start, 1, accessCode_end - accessCode_start, f);
                fclose(f);
                if(written != accessCode_end - accessCode_start){
                    ESP_LOGE(TAG, "Failed to write Access Code to accessCode.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open accessCode.txt for writing");
            }
            f=fopen("/storage/partnerCode.txt","w");
            if(f){
                size_t written = fwrite(partnerCode_start, 1, partnerCode_end - partnerCode_start, f);
                fclose(f);
                if(written != partnerCode_end - partnerCode_start){
                    ESP_LOGE(TAG, "Failed to write Partner Code to partnerCode.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open partnerCode.txt for writing");
            }      
            f=fopen("/storage/merchantCard.txt","w");
            if(f){
                size_t written = fwrite(merchantCard_start, 1, merchantCard_end - merchantCard_start, f);
                fclose(f);
                if(written != merchantCard_end - merchantCard_start){
                    ESP_LOGE(TAG, "Failed to write Merchant Card to merchantCard.txt");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open merchantCard.txt for writing");
            } 
        } else {
            ESP_LOGE(TAG, "Failed to parse SSID and password from received data");
        }
        remaining -= ret;
    }
    /* Send a response */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/choose_mode");
    httpd_resp_send(req, NULL, 0);
    vTaskDelay(1000/ portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}
httpd_uri_t admin_submit_uri = {
    .uri       = "/admin_config",
    .method    = HTTP_POST,
    .handler   = admin_config_submit_handler,
    .user_ctx  = NULL
};

esp_err_t supporter_config_get_handler(httpd_req_t *req){
    char auth_header[100]; // Adjust the size as needed
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) == ESP_OK) {
        // The Authorization header is present. Check if it contains the correct credentials.
        // The credentials should be in the format "Basic base64(username:password)"
        // For "admin:admin", this is "Basic YWRtaW46YWRtaW4="
        ESP_LOGI(TAG, "Authorization header: %s", auth_header);
        if (strcmp(auth_header, expected_auth_supporter_header)!=0) {
            // The credentials are incorrect. Send a 401 Unauthorized response with a WWW-Authenticate header.
            httpd_resp_set_status(req, "401 not authorized");
            httpd_resp_set_hdr(req, "WWW-Authenticate","Basic realm=\"Access to the supporter area.\",charset=\"UTF-8\"");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }
    generate_supporter_html(device_to_use);
    FILE* f = fopen("/storage/supporter_config.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_send_chunk(req, line, strlen(line));
    }
    httpd_resp_send_chunk(req, NULL, 0); // Send empty chunk to signal end of response
    return ESP_OK;
}

httpd_uri_t supporter_config_get_uri = {
    .uri       = "/supporter_config",
    .method    = HTTP_GET,
    .handler   = supporter_config_get_handler,
    .user_ctx  = NULL
};

esp_err_t supporter_config_post_handler(httpd_req_t *req) {
    send_post_login_request(device_to_use);
    char attendance_response[256];
    char response[512]; // Increase the size to accommodate both responses
    if (strcmp(response_success, "true") == 0)
    {
        sprintf(response, "Login, Success: %s, Message: %s", response_success, response_message);
        send_post_test_request(device_to_use);
        if (strcmp(response_success, "false") == 0)
        {
            sprintf(attendance_response, "Attendance, Success: %s, Code: %s", response_success, response_code);
            strcat(response, "<br>"); // Add a line break between the responses
            strcat(response, attendance_response); // Append the attendance response to the login response
        }
        else
        {
            sprintf(attendance_response, "Attendance, Success: %s, Message: %s", response_success, response_message);
            strcat(response, "<br>"); // Add a line break between the responses
            strcat(response, attendance_response); // Append the attendance response to the login response
        }
    }
    else
    {
        sprintf(response, "Login, Success: %s, Code: %s", response_success, response_code);
        strcat(response, "<br>"); // Add a line break between the responses
        strcat(response, "Attendance, Success: false, Message: Login failed"); // Append the attendance response to the login response
    }
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t supporter_config_post_uri = {
    .uri       = "/supporter_config",
    .method    = HTTP_POST,
    .handler   = supporter_config_post_handler,
    .user_ctx  = NULL
};

// This is the handler for the login page
esp_err_t choose_mode_get_handler(httpd_req_t *req) {
    FILE* f = fopen("/storage/choose_mode.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_send_chunk(req, line, strlen(line));
    }
    httpd_resp_send_chunk(req, NULL, 0); // Send empty chunk to signal end of response

    fclose(f);
    return ESP_OK;
}

// This is the handler for the login request
esp_err_t choose_mode_post_handler(httpd_req_t *req) {
    char buf[100]; // Adjust the size as needed
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Received data: %s", buf);
    if (strcmp(buf, "Admin") == 0) {
        // If the Admin button was clicked, redirect to the admin_config page...
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/admin_config");
        httpd_resp_send(req, NULL, 0);
    } else if (strcmp(buf, "Support") == 0) {
        // If the Support button was clicked, redirect to the supporter_config page...
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/supporter_config");
        httpd_resp_send(req, NULL, 0);
    } else {
        // If neither button was clicked, send a 400 Bad Request response.
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

httpd_uri_t choose_mode_get_uri = {
    .uri       = "/choose_mode",
    .method    = HTTP_GET,
    .handler   = choose_mode_get_handler,
    .user_ctx  = NULL
};

// Register the login request handler
httpd_uri_t choose_mode_post_uri = {
    .uri       = "/choose_mode",
    .method    = HTTP_POST,
    .handler   = choose_mode_post_handler,
};

esp_err_t logout_handler(httpd_req_t *req)
{
    // Then, send a 303 See Other status to redirect the client to choose_mode
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/choose_mode");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t logout_uri = {
    .uri       = "/logout",
    .method    = HTTP_POST,
    .handler   = logout_handler,
};

esp_err_t test_api_login_admin_handler(httpd_req_t* req){
    char buf[1024];
    int ret=0;
    int remaining = req->content_len;
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;  // Decrement remaining by the number of bytes read
    }
        /* Process the data */
    ESP_LOGI(TAG, "Received %.*s", ret, buf);
    char *api_login_url_start = strstr(buf, "apiLoginUrl=");
    char *accessCode_start = strstr(buf, "accessCode=");
    char *partnerCode_start = strstr(buf, "partnerCode=");
    api_login_url_start += 12;  // Skip past "api_login_url="
    accessCode_start += 11;  // Skip past "accessCode="
    partnerCode_start += 12;  // Skip past "partnerCode="
    char *api_login_url_end = strchr(api_login_url_start, '&');
    char *accessCode_end = strchr(accessCode_start, '&');
    char *partnerCode_end = strchr(partnerCode_start, '&');
    if (!api_login_url_end) api_login_url_end = buf + ret;
    if (!accessCode_end) accessCode_end = buf + ret;
    if (!partnerCode_end) partnerCode_end = buf + ret;
    char* api_login_url_test=malloc(api_login_url_end-api_login_url_start+1);
    char* accessCode_test=malloc(accessCode_end-accessCode_start+1);
    char* partnerCode_test=malloc(partnerCode_end-partnerCode_start+1);
    strncpy(api_login_url_test,api_login_url_start,api_login_url_end-api_login_url_start);
    strncpy(accessCode_test,accessCode_start,accessCode_end-accessCode_start);
    strncpy(partnerCode_test,partnerCode_start,partnerCode_end-partnerCode_start);
    api_login_url_test[api_login_url_end-api_login_url_start]='\0';
    accessCode_test[accessCode_end-accessCode_start]='\0';
    partnerCode_test[partnerCode_end-partnerCode_start]='\0';
    api_login_url_test=urlDecode(api_login_url_test);
    accessCode_test=urlDecode(accessCode_test);
    partnerCode_test=urlDecode(partnerCode_test);
    ESP_LOGI(TAG, "API Login URL: %s", api_login_url_test);
    ESP_LOGI(TAG, "Access Code: %s", accessCode_test);
    ESP_LOGI(TAG,"Partner Code: %s", partnerCode_test);
    send_post_login_test_request(api_login_url_test,accessCode_test,partnerCode_test);
    char response[512];
    if(strcmp(response_success,"true")==0){
        sprintf(response, "Login, Success: %s, Message: %s, Token: %s", response_success, response_message,token_test);
        httpd_resp_send(req,response,HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    else{
        sprintf(response, "Login, Success: %s, Code: %s", response_success, response_code);
        httpd_resp_send(req,response,HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    return ESP_OK;
}   

httpd_uri_t test_api_login_admin_uri={
    .uri       = "/test_api_login_admin",
    .method    = HTTP_POST,
    .handler   = test_api_login_admin_handler,
};

esp_err_t test_api_attendance_admin_handler(httpd_req_t* req){
    char buf[1024];
    int ret=0;
    int remaining = req->content_len;
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;  // Decrement remaining by the number of bytes read
    }
        /* Process the data */
    ESP_LOGI(TAG, "Received %.*s", ret, buf);
    char *api_attendance_url_start = strstr(buf, "apiAttendanceUrl=");
    char *token_start = strstr(buf, "token=");
    char *deviceId_start = strstr(buf, "deviceId=");
    char *serviceCode_start = strstr(buf, "serviceCode=");
    char *cardUid_start = strstr(buf, "cardUid=");
    char *Uid_start=strstr(buf,"uid=");
    api_attendance_url_start += 17;  // Skip past "api_attendance_url="
    token_start += 6;  // Skip past "token="
    deviceId_start += 9;  // Skip past "deviceId="
    serviceCode_start += 12;  // Skip past "serviceCode="
    cardUid_start += 8;  // Skip past "cardUid="
    Uid_start+=4; // Skip past "Uid="
    char *api_attendance_url_end = strchr(api_attendance_url_start, '&');
    char *token_end = strchr(token_start, '&');
    char *deviceId_end = strchr(deviceId_start, '&');
    char *serviceCode_end = strchr(serviceCode_start, '&');
    char *cardUid_end = strchr(cardUid_start, '&');
    char *Uid_end=strchr(Uid_start,'&');
    if (!api_attendance_url_end) api_attendance_url_end = buf + ret;
    if (!token_end) token_end = buf + ret;
    if (!deviceId_end) deviceId_end = buf + ret;
    if (!serviceCode_end) serviceCode_end = buf + ret;
    if (!cardUid_end) cardUid_end = buf + ret;
    if (!Uid_end) Uid_end=buf+ret;
    char* api_attendance_url_test=malloc(api_attendance_url_end-api_attendance_url_start+1);
    char* token_test=malloc(token_end-token_start+1);
    char* deviceId_test=malloc(deviceId_end-deviceId_start+1);
    char* serviceCode_test=malloc(serviceCode_end-serviceCode_start+1);
    char* uid_card_admin_test=malloc(cardUid_end-cardUid_start+1);
    char* uid_admin_test=malloc(Uid_end-Uid_start+1);
    strncpy(api_attendance_url_test,api_attendance_url_start,api_attendance_url_end-api_attendance_url_start);
    strncpy(token_test,token_start,token_end-token_start);
    strncpy(deviceId_test,deviceId_start,deviceId_end-deviceId_start);
    strncpy(serviceCode_test,serviceCode_start,serviceCode_end-serviceCode_start);
    strncpy(uid_card_admin_test,cardUid_start,cardUid_end-cardUid_start);
    strncpy(uid_admin_test,Uid_start,Uid_end-Uid_start);
    api_attendance_url_test[api_attendance_url_end-api_attendance_url_start]='\0';
    token_test[token_end-token_start]='\0';
    deviceId_test[deviceId_end-deviceId_start]='\0';
    serviceCode_test[serviceCode_end-serviceCode_start]='\0';
    uid_card_admin_test[cardUid_end-cardUid_start]='\0';
    uid_admin_test[Uid_end-Uid_start]='\0';
    api_attendance_url_test=urlDecode(api_attendance_url_test);
    token_test=urlDecode(token_test);
    deviceId_test=urlDecode(deviceId_test);
    serviceCode_test=urlDecode(serviceCode_test);
    uid_card_admin_test=urlDecode(uid_card_admin_test);
    uid_admin_test=urlDecode(uid_admin_test);
    ESP_LOGI(TAG, "API Attendance URL: %s", api_attendance_url_test);
    ESP_LOGI(TAG, "Token: %s", token_test);
    ESP_LOGI(TAG, "Device Id: %s", deviceId_test);
    ESP_LOGI(TAG, "Service Code: %s", serviceCode_test);
    ESP_LOGI(TAG, "Card Uid: %s", uid_card_admin_test);
    ESP_LOGI(TAG, "Uid: %s", uid_admin_test);
    send_post_test_admin_request(api_attendance_url_test,deviceId_test,uid_card_admin_test, uid_admin_test, serviceCode_test, token_test);
    char response[512];
    if(strcmp(response_success,"true")==0){
        sprintf(response, "Attendance, Success: %s, Message: %s", response_success, response_message);
        httpd_resp_send(req,response,HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    else{
        sprintf(response, "Attendance, Success: %s, Code: %s", response_success, response_code);
        httpd_resp_send(req,response,HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
}

httpd_uri_t test_api_attendance_admin_uri={
    .uri       = "/test_api_attendance_admin",
    .method    = HTTP_POST,
    .handler   = test_api_attendance_admin_handler,
};

void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 20480; // Increase the stack size
    config.max_uri_handlers=15;
    // Start the httpd server
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // Register URI handlers

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &admin_config_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &admin_submit_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &choose_mode_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &choose_mode_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &redirect));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &logout_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supporter_config_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supporter_config_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &test_api_login_admin_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &test_api_attendance_admin_uri));
}

void spiffs_init()
{
    esp_vfs_spiffs_conf_t config={
        .base_path="/storage",
        .partition_label=NULL,
        .max_files=5,
        .format_if_mount_failed=true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&config);

    if (ret!=ESP_OK){
        ESP_LOGI(TAG, "Failed to intialize SPIFFS (%s)",esp_err_to_name(ret));
        return;
    }

    size_t total=0, used=0;
    ret = esp_spiffs_info(config.partition_label,&total,&used);
    if(ret!=ESP_OK)
    {
        ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d",total,used);
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_init_sta_ap(Attendance_Device_t *device) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Initialize the WiFi library
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configure the WiFi interface in STA mode
    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strncpy((char *)wifi_sta_config.sta.ssid, device->ssid, sizeof(wifi_sta_config.sta.ssid));
    strncpy((char *)wifi_sta_config.sta.password, device->pass, sizeof(wifi_sta_config.sta.password));

    // Configure the WiFi interface in AP mode
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    // Set the WiFi mode to AP+STA and configure the interfaces
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));

    // Start the WiFi interfaces
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for the connection to the AP to be established
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", device->ssid, device->pass);
        device->connected=true;
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", device->ssid, device->pass);
        device->connected=false;
        return false;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        device->connected=false;
        return false;
    }
    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void Start_connection(Attendance_Device_t *device)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    spiffs_init();
    get_config(device);
    device_to_use = device;
    ESP_LOGI(TAG,"Admin Pass: %s",device->adminPass);
    ESP_LOGI(TAG,"Supporter Pass: %s",device->supporterPass);
    if (device->api_attendance_url != NULL) ESP_LOGI(TAG,"API Attendance URL: %s",device->api_attendance_url);
    else ESP_LOGI(TAG,"API Attendance URL is NULL");
    if (device->api_login_url != NULL)  ESP_LOGI(TAG,"API Login URL: %s",device->api_login_url);
    else ESP_LOGI(TAG,"API Login URL is NULL");
    ESP_LOGI(TAG, "SSID: %s",device->ssid);
    ESP_LOGI(TAG,"Password: %s",device->pass);
    ESP_LOGI(TAG,"DeviceId: %s",device->deviceId);
    ESP_LOGI(TAG,"ServiceCode: %s",device->serviceCode);
    ESP_LOGI(TAG,"Merchant Card: %s",device->merchantCard);
    generate_auth_admin_header("admin",device->adminPass);
    generate_auth_supporter_header("supporter",device->supporterPass);
    if (wifi_init_sta_ap(device)) {
        ESP_LOGI(TAG, "Connected to WiFi network");
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi network: %s",esp_err_to_name(ret));
    }
    ESP_LOGI(TAG,"Hosting at 192.168.4.1 now!");
    start_webserver();
}

