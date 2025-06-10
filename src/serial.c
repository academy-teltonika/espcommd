#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <libubox/blobmsg.h>

#define ESP_VID 0x10c4
#define ESP_PID 0xea60

enum CheckEspPortResult {
    ESP_RESULT_OK_IS_ESP,
    ESP_RESULT_OK_NOT_ESP,
    ESP_RESULT_ERR_FAILURE,
};

static enum CheckEspPortResult
check_port_for_esp(struct sp_port *port) {
    if (sp_get_port_transport(port) != SP_TRANSPORT_USB) {
        return ESP_RESULT_OK_NOT_ESP;
    }

    int vid, pid;
    if (sp_get_port_usb_vid_pid(port, &vid, &pid) != SP_OK) {
        return ESP_RESULT_ERR_FAILURE;
    }
    if (vid != ESP_VID || pid != ESP_PID) {
        return ESP_RESULT_OK_NOT_ESP;
    }

    return ESP_RESULT_OK_IS_ESP;
}

// Result will not a full count of all esp ports on error.
static enum UsbResult
count_esp_ports(int *esp_port_count, struct sp_port **port_list) {
    *esp_port_count = 0;
    for (int i = 0; port_list[i] != NULL; i++) {
        enum CheckEspPortResult result = check_port_for_esp(port_list[i]);
        switch (result) {
            case ESP_RESULT_OK_IS_ESP:
                esp_port_count++;
                break;
            case ESP_RESULT_OK_NOT_ESP:
                continue;
            default:
                return USB_RESULT_ERR_UNKNOWN;
        }
    }

    return USB_RESULT_OK;
}

// Result will not be a full list of filtered ports on error.
static enum UsbResult
filter_esp_ports(struct sp_port **port_list_all, struct sp_port **port_list_esp) {
    int port_list_esp_count = 0;
    for (int i = 0; port_list_all[i] != NULL; i++) {
        struct sp_port *port = port_list_all[i];
        enum CheckEspPortResult result = check_port_for_esp(port);
        switch (result) {
            case ESP_RESULT_OK_IS_ESP:
                if (sp_copy_port(port, port_list_esp + port_list_esp_count) != SP_OK) {
                    return USB_RESULT_ERR_UNKNOWN;
                }
                port_list_esp_count++;
                break;
            case ESP_RESULT_OK_NOT_ESP:
                continue;
            default:
                return USB_RESULT_ERR_UNKNOWN;
        }
    }

    return USB_RESULT_OK;
}

enum UsbResult
enumerate_esp_serial_ports(struct sp_port ***port_list) {
    enum UsbResult ret = USB_RESULT_OK;

    struct sp_port **port_list_all = NULL;
    struct sp_port **port_list_esp = NULL;

    if (sp_list_ports(&port_list_all) != SP_OK) {
        ret = USB_RESULT_ERR_UNKNOWN;
        goto cleanup;
    }

    int esp_port_count = 0;
    ret = count_esp_ports(&esp_port_count, port_list_all);
    if (ret != USB_RESULT_OK) {
        goto cleanup;
    }

    // NULL terminated array of filtered ports
    port_list_esp = (struct sp_port **) calloc(esp_port_count + 1, sizeof(struct sp_port *));
    ret = filter_esp_ports(port_list_all, port_list_esp);

cleanup:
    if (port_list_all != NULL) {
        sp_free_port_list(port_list_all);
    }
    if (ret != USB_RESULT_OK) {
        if (port_list_esp != NULL) {
            sp_free_port_list(port_list_esp);
            port_list_esp = NULL;
        }
    }

    *port_list = port_list_esp;
    return ret;
}

enum UsbResult
get_esp_port_by_name(const char *port_name, struct sp_port **port) {
    enum UsbResult result = USB_RESULT_OK;

    // Technically this might not always be NOT_FOUND
    if (sp_get_port_by_name(port_name, port) != SP_OK) {
        result = USB_RESULT_ERR_PORT_NOT_FOUND;
        goto failure;
    }
    // This might also fail for other reasons
    if (check_port_for_esp(*port) != ESP_RESULT_OK_IS_ESP) {
        result = USB_RESULT_ERR_PORT_INVALID;
        goto failure;
    }

    return result;

failure:
    sp_free_port(*port);
    *port = NULL;
    return result;
}

enum UsbResult
open_port(struct sp_port *port) {
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK) {
        return USB_RESULT_ERR_PORT_OPEN;
    }

    struct sp_port_config *config = NULL;
    sp_new_config(&config);
    sp_set_config_baudrate(config, 9600);
    sp_set_config_bits(config, 8);
    sp_set_config_parity(config, SP_PARITY_NONE);
    sp_set_config_flowcontrol(config, SP_FLOWCONTROL_NONE);
    enum sp_return sp_ret = sp_set_config(port, config);

    sp_free_config(config);
    if (sp_ret != SP_OK) {
        return USB_RESULT_ERR_PORT_WRITE;
    }

    return USB_RESULT_OK;
}

enum UsbResult
write_and_await_response(
    struct sp_port *port,
    const char *input_buf,
    int write_bytes,
    char *response_buf,
    int read_bytes
) {
    int ret = sp_blocking_write(port, input_buf, write_bytes, 1000);
    if (ret != write_bytes) {
        return USB_RESULT_ERR_PORT_WRITE;
    }

    if (sp_blocking_read(port, response_buf, read_bytes, 1500) <= 0) {
        return USB_RESULT_ERR_PORT_READ;
    }

    return USB_RESULT_OK;
}

const char *UsbResult_str[] = {
    "Success.",
    "Failed to open port.",
    "Failed to read from port.",
    "Failed to write to port.",
    "Port does not exist.",
    "Port is not connected to an ESP.",
    "Unknown failure."
};
