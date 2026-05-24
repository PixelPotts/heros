#ifndef UI_APP_REGISTRY_H
#define UI_APP_REGISTRY_H

#include "window.h"
#include "icons.h"

#define MAX_APPS  8

typedef struct {
    const char *name;
    IconId      icon;
    int         running;      /* has an open window */
    AppContent *(*create)(void);   /* factory function */
} AppManifest;

void    app_registry_init(void);
int     app_registry_register(const char *name, IconId icon,
                                AppContent *(*create)(void));
int     app_registry_count(void);
const AppManifest *app_registry_get(int index);
void    app_registry_launch(int index);
void    app_registry_set_running(int index, int running);

#endif
