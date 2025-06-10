#include <libubus.h>
#include "ubus.h"

static struct ubus_context *g_ubus_context;

int main(void) {
    ubus_init(&g_ubus_context);
    uloop_run();
    ubus_deinit(g_ubus_context);

    return 0;
}
