#include "ubus.h"
#include "serial.h"
#include "esp.h"
#include <assert.h>
#include <libubox/blobmsg_json.h>

static int
devices_get(
    struct ubus_context *ctx,
    struct ubus_object *obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
);

// TODO: move out to esp.c
static int
toggle_pin(
    struct ubus_context *ctx,
    struct ubus_object *obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
);

static int
get_sensor(
    struct ubus_context *ctx,
    struct ubus_object *obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
);

enum {
    ESP_UBUS_TOGGLE_PIN_POLICY_PORT,
    ESP_UBUS_TOGGLE_PIN_POLICY_PIN,
    __ESP_UBUS_TOGGLE_PIN_POLICY_MAX,
};

enum {
    ESP_UBUS_GET_SENSOR_POLICY_PORT,
    ESP_UBUS_GET_SENSOR_POLICY_PIN,
    ESP_UBUS_GET_SENSOR_POLICY_SENSOR,
    ESP_UBUS_GET_SENSOR_POLICY_SENSOR_MODEL,
    __ESP_UBUS_GET_SENSOR_POLICY_MAX,
};

static const struct blobmsg_policy
esp_toggle_pin_policy[] = {
    [ESP_UBUS_TOGGLE_PIN_POLICY_PORT] = {.name = "port", .type = BLOBMSG_TYPE_STRING},
    [ESP_UBUS_TOGGLE_PIN_POLICY_PIN] = {.name = "pin", .type = BLOBMSG_TYPE_INT32},
};

static const struct blobmsg_policy
esp_get_sensor_policy[] = {
    [ESP_UBUS_GET_SENSOR_POLICY_PORT] = {.name = "port", .type = BLOBMSG_TYPE_STRING},
    [ESP_UBUS_GET_SENSOR_POLICY_PIN] = {.name = "pin", .type = BLOBMSG_TYPE_INT32},
    [ESP_UBUS_GET_SENSOR_POLICY_SENSOR] = {.name = "sensor", .type = BLOBMSG_TYPE_STRING},
    [ESP_UBUS_GET_SENSOR_POLICY_SENSOR_MODEL] = {.name = "model", .type = BLOBMSG_TYPE_STRING},
};

static struct ubus_method
esp_methods[] = {
    UBUS_METHOD_NOARG("devices", devices_get),
    UBUS_METHOD("on", toggle_pin, esp_toggle_pin_policy),
    UBUS_METHOD("off", toggle_pin, esp_toggle_pin_policy),
    UBUS_METHOD("get", get_sensor, esp_get_sensor_policy),
};

static struct ubus_object_type
esp_object_type = UBUS_OBJECT_TYPE("esp", esp_methods);

static struct ubus_object
esp_object = {
    .name = "espcommd",
    .type = &esp_object_type,
    .methods = esp_methods,
    .n_methods = ARRAY_SIZE(esp_methods),
};

static int
devices_get(
    struct ubus_context *ctx,
    struct ubus_object *obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
) {
    struct blob_buf blob_buf = {};
    blob_buf_init(&blob_buf, 0);

    struct sp_port **port_list;
    enum UsbResult usb_result = enumerate_esp_serial_ports(&port_list);
    if (usb_result != USB_RESULT_OK) {
        create_usb_result_message(&blob_buf, usb_result);
        goto end;
    }

    void *devices_array = blobmsg_open_array(&blob_buf, "devices");
    for (int i = 0; port_list[i] != NULL; i++) {
        struct sp_port *port = port_list[i];

        void *device_table = blobmsg_open_table(&blob_buf, NULL);
        blobmsg_add_string(&blob_buf, "port", sp_get_port_name(port));

        int vid, pid;
        sp_get_port_usb_vid_pid(port, &vid, &pid); // Should never fail, as we've already checked the vid and pid.
        char vid_pid_buf[32];
        snprintf(vid_pid_buf, sizeof(vid_pid_buf), "%x", vid);
        blobmsg_add_string(&blob_buf, "vid", vid_pid_buf);
        snprintf(vid_pid_buf, sizeof(vid_pid_buf), "%x", pid);
        blobmsg_add_string(&blob_buf, "pid", vid_pid_buf);
        blobmsg_close_table(&blob_buf, device_table);
    }
    blobmsg_close_array(&blob_buf, devices_array);

    end:
    ubus_send_reply(ctx, req, blob_buf.head);
    blob_buf_free(&blob_buf);
    sp_free_port_list(port_list);

    return UBUS_STATUS_OK;
}

