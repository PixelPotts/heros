#include "app_registry.h"
#include "wm.h"
#include "../kernel/string.h"

static AppManifest apps[MAX_APPS];
static int app_count = 0;

void app_registry_init(void)
{
    memset(apps, 0, sizeof(apps));
    app_count = 0;
}

int app_registry_register(const char *name, IconId icon,
                           AppContent *(*create)(void))
{
    if (app_count >= MAX_APPS) return -1;
    int id = app_count++;
    apps[id].name = name;
    apps[id].icon = icon;
    apps[id].running = 0;
    apps[id].create = create;
    return id;
}

int app_registry_count(void)
{
    return app_count;
}

const AppManifest *app_registry_get(int index)
{
    if (index < 0 || index >= app_count)
        return (void *)0;
    return &apps[index];
}

void app_registry_launch(int index)
{
    if (index < 0 || index >= app_count) return;

    /* If already running, focus existing window */
    int existing = wm_find_by_app(index);
    if (existing >= 0) {
        wm_focus(existing);
        return;
    }

    /* Create new instance */
    if (!apps[index].create) return;
    AppContent *content = apps[index].create();
    if (!content) return;

    /* Position: cascade from top-left */
    int offset = (index * 30) % 150;
    int win_id = wm_open(apps[index].name,
                          80 + offset, 60 + offset,
                          500, 350,
                          WIN_CLOSABLE | WIN_RESIZABLE,
                          index, content);
    if (win_id >= 0) {
        apps[index].running = 1;
    }
}

void app_registry_set_running(int index, int running)
{
    if (index >= 0 && index < app_count)
        apps[index].running = running;
}
