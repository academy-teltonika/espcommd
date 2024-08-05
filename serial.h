#pragma once
#include <libserialport.h>

enum UsbResult {
    USB_RESULT_OK,
    USB_RESULT_ERR_PORT_OPEN,
    USB_RESULT_ERR_PORT_READ,
    USB_RESULT_ERR_PORT_WRITE,
    USB_RESULT_ERR_PORT_NOT_FOUND,
    USB_RESULT_ERR_PORT_INVALID,
    USB_RESULT_ERR_UNKNOWN,
};

extern const char *UsbResult_str[];

static enum CheckEspPortResult check_port_for_esp(struct sp_port *port);

enum UsbResult enumerate_esp_serial_ports(struct sp_port ***port_list);

enum UsbResult get_esp_port_by_name(const char *port_name, struct sp_port **port);

enum UsbResult write_and_await_response(struct sp_port *port, const char *input_buf, int write_bytes,
                                        char *response_buf, int read_bytes);

enum UsbResult open_port(struct sp_port *port);