// Return 1 for "on", 0 for "off" and -1 if neither.
void
get_pin_target_state_from_ubus_method(int *pin_state, char *method) {
    if (strncmp("on", method, 32) == 0) {
        *pin_state = 1;
        return;
    }
    if (strncmp("off", method, 32) == 0) {
        *pin_state = 0;
        return;
    }

    // Method was not "on" or "off".
    *pin_state = -1;
    return;
}

static int
toggle_pin(
    struct ubus_context *ctx,
    struct ubus_object *_obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
) {
    struct blob_attr *tb[__ESP_UBUS_TOGGLE_PIN_POLICY_MAX];
    blobmsg_parse(
        esp_toggle_pin_policy,
        __ESP_UBUS_TOGGLE_PIN_POLICY_MAX,
        tb,
        blob_data(msg),
        blob_len(msg)
    );
    if (tb[ESP_UBUS_TOGGLE_PIN_POLICY_PORT] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    char *port_name = blobmsg_get_string(tb[ESP_UBUS_TOGGLE_PIN_POLICY_PORT]);

    if (tb[ESP_UBUS_TOGGLE_PIN_POLICY_PIN] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    int pin = blobmsg_get_u32(tb[ESP_UBUS_TOGGLE_PIN_POLICY_PIN]);

    struct blob_buf blob_buf = {};
    blob_buf_init(&blob_buf, 0);

    int pin_target_state = -1;
    get_pin_target_state_from_ubus_method(&pin_target_state, (char*) method);
    assert(pin_target_state != -1); // Ubus method, which called pin_toggle was not on or off.

    enum EspActionType esp_action_type = pin_target_state == 1 ? ESP_ACTION_ON : ESP_ACTION_OFF; 
    struct EspAction esp_action = {
        .action_type = esp_action_type,
        .port_name = port_name,
        .pin = pin,
    };

    struct EspActionResult result = execute_esp_action(esp_action);
    create_esp_action_result_message(&blob_buf, esp_action.action_type, result);
    EspActionResult_free(&result);

    ubus_send_reply(ctx, req, blob_buf.head);
    blob_buf_free(&blob_buf);
        
    return UBUS_STATUS_OK;
}

static int
get_sensor(
    struct ubus_context *ctx,
    struct ubus_object *obj,
    struct ubus_request_data *req,
    const char *method,
    struct blob_attr *msg
) {
    struct blob_attr *tb[__ESP_UBUS_GET_SENSOR_POLICY_MAX];
    blobmsg_parse(
        esp_get_sensor_policy,
        __ESP_UBUS_GET_SENSOR_POLICY_MAX,
        tb,
        blob_data(msg),
        blob_len(msg)
    );
    if (tb[ESP_UBUS_GET_SENSOR_POLICY_PORT] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    char *port_name = blobmsg_get_string(tb[ESP_UBUS_TOGGLE_PIN_POLICY_PORT]);

    if (tb[ESP_UBUS_GET_SENSOR_POLICY_PIN] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    int pin = blobmsg_get_u32(tb[ESP_UBUS_TOGGLE_PIN_POLICY_PIN]);

    if (tb[ESP_UBUS_GET_SENSOR_POLICY_SENSOR] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    char *sensor = blobmsg_get_string(tb[ESP_ACTION_GET_SENSOR]);

    if (tb[ESP_UBUS_GET_SENSOR_POLICY_SENSOR_MODEL] == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }
    char *model = blobmsg_get_string(tb[ESP_UBUS_GET_SENSOR_POLICY_SENSOR_MODEL]);

    struct blob_buf blob_buf = {};
    blob_buf_init(&blob_buf, 0);
    struct EspAction esp_action = {
        .action_type = ESP_ACTION_GET_SENSOR,
        .port_name = port_name,
        .pin = pin,
        .sensor = sensor,
        .model = model
    };
    struct EspActionResult result = execute_esp_action(esp_action);
    create_esp_action_result_message(&blob_buf, esp_action.action_type, result);
    EspActionResult_free(&result);

    ubus_send_reply(ctx, req, blob_buf.head);
    blob_buf_free(&blob_buf);

    return UBUS_STATUS_OK;
}

enum UbusResult
ubus_init(struct ubus_context **context) {
    struct ubus_context *ctx = ubus_connect(NULL);
    *context = ctx;
    if (ctx == NULL) {
        return UBUS_RESULT_ERROR_CONNECTION_FAILED;
    }
    uloop_init();
    ubus_add_uloop(ctx);
    if (ubus_add_object(ctx, &esp_object) != 0) {
        return UBUS_RESULT_ERROR_INIT_FAILED;
    }

    return UBUS_RESULT_OK;
}

void
ubus_deinit(struct ubus_context *context) {
    ubus_free(context);
    uloop_done();
}
