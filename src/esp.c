#include "esp.h"
#include "serial.h"
#include <stdio.h>

#define ESP_SERIAL_READ_BUFFER_SIZE 1024
#define ESP_SERIAL_WRITE_BUFFER_SIZE 1024
#define ESP_TOGGLE_PIN_FORMAT "{\"action\": \"%s\", \"pin\": %i}"
#define ESP_GET_SENSOR_FORMAT "{\"action\": \"get\", \"sensor\": \"%s\", \"pin\": %i, \"model\": \"%s\"}"

enum {
    ESP_RESPONSE_RC,
    ESP_RESPONSE_MSG,
    ESP_RESPONSE_DATA,
    __ESP_RESPONSE_MAX,
};

struct EspResponse {
    bool success;
    char *message;
    char *data;
};

static const struct blobmsg_policy
esp_response_verification_policy[] = {
    [ESP_RESPONSE_RC] = {.name = "rc", .type = BLOBMSG_TYPE_INT32},
    [ESP_RESPONSE_MSG] = {.name = "msg", .type = BLOBMSG_TYPE_STRING},
    [ESP_RESPONSE_DATA] = {.name = "data", .type = BLOBMSG_TYPE_TABLE},
};

static struct EspResponse EspResponse_new(void);

static void EspResponse_free(struct EspResponse *esp_response);

struct EspActionResult
execute_esp_action(struct EspAction action) {
    struct EspActionResult result = {
        .usb_result = USB_RESULT_OK,
        .esp_response_string = NULL
    };

    struct sp_port *port = NULL;
    result.usb_result = get_esp_port_by_name(action.port_name, &port);
    if (result.usb_result != USB_RESULT_OK) {
        return result;
    }

    char serial_write_buf[1024];
    switch (action.action_type) {
        case ESP_ACTION_ON:
            snprintf(
                serial_write_buf,
                sizeof(serial_write_buf),
                ESP_TOGGLE_PIN_FORMAT,
                "on",
                action.pin
            );
            break;
        case ESP_ACTION_OFF:
            snprintf(
                serial_write_buf,
                sizeof(serial_write_buf),
                ESP_TOGGLE_PIN_FORMAT,
                "off",
                action.pin
            );
            break;
        case ESP_ACTION_GET_SENSOR:
            snprintf(
                serial_write_buf,
                sizeof(serial_write_buf),
                ESP_GET_SENSOR_FORMAT,
                action.sensor,
                action.pin,
                action.model
            );
            break;
    }

    char *serial_read_buf = (char *) calloc(ESP_SERIAL_READ_BUFFER_SIZE, sizeof(char));
    if (serial_read_buf == NULL) {
        goto cleanup_port;
    }

    result.usb_result = open_port(port);
    if (result.usb_result != USB_RESULT_OK) {
        free(serial_read_buf);
        serial_read_buf = NULL;
        goto cleanup_open_port;
    }

    result.usb_result = write_and_await_response(
        port,
        serial_write_buf,
        strlen(serial_write_buf),
        serial_read_buf,
        ESP_SERIAL_READ_BUFFER_SIZE
    );
    if (result.usb_result != USB_RESULT_OK) {
        free(serial_read_buf);
        serial_read_buf = NULL;
    }
    result.esp_response_string = serial_read_buf;

cleanup_open_port:
    sp_close(port);
cleanup_port:
    sp_free_port(port);

    return result;
}

static bool
parse_esp_response(char *esp_response_json_string, struct EspResponse *esp_response) {
    bool parse_success = true;
    struct blob_buf blob_buf = {};
    blob_buf_init(&blob_buf, 0);
    // blobbuf takes ownership of esp_response_json_string
    blobmsg_add_json_from_string(&blob_buf, esp_response_json_string);
    struct blob_attr *tb[__ESP_RESPONSE_MAX];
    blobmsg_parse(
        esp_response_verification_policy,
        __ESP_RESPONSE_MAX,
        tb,
        blob_data(blob_buf.head),
        blobmsg_len(blob_buf.head)
    );
    if (tb[ESP_RESPONSE_RC] == NULL) {
        parse_success = false;
        goto end;
    }
    esp_response->success = blobmsg_get_u32(tb[ESP_RESPONSE_RC]) == 0 ? true : false;

    if (tb[ESP_RESPONSE_MSG] != NULL) {
        char *message = blobmsg_get_string(tb[ESP_RESPONSE_MSG]);
        esp_response->message = (char *) malloc(strlen(message) + 1);
        strcpy(esp_response->message, message);
    }
    if (tb[ESP_RESPONSE_DATA] != NULL) {
        char *data = blobmsg_format_json(tb[ESP_RESPONSE_DATA], true);
        esp_response->data = data;
    }
end:
    blob_buf_free(&blob_buf);

    return parse_success;
}

struct blob_buf *
create_usb_result_message(struct blob_buf *result_blob_buf, enum UsbResult usb_result) {
    blobmsg_add_string(result_blob_buf, "result", "err");
    blobmsg_add_string(result_blob_buf, "message", UsbResult_str[usb_result]);
    return result_blob_buf;
}

struct blob_buf *
create_esp_action_result_message(
    struct blob_buf *result_blob_buf,
    enum EspActionType esp_action,
    struct EspActionResult esp_result)
{
    if (esp_result.usb_result != USB_RESULT_OK) {
        create_usb_result_message(result_blob_buf, esp_result.usb_result);
        return result_blob_buf;
    }

    struct EspResponse esp_response = EspResponse_new();
    if (!parse_esp_response(esp_result.esp_response_string, &esp_response)) {
        blobmsg_add_string(result_blob_buf, "result", "err");
        blobmsg_add_string(result_blob_buf, "message", "Failed to parse ESP response JSON.");
        goto end;
    }

    if (esp_response.success) {
        blobmsg_add_string(result_blob_buf, "result", "ok");
    } else {
        blobmsg_add_string(result_blob_buf, "result", "err");
    }

    if (!esp_response.success && esp_response.message == NULL) {
        blobmsg_add_string(result_blob_buf, "message", "Unknown ESP failure.");
        goto end;
    }
    if (esp_response.message != NULL) {
        blobmsg_add_string(result_blob_buf, "message", esp_response.message);
    }

    switch (esp_action) {
        case ESP_ACTION_GET_SENSOR:
            if (esp_response.data != NULL) {
                void *tb = blobmsg_open_table(result_blob_buf, "data");
                blobmsg_add_json_from_string(result_blob_buf, esp_response.data);
                blobmsg_close_table(result_blob_buf, tb);
            }
            break;
        default:
            break;
    }

end:
    EspResponse_free(&esp_response);

    return result_blob_buf;
}

static struct EspResponse
EspResponse_new(void) {
    struct EspResponse esp_response = {.success = false, .message = NULL, .data = NULL};
    return esp_response;
}

static void
EspResponse_free(struct EspResponse *esp_response) {
    if (esp_response->message != NULL) {
        free(esp_response->message);
    }
    if (esp_response->data != NULL) {
        free(esp_response->data);
    }
}

void
EspActionResult_free(struct EspActionResult *esp_action_result) {
    if (esp_action_result->esp_response_string != NULL) {
        free(esp_action_result->esp_response_string);
        esp_action_result->esp_response_string = NULL;
    }
}
