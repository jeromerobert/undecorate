#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display *display;
static unsigned long window;
static unsigned char *prop;

// https://github.com/UltimateHackingKeyboard/current-window-linux/blob/master/get-current-window.c
static void check_status(int status, unsigned long window)
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

static unsigned char *get_string_property(char *property_name)
{
    Atom actual_type, filter_atom;
    int actual_format, status;
    unsigned long nitems, bytes_after;

    filter_atom = XInternAtom(display, property_name, True);
    status = XGetWindowProperty(display, window, filter_atom, 0, 4096, False, AnyPropertyType,
                                &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    check_status(status, window);
    return prop;
}

static unsigned long get_long_property(char *property_name)
{
    get_string_property(property_name);
    unsigned long long_property = prop[0] + (prop[1] << 8) + (prop[2] << 16) + (prop[3] << 24);
    return long_property;
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

static MotifWmHints *get_motif_wm_hints()
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

static void toggle_window_decorations()
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
    window = 0;
    if (argc > 1) {
        sscanf (argv[1], "0x%lx", &window);
        if (window == 0)
            sscanf (argv[1], "%lu", &window);
    }
    if(window == 0) {
        int screen = XDefaultScreen(display);
        window = RootWindow(display, screen);
        window = get_long_property("_NET_ACTIVE_WINDOW");
    }
    printf("Toggling decoration on windows %x\n", window);
    toggle_window_decorations(display, window);
    XCloseDisplay(display);
    return 0;
}
