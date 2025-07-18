#pragma once
#include <libubus.h>
#include <libserialport.h>

enum UbusResult {
    UBUS_RESULT_OK,
    UBUS_RESULT_ERROR_CONNECTION_FAILED,
    UBUS_RESULT_ERROR_INIT_FAILED,
};

enum UbusResult
ubus_init(struct ubus_context **context);

void
ubus_deinit(struct ubus_context *context);

enum UbusResult
ubus_load_device_ports(struct sp_port **port_list);
