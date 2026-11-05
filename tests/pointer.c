// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only virtual pointer. NEVER use this on the user's live Wayland display.
#include <wayland-client.h>
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
static struct zwlr_virtual_pointer_manager_v1 *manager;
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *iface, uint32_t version) {
    if (!strcmp(iface, zwlr_virtual_pointer_manager_v1_interface.name))
        manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {}
static const struct wl_registry_listener listener = {global, removed};
static uint32_t stamp(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000+t.tv_nsec/1000000;
}
static void position(struct wl_display *d, struct zwlr_virtual_pointer_v1 *p, int x, int y, int w, int h) {
    zwlr_virtual_pointer_v1_motion_absolute(p, stamp(), x, y, w, h);
    zwlr_virtual_pointer_v1_frame(p); wl_display_roundtrip(d); usleep(40000);
}
int main(int argc, char **argv) {
    // Fail closed: a testbed-launch marker AND the deliberately private display are required.
    if (!getenv("TESTBED_DIR") || !getenv("WAYLAND_DISPLAY") || strcmp(getenv("WAYLAND_DISPLAY"), "testbed-wayland")) {
        fputs("Run only through testbed run\n", stderr); return 2;
    }
    if (argc != 7 && argc != 9) { fputs("usage: pointer width height x y click|hold|drag milliseconds [end-x end-y]\n", stderr); return 2; }
    int w=atoi(argv[1]), h=atoi(argv[2]), x=atoi(argv[3]), y=atoi(argv[4]), ms=atoi(argv[6]);
    struct wl_display *d = wl_display_connect(NULL);
    if (!d) return 3;
    struct wl_registry *r = wl_display_get_registry(d);
    wl_registry_add_listener(r, &listener, NULL); wl_display_roundtrip(d);
    if (!manager) return 4;
    struct zwlr_virtual_pointer_v1 *p = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, NULL);
    position(d,p,x,y,w,h);
    zwlr_virtual_pointer_v1_button(p,stamp(),BTN_LEFT,WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(p); wl_display_roundtrip(d);
    usleep(80000);
    if (!strcmp(argv[5],"drag") && argc == 9) {
        int ex=atoi(argv[7]), ey=atoi(argv[8]);
        for (int i=1;i<=12;++i) {
            position(d,p,x+(ex-x)*i/12,y+(ey-y)*i/12,w,h);
            usleep(ms*1000/12);
        }
    } else usleep(ms*1000);
    zwlr_virtual_pointer_v1_button(p,stamp(),BTN_LEFT,WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(p); wl_display_roundtrip(d);
    zwlr_virtual_pointer_v1_destroy(p); zwlr_virtual_pointer_manager_v1_destroy(manager);
    wl_registry_destroy(r); wl_display_disconnect(d);
    return 0;
}
