#include <libubus.h>
#include <stdio.h>
#include "ubus.h"

static struct ubus_context *g_ubus_context;

void configure_signal_handlers(void);

int main(void) {
    ubus_init(&g_ubus_context);
    uloop_run();
    ubus_deinit(g_ubus_context);

    puts("Done.");
    return 0;
}

void termination_handler(int signum) {
    uloop_end();
}

void configure_signal_handlers(void) {
    struct sigaction action;
    action.sa_handler = termination_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

