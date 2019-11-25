#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXSTR 1000
#define _NET_WM_STATE_REMOVE 0 /* remove/unset property */
#define _NET_WM_STATE_ADD 1    /* add/set property */
#define _NET_WM_STATE_TOGGLE 2 /* toggle property  */
Display *display;
unsigned long window;
unsigned char *prop;

// https://github.com/UltimateHackingKeyboard/current-window-linux/blob/master/get-current-window.c
void check_status(int status, unsigned long window)
{
    if (status == BadWindow)
    {
        printf("window id # 0x%lx does not exists!", window);
        exit(1);
    }

    if (status != Success)
    {
        printf("XGetWindowProperty failed!");
        exit(2);
    }
}

unsigned char *get_string_property(char *property_name)
{
    Atom actual_type, filter_atom;
    int actual_format, status;
    unsigned long nitems, bytes_after;

    filter_atom = XInternAtom(display, property_name, True);
    status = XGetWindowProperty(display, window, filter_atom, 0, MAXSTR, False, AnyPropertyType,
                                &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    check_status(status, window);
    return prop;
}

unsigned long get_long_property(char *property_name)
{
    get_string_property(property_name);
    unsigned long long_property = prop[0] + (prop[1] << 8) + (prop[2] << 16) + (prop[3] << 24);
    return long_property;
}

// https://stackoverflow.com/questions/7365256/xlib-how-to-check-if-a-window-is-minimized-or-not
int contains_atom(char *property_name, char *atom)
{
    Atom actual_type, filter_atom, tocheck;
    int actual_format, status;
    unsigned long nitems, bytes_after;
    Atom *atoms;

    filter_atom = XInternAtom(display, property_name, True);
    status = XGetWindowProperty(display, window, filter_atom, 0, MAXSTR, False, XA_ATOM,
                                &actual_type, &actual_format, &nitems, &bytes_after, (unsigned char **)&atoms);
    check_status(status, window);
    tocheck = XInternAtom(display, atom, True);
    for (int i = 0; i < nitems; i++)
    {
        if (atoms[i] == tocheck)
        {
            XFree(atoms);
            return 1;
        }
    }
    XFree(atoms);
    return 0;
}

// https://stackoverflow.com/questions/4530786/xlib-create-window-in-mimized-or-maximized-state
void set_maximized(int enable)
{
    XEvent xev;
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom max_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom max_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = enable ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    xev.xclient.data.l[1] = max_horz;
    xev.xclient.data.l[2] = max_vert;

    XSendEvent(display, DefaultRootWindow(display), False, SubstructureNotifyMask, &xev);
}

// https://gist.github.com/muktupavels/d03bb14ea6042b779df89b4c87df975d
typedef struct
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints;

static MotifWmHints *get_motif_wm_hints(Display *display, Window window)
{
    Atom property;
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *data;

    property = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    result = XGetWindowProperty(display, window, property,
                                0, 4096, False, AnyPropertyType,
                                &actual_type, &actual_format,
                                &nitems, &bytes_after, &data);

    if (result == Success && data != NULL)
    {
        size_t data_size;
        size_t max_size;
        MotifWmHints *hints;

        data_size = nitems * sizeof(long);
        max_size = sizeof(*hints);

        hints = calloc(1, max_size);

        memcpy(hints, data, data_size > max_size ? max_size : data_size);
        XFree(data);

        return hints;
    }

    return NULL;
}

static void
toggle_window_decorations(Display *display,
                          Window window)
{
    MotifWmHints *hints;
    Atom property;
    int nelements;

    hints = get_motif_wm_hints(display, window);
    if (hints == NULL)
    {
        hints = calloc(1, sizeof(*hints));
        hints->decorations = (1L << 0);
    }

    hints->flags |= (1L << 1);
    hints->decorations = hints->decorations == 0 ? (1L << 0) : 0;

    property = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    nelements = sizeof(*hints) / sizeof(long);

    XChangeProperty(display, window, property, property, 32, PropModeReplace,
                    (unsigned char *)hints, nelements);

    free(hints);
}

int main(int argc, char **argv)
{
    char *display_name = NULL; // could be the value of $DISPLAY

    display = XOpenDisplay(display_name);
    if (display == NULL)
    {
        fprintf(stderr, "%s:  unable to open display '%s'\n", argv[0], XDisplayName(display_name));
    }
    int screen = XDefaultScreen(display);
    window = RootWindow(display, screen);

    window = get_long_property("_NET_ACTIVE_WINDOW");

    printf("_NET_WM_PID: %lu\n", get_long_property("_NET_WM_PID"));
    printf("WM_CLASS: %s\n", get_string_property("WM_CLASS"));
    printf("_NET_WM_NAME: %s\n", get_string_property("_NET_WM_NAME"));
    int maximized = contains_atom("_NET_WM_STATE", "_NET_WM_STATE_MAXIMIZED_HORZ") &&
                    contains_atom("_NET_WM_STATE", "_NET_WM_STATE_MAXIMIZED_VERT");
    if (maximized)
        set_maximized(0);
    toggle_window_decorations(display, window);
    if (maximized)
        set_maximized(1);
    XCloseDisplay(display);
    return 0;
}
