#include <libubus.h>
#include <sys/syslog.h>
#include <syslog.h>
#include "ubus.h"

#define SYSLOG_OPTIONS LOG_PID | LOG_NDELAY

static struct ubus_context *g_ubus_context;

int main(void) {
    openlog(NULL, SYSLOG_OPTIONS, LOG_LOCAL0);

    switch (ubus_init(&g_ubus_context)) {
        case UBUS_RESULT_ERROR_CONNECTION_FAILED:
            syslog(LOG_ERR, "Failed to connect to ubus.");
            break; 
        case UBUS_RESULT_ERROR_INIT_FAILED:
            syslog(LOG_ERR, "Ubus initialization failed.");
            break;
        case UBUS_RESULT_OK:
            syslog(LOG_INFO, "Esp communication daemon started successfully.");
            break;
    }
    uloop_run();
    ubus_deinit(g_ubus_context);

    return 0;
}
