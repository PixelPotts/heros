#ifndef UI_APPS_TERMINAL_H
#define UI_APPS_TERMINAL_H

#include "../window.h"

AppContent *terminal_create(void);
AppContent *terminal_create_with_nano(const char *path);

#endif
