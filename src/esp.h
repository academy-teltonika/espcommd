#pragma once

#include "serial.h"
#include <libubox/blobmsg_json.h>

enum EspActionType {
    ESP_ACTION_ON,
    ESP_ACTION_OFF,
    ESP_ACTION_GET_SENSOR,
};

struct EspAction {
    enum EspActionType action_type;
    char *port_name;
    int pin;

    char *sensor;
    char *model;
};

struct EspActionResult {
    enum UsbResult usb_result;
    char *esp_response_string;
};

struct EspActionResult
execute_esp_action(struct EspAction action);

struct blob_buf *
create_esp_action_result_message(
    struct blob_buf *result_blob_buf,
    enum EspActionType esp_action,
    struct EspActionResult esp_result
);

struct blob_buf *
create_usb_result_message(
    struct blob_buf *result_blob_buf,
    enum UsbResult usb_result
);

void EspActionResult_free(struct EspActionResult *esp_action_result);
