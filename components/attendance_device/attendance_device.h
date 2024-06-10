// attendance_device.h

#ifndef ATTENDANCE_DEVICE_H
#define ATTENDANCE_DEVICE_H

#include <stdbool.h>

typedef struct {
    char *ssid;
    char *pass;
    char *deviceId;
    char *serviceCode;
    char *token;
    char *adminPass;
    char *supporterPass;
    char *api_login_url;
    char *api_attendance_url;
    char *accessCode;
    char *partnerCode;
    char *merchantCard;
    bool connected;
} Attendance_Device_t;

#endif // ATTENDANCE_DEVICE_H