#pragma once
#include <libubus.h>
#include <libserialport.h>

// TODO: Will use this in the next part or just remove.
enum UbusResult {
    UBUS_RESULT_OK,
    UBUS_RESULT_ERROR_CONNECTION_FAILED,
    UBUS_RESULT_ERROR_DEVICES_FAILED_LOAD_PORTS,
    UBUS_RESULT_ERROR_INIT_FAILED,
};

enum UbusResult ubus_init(struct ubus_context **context);

void ubus_deinit(struct ubus_context *context);

enum UbusResult ubus_load_device_ports(struct sp_port **port_list);
