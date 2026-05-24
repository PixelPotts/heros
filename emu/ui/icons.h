#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "types.h"

/* Icon IDs */
typedef enum {
    ICON_TERMINAL = 0,
    ICON_FILES,
    ICON_SETTINGS,
    ICON_TASK_MGR,
    ICON_CLOSE,
    ICON_MINIMIZE,
    ICON_MAXIMIZE,
    ICON_FOLDER,
    ICON_FILE,
    ICON_HOME,
    ICON_SEARCH,
    ICON_LOCK,
    ICON_POWER,
    ICON_WIFI,
    ICON_BATTERY,
    ICON_CLOCK,
    ICON_COUNT
} IconId;

/* Draw an icon at (x,y) with given size and color */
void icon_draw(IconId id, int x, int y, int size, Color c);

#endif
