#ifndef ZOOMER_TAP_H
#define ZOOMER_TAP_H

#include <stdbool.h>

// Global event tap: Cmd+Alt+Shift + scroll zooms, mouse moves pan, Esc
// closes. Returns false if the tap could not be created (no Accessibility
// permission). `reverse` flips the scroll direction.
bool tap_init(bool reverse);

#endif
