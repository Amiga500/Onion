#ifndef INFOPANEL_APPSTATE_H__
#define INFOPANEL_APPSTATE_H__

#include "utils/signal_handler.h"

static bool quit = false;
static bool all_changed = true;
static bool header_changed = true;
static bool footer_changed = true;
static bool battery_changed = true;

static void sigHandler(int sig)
{
    signal_handler_quit(&quit, sig);
}

#endif // INFOPANEL_APPSTATE_H__
