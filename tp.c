#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xatom.h>
#include <sys/time.h>
#include <time.h>

#define POLL_DELAY 200000 // 200 ms

#define PROP_DEVICE_ENABLED "Device Enabled"

#define TOUCHPAD_STATE_UNDEFINED -1
#define TOUCHPAD_STATE_ENABLE 1
#define TOUCHPAD_STATE_DISABLE 0

static int trackpointCoordX = -1;
static int trackpointCoordY = -1;

static const unsigned long long disableTimeMS = 10000;

static XDevice *GetTouchpadDevice(Display *display);
static XDevice *GetTrackpointDevice(Display *display);
static Bool IsTrackpointMoved(Display *display, XDevice *trackpoint);
static void ChangeTouchpadState(Display *display, XDevice *touchpad, char state);
static unsigned long long GetTimeMS();

int main(int argc, char *argv[])
{
    Display *display = XOpenDisplay(NULL);

    if (!display)
    {
        fprintf(stderr, "Can't open display.\n");
        exit(2);
    }

    XDevice *touchpad = GetTouchpadDevice(display);
    if (!touchpad)
    {
        XCloseDisplay(display);
        fprintf(stderr, "No touchpad device found.\n");
        exit(2);
    }

    XDevice *trackpoint = GetTrackpointDevice(display);
    if (!trackpoint)
    {
      fprintf(stderr, "No trackpoint device found.\n");
      XCloseDevice(display, touchpad);
      XCloseDisplay(display);
      exit(2);
    }

    printf("id: %d\n", touchpad ? touchpad->device_id : -1);
    printf("id: %d\n", trackpoint ? trackpoint->device_id : -1);

    char touchPadCurrentState = TOUCHPAD_STATE_UNDEFINED;
    unsigned long long trackpointLastMovedTime = 0;

    while (1)
    {
        if (IsTrackpointMoved(display, trackpoint))
        {
            trackpointLastMovedTime = GetTimeMS();
            if (touchPadCurrentState != TOUCHPAD_STATE_DISABLE)
            {
                touchPadCurrentState = TOUCHPAD_STATE_DISABLE;
                ChangeTouchpadState(display, touchpad, touchPadCurrentState);
            }
        }
        else
        {
            if (touchPadCurrentState != TOUCHPAD_STATE_ENABLE)
            {
                if ((trackpointLastMovedTime + disableTimeMS) <= GetTimeMS())
                {
                    touchPadCurrentState = TOUCHPAD_STATE_ENABLE;
                    ChangeTouchpadState(display, touchpad, touchPadCurrentState);
                }
            }
        }
        usleep(POLL_DELAY);
    }

    return 0;
}

static XDevice *GetTouchpadDevice(Display *display)
{
    XDevice *touchpadDevice = NULL;

    int inputDevicesCount = 0;
    // Lists available input devices.
    XDeviceInfo *inputDevices = XListInputDevices(display, &inputDevicesCount);

    printf("inputDevicesCount: %d\n", inputDevicesCount);

    Atom touchpadType = XInternAtom(display, XI_TOUCHPAD, True);
    Atom touchpadPropDeviceEnabled = XInternAtom(display, PROP_DEVICE_ENABLED, True);

    int i;
    for (i = 0; i < inputDevicesCount; ++i)
    {
        if (inputDevices[i].type != touchpadType)
          continue;

        printf("[%d]: id: %d name: %s type: %d\n", i, inputDevices[i].id, inputDevices[i].name, inputDevices[i].type);

        XDevice *dev = XOpenDevice(display, inputDevices[i].id);
        if (!dev)
        {
            printf("\tCan't open the following device: id: %d name: %s\n", inputDevices[i].id, inputDevices[i].name);
            continue;
        }

        int propertiesCount = 0;
        Atom *properties = XListDeviceProperties(display, dev, &propertiesCount);
        if (!properties || !propertiesCount)
        {
            printf("\tCan't get the properties from the following device: id: %d name: %s\n", inputDevices[i].id, inputDevices[i].name);
            XCloseDevice(display, dev);
            continue;
        }

        Bool devGood = False;
        int j;
        for (j = 0; j < propertiesCount; ++j)
        {
            // Found a proper device.
            if (properties[j] == touchpadPropDeviceEnabled)
            {
                devGood = True;
                break;
            }
        }
        XFree(properties);

        // Proper device not found.
        if (!devGood)
            XCloseDevice(display, dev);
        else
        {
            touchpadDevice = dev;
            break;
        }
    }

    XFreeDeviceList(inputDevices);

    return touchpadDevice;
}

static XDevice *GetTrackpointDevice(Display *display)
{
    XDevice *trackpointDevice = NULL;

    int inputDevicesCount = 0;
    // Lists available input devices.
    XDeviceInfo *inputDevices = XListInputDevices(display, &inputDevicesCount);

    printf("inputDevicesCount: %d\n", inputDevicesCount);

    // Mouse type.
    Atom trackpointType = XInternAtom(display, XI_MOUSE, True);

    int i;
    for (i = 0; i < inputDevicesCount; ++i)
    {
        // If it's not a mouse then it's definitely not a trackpoint.
        if (inputDevices[i].type != trackpointType)
          continue;

        printf("[%d]: id: %d name: %s type: %d\n", i, inputDevices[i].id, inputDevices[i].name, inputDevices[i].type);

        XDevice *dev = XOpenDevice(display, inputDevices[i].id);
        if (!dev)
        {
            printf("\tCan't open the following device: id: %d name: %s\n", inputDevices[i].id, inputDevices[i].name);
        }
        else if (!strstr(inputDevices[i].name, "TrackPoint") != NULL)
        {
          // Proper device not found.
          XCloseDevice(display, dev);
        }
        else
        {
          // Trackpoint found. Hurray!
          trackpointDevice = dev;
          break;
        }
    }

    XFreeDeviceList(inputDevices);

    return trackpointDevice;
}

static Bool IsTrackpointMoved(Display *display, XDevice *trackpoint)
{
    int i, j, oldX, oldY;

    oldX = trackpointCoordX;
    oldY = trackpointCoordY;

    XDeviceState *xds = XQueryDeviceState(display, trackpoint);
    XInputClass *dataptr = xds->data;
    for (i = 0; i < xds->num_classes; ++i)
    {
        if (dataptr->class == ValuatorClass)
        {
            XValuatorState *xvs = (XValuatorState*)dataptr;
            if (xvs->num_valuators != 2)
            {
                fprintf(stderr, "Invalid ValuatorClass.\n");
                exit(2);
            }
            trackpointCoordX = xvs->valuators[0];
            trackpointCoordY = xvs->valuators[1];
            break;
        }
        dataptr = (XInputClass *)((char *)dataptr + dataptr->length);
    }

    // Not moved yet.
    if (oldX == -1 || oldY == -1)
        return False;

    return oldX != trackpointCoordX || oldY != trackpointCoordY;
}

static void ChangeTouchpadState(Display *display, XDevice *touchpad, char state)
{
    printf("ChangeTouchpadState %d\n", state);
    // TODO: cache
    Atom touchpadPropDeviceEnabled = XInternAtom(display, PROP_DEVICE_ENABLED, True);

    XChangeDeviceProperty(display, touchpad, touchpadPropDeviceEnabled, XA_INTEGER, 8, PropModeReplace, &state, 1);
    XFlush(display);
}

static unsigned long long GetTimeMS()
{
    struct timespec spec;
    // Faster but less precise.
    clock_gettime(CLOCK_REALTIME_COARSE, &spec);
    // We might lose 1 millisecond accuracy but it doesn't matter.
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}
