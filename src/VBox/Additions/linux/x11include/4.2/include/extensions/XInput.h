/* $Xorg: XInput.h,v 1.4 2001/02/09 02:03:23 xorgcvs Exp $ */

/************************************************************

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989 by Hewlett-Packard Company, Palo Alto, California.

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Hewlett-Packard not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

HEWLETT-PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HEWLETT-PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/
/* $XFree86: xc/include/extensions/XInput.h,v 1.3 2001/12/14 19:53:28 dawes Exp $ */

/* Definitions used by the library and client */

#ifndef _XINPUT_H_
#define _XINPUT_H_

#include <X11/Xlib.h>
#include <X11/extensions/XI.h>

#define _deviceKeyPress		0
#define _deviceKeyRelease	1

#define _deviceButtonPress	0
#define _deviceButtonRelease	1

#define _deviceMotionNotify	0

#define _deviceFocusIn		0
#define _deviceFocusOut		1

#define _proximityIn		0
#define _proximityOut		1

#define _deviceStateNotify	0
#define _deviceMappingNotify	1
#define _changeDeviceNotify	2

#define FindTypeAndClass(d,type,_class,classid,offset) \
    { int _i; XInputClassInfo *_ip; \
    type = 0; _class = 0; \
    for (_i=0, _ip= ((XDevice *) d)->classes; \
	 _i< ((XDevice *) d)->num_classes; \
	 _i++, _ip++) \
	if (_ip->input_class == classid) \
	    {type =  _ip->event_type_base + offset; \
	     _class =  ((XDevice *) d)->device_id << 8 | type;}}

#define DeviceKeyPress(d,type,_class) \
    FindTypeAndClass(d, type, _class, KeyClass, _deviceKeyPress)

#define DeviceKeyRelease(d,type,_class) \
    FindTypeAndClass(d, type, _class, KeyClass, _deviceKeyRelease)

#define DeviceButtonPress(d,type,_class) \
    FindTypeAndClass(d, type, _class, ButtonClass, _deviceButtonPress)

#define DeviceButtonRelease(d,type,_class) \
    FindTypeAndClass(d, type, _class, ButtonClass, _deviceButtonRelease)

#define DeviceMotionNotify(d,type,_class) \
    FindTypeAndClass(d, type, _class, ValuatorClass, _deviceMotionNotify)

#define DeviceFocusIn(d,type,_class) \
    FindTypeAndClass(d, type, _class, FocusClass, _deviceFocusIn)

#define DeviceFocusOut(d,type,_class) \
    FindTypeAndClass(d, type, _class, FocusClass, _deviceFocusOut)

#define ProximityIn(d,type,_class) \
    FindTypeAndClass(d, type, _class, ProximityClass, _proximityIn)

#define ProximityOut(d,type,_class) \
    FindTypeAndClass(d, type, _class, ProximityClass, _proximityOut)

#define DeviceStateNotify(d,type,_class) \
    FindTypeAndClass(d, type, _class, OtherClass, _deviceStateNotify)

#define DeviceMappingNotify(d,type,_class) \
    FindTypeAndClass(d, type, _class, OtherClass, _deviceMappingNotify)

#define ChangeDeviceNotify(d,type,_class) \
    FindTypeAndClass(d, type, _class, OtherClass, _changeDeviceNotify)

#define DevicePointerMotionHint(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _devicePointerMotionHint;}

#define DeviceButton1Motion(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButton1Motion;}

#define DeviceButton2Motion(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButton2Motion;}

#define DeviceButton3Motion(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButton3Motion;}

#define DeviceButton4Motion(d,type, _class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButton4Motion;}

#define DeviceButton5Motion(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButton5Motion;}

#define DeviceButtonMotion(d,type, _class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButtonMotion;}

#define DeviceOwnerGrabButton(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceOwnerGrabButton;}

#define DeviceButtonPressGrab(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _deviceButtonGrab;}

#define NoExtensionEvent(d,type,_class) \
    { _class =  ((XDevice *) d)->device_id << 8 | _noExtensionEvent;}

#define BadDevice(dpy,error) _xibaddevice(dpy, &error)

#define BadClass(dpy,error) _xibadclass(dpy, &error)

#define BadEvent(dpy,error) _xibadevent(dpy, &error)

#define BadMode(dpy,error) _xibadmode(dpy, &error)

#define DeviceBusy(dpy,error) _xidevicebusy(dpy, &error)

/***************************************************************
 *
 * DeviceKey events.  These events are sent by input devices that
 * support input class Keys.
 * The location of the X pointer is reported in the coordinate
 * fields of the x,y and x_root,y_root fields.
 *
 */

typedef struct 
    {
    int            type;         /* of event */
    unsigned long  serial;       /* # of last request processed */
    Bool           send_event;   /* true if from SendEvent request */
    Display        *display;     /* Display the event was read from */
    Window         window;       /* "event" window reported relative to */
    XID            deviceid;
    Window         root;         /* root window event occured on */
    Window         subwindow;    /* child window */
    Time           time;         /* milliseconds */
    int            x, y;         /* x, y coordinates in event window */
    int            x_root;       /* coordinates relative to root */
    int            y_root;       /* coordinates relative to root */
    unsigned int   state;        /* key or button mask */
    unsigned int   keycode;      /* detail */
    Bool           same_screen;  /* same screen flag */
    unsigned int   device_state; /* device key or button mask */
    unsigned char  axes_count;
    unsigned char  first_axis;
    int            axis_data[6];
    } XDeviceKeyEvent;

typedef XDeviceKeyEvent XDeviceKeyPressedEvent;
typedef XDeviceKeyEvent XDeviceKeyReleasedEvent;

/*******************************************************************
 *
 * DeviceButton events.  These events are sent by extension devices
 * that support input class Buttons.
 *
 */

typedef struct {
    int           type;         /* of event */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* "event" window reported relative to */
    XID           deviceid;
    Window        root;         /* root window that the event occured on */
    Window        subwindow;    /* child window */
    Time          time;         /* milliseconds */
    int           x, y;         /* x, y coordinates in event window */
    int           x_root;       /* coordinates relative to root */
    int           y_root;       /* coordinates relative to root */
    unsigned int  state;        /* key or button mask */
    unsigned int  button;       /* detail */
    Bool          same_screen;  /* same screen flag */
    unsigned int  device_state; /* device key or button mask */
    unsigned char axes_count;
    unsigned char first_axis;
    int           axis_data[6];
    } XDeviceButtonEvent;

typedef XDeviceButtonEvent XDeviceButtonPressedEvent;
typedef XDeviceButtonEvent XDeviceButtonReleasedEvent;

/*******************************************************************
 *
 * DeviceMotionNotify event.  These events are sent by extension devices
 * that support input class Valuators.
 *
 */

typedef struct 
    {
    int           type;        /* of event */
    unsigned long serial;      /* # of last request processed by server */
    Bool          send_event;  /* true if from a SendEvent request */
    Display       *display;    /* Display the event was read from */
    Window        window;      /* "event" window reported relative to */
    XID           deviceid;
    Window        root;        /* root window that the event occured on */
    Window        subwindow;   /* child window */
    Time          time;        /* milliseconds */
    int           x, y;        /* x, y coordinates in event window */
    int           x_root;      /* coordinates relative to root */
    int           y_root;      /* coordinates relative to root */
    unsigned int  state;       /* key or button mask */
    char          is_hint;     /* detail */
    Bool          same_screen; /* same screen flag */
    unsigned int  device_state; /* device key or button mask */
    unsigned char axes_count;
    unsigned char first_axis;
    int           axis_data[6];
    } XDeviceMotionEvent;

/*******************************************************************
 *
 * DeviceFocusChange events.  These events are sent when the focus
 * of an extension device that can be focused is changed.
 *
 */

typedef struct 
    {
    int           type;       /* of event */
    unsigned long serial;     /* # of last request processed by server */
    Bool          send_event; /* true if from a SendEvent request */
    Display       *display;   /* Display the event was read from */
    Window        window;     /* "event" window reported relative to */
    XID           deviceid;
    int           mode;       /* NotifyNormal, NotifyGrab, NotifyUngrab */
    int           detail;
	/*
	 * NotifyAncestor, NotifyVirtual, NotifyInferior, 
	 * NotifyNonLinear,NotifyNonLinearVirtual, NotifyPointer,
	 * NotifyPointerRoot, NotifyDetailNone 
	 */
    Time                time;
    } XDeviceFocusChangeEvent;

typedef XDeviceFocusChangeEvent XDeviceFocusInEvent;
typedef XDeviceFocusChangeEvent XDeviceFocusOutEvent;

/*******************************************************************
 *
 * ProximityNotify events.  These events are sent by those absolute
 * positioning devices that are capable of generating proximity information.
 *
 */

typedef struct 
    {
    int             type;      /* ProximityIn or ProximityOut */        
    unsigned long   serial;    /* # of last request processed by server */
    Bool            send_event; /* true if this came from a SendEvent request */
    Display         *display;  /* Display the event was read from */
    Window          window;      
    XID	            deviceid;
    Window          root;            
    Window          subwindow;      
    Time            time;            
    int             x, y;            
    int             x_root, y_root;  
    unsigned int    state;           
    Bool            same_screen;     
    unsigned int    device_state; /* device key or button mask */
    unsigned char   axes_count;
    unsigned char   first_axis;
    int             axis_data[6];
    } XProximityNotifyEvent;
typedef XProximityNotifyEvent XProximityInEvent;
typedef XProximityNotifyEvent XProximityOutEvent;

/*******************************************************************
 *
 * DeviceStateNotify events are generated on EnterWindow and FocusIn 
 * for those clients who have selected DeviceState.
 *
 */

typedef struct
    {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    } XInputClass;

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;
    XID           deviceid;
    Time          time;
    int           num_classes;
    char	  data[64];
} XDeviceStateNotifyEvent;	

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    unsigned char	num_valuators;
    unsigned char	mode;
    int        		valuators[6];
} XValuatorStatus;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    short		num_keys;
    char        	keys[32];
} XKeyStatus;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    short		num_buttons;
    char        	buttons[32];
} XButtonStatus;

/*******************************************************************
 *
 * DeviceMappingNotify event.  This event is sent when the key mapping,
 * modifier mapping, or button mapping of an extension device is changed.
 *
 */

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* unused */
    XID           deviceid;
    Time          time;
    int           request;      /* one of MappingModifier, MappingKeyboard,
                                    MappingPointer */
    int           first_keycode;/* first keycode */
    int           count;        /* defines range of change w. first_keycode*/
} XDeviceMappingEvent;

/*******************************************************************
 *
 * ChangeDeviceNotify event.  This event is sent when an 
 * XChangeKeyboard or XChangePointer request is made.
 *
 */

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* unused */
    XID           deviceid;
    Time          time;
    int           request;      /* NewPointer or NewKeyboard */
} XChangeDeviceNotifyEvent;

/*******************************************************************
 *
 * Control structures for input devices that support input class
 * Feedback.  These are used by the XGetFeedbackControl and 
 * XChangeFeedbackControl functions.
 *
 */

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
     XID            c_class;
#else
     XID            class;
#endif
     int            length;
     XID            id;
} XFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     click;
    int     percent;
    int     pitch;
    int     duration;
    int     led_mask;
    int     global_auto_repeat;
    char    auto_repeats[32];
} XKbdFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     accelNum;
    int     accelDenom;
    int     threshold;
} XPtrFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     resolution;
    int     minVal;
    int     maxVal;
} XIntegerFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     max_symbols;
    int     num_syms_supported;
    KeySym  *syms_supported;
} XStringFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     percent;
    int     pitch;
    int     duration;
} XBellFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     led_values;
    int     led_mask;
} XLedFeedbackState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
     XID            c_class;
#else
     XID            class;
#endif
     int            length;
     XID	    id;
} XFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     accelNum;
    int     accelDenom;
    int     threshold;
} XPtrFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     click;
    int     percent;
    int     pitch;
    int     duration;
    int     led_mask;
    int     led_value;
    int     key;
    int     auto_repeat_mode;
} XKbdFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     num_keysyms;
    KeySym  *syms_to_display;
} XStringFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     int_to_display;
} XIntegerFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     percent;
    int     pitch;
    int     duration;
} XBellFeedbackControl;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    XID     c_class;
#else
    XID     class;
#endif
    int     length;
    XID     id;
    int     led_mask;
    int     led_values;
} XLedFeedbackControl;

/*******************************************************************
 *
 * Device control structures.
 *
 */

typedef struct {
     XID            control;
     int            length;
} XDeviceControl;

typedef struct {
     XID            control;
     int            length;
     int            first_valuator;
     int            num_valuators;
     int            *resolutions;
} XDeviceResolutionControl;

typedef struct {
     XID            control;
     int            length;
     int            num_valuators;
     int            *resolutions;
     int            *min_resolutions;
     int            *max_resolutions;
} XDeviceResolutionState;

/*******************************************************************
 *
 * An array of XDeviceList structures is returned by the 
 * XListInputDevices function.  Each entry contains information
 * about one input device.  Among that information is an array of 
 * pointers to structures that describe the characteristics of 
 * the input device.
 *
 */

typedef struct _XAnyClassinfo *XAnyClassPtr;

typedef struct _XAnyClassinfo {
#if defined(__cplusplus) || defined(c_plusplus)
    XID 	c_class;
#else
    XID 	class;
#endif
    int 	length;
    } XAnyClassInfo;

typedef struct _XDeviceInfo *XDeviceInfoPtr;

typedef struct _XDeviceInfo
    {
    XID                 id;        
    Atom                type;
    char                *name;
    int                 num_classes;
    int                 use;
    XAnyClassPtr 	inputclassinfo;
    } XDeviceInfo;

typedef struct _XKeyInfo *XKeyInfoPtr;

typedef struct _XKeyInfo
    {
#if defined(__cplusplus) || defined(c_plusplus)
    XID			c_class;
#else
    XID			class;
#endif
    int			length;
    unsigned short      min_keycode;
    unsigned short      max_keycode;
    unsigned short      num_keys;
    } XKeyInfo;

typedef struct _XButtonInfo *XButtonInfoPtr;

typedef struct _XButtonInfo {
#if defined(__cplusplus) || defined(c_plusplus)
    XID		c_class;
#else
    XID		class;
#endif
    int		length;
    short 	num_buttons;
    } XButtonInfo;

typedef struct _XAxisInfo *XAxisInfoPtr;

typedef struct _XAxisInfo {
    int 	resolution;
    int 	min_value;
    int 	max_value;
    } XAxisInfo;

typedef struct _XValuatorInfo *XValuatorInfoPtr;

typedef struct	_XValuatorInfo
    {
#if defined(__cplusplus) || defined(c_plusplus)
    XID			c_class;
#else
    XID			class;
#endif
    int			length;
    unsigned char       num_axes;
    unsigned char       mode;
    unsigned long       motion_buffer;
    XAxisInfoPtr        axes;
    } XValuatorInfo;


/*******************************************************************
 *
 * An XDevice structure is returned by the XOpenDevice function.  
 * It contains an array of pointers to XInputClassInfo structures.
 * Each contains information about a class of input supported by the
 * device, including a pointer to an array of data for each type of event
 * the device reports.
 *
 */


typedef struct {
        unsigned char   input_class;
        unsigned char   event_type_base;
} XInputClassInfo;

typedef struct {
        XID                    device_id;
        int                    num_classes;
        XInputClassInfo        *classes;
} XDevice;


/*******************************************************************
 *
 * The following structure is used to return information for the 
 * XGetSelectedExtensionEvents function.
 *
 */

typedef struct {
        XEventClass     event_type;
        XID             device;
} XEventList;

/*******************************************************************
 *
 * The following structure is used to return motion history data from 
 * an input device that supports the input class Valuators.
 * This information is returned by the XGetDeviceMotionEvents function.
 *
 */

typedef struct {
        Time   time;
        int    *data;
} XDeviceTimeCoord;


/*******************************************************************
 *
 * Device state structure.
 * This is returned by the XQueryDeviceState request.
 *
 */

typedef struct {
        XID		device_id;
        int		num_classes;
        XInputClass	*data;
} XDeviceState;

/*******************************************************************
 *
 * Note that the mode field is a bitfield that reports the Proximity
 * status of the device as well as the mode.  The mode field should
 * be OR'd with the mask DeviceMode and compared with the values
 * Absolute and Relative to determine the mode, and should be OR'd
 * with the mask ProximityState and compared with the values InProximity
 * and OutOfProximity to determine the proximity state.
 *
 */

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    unsigned char	num_valuators;
    unsigned char	mode;
    int        		*valuators;
} XValuatorState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    short		num_keys;
    char        	keys[32];
} XKeyState;

typedef struct {
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned char	c_class;
#else
    unsigned char	class;
#endif
    unsigned char	length;
    short		num_buttons;
    char        	buttons[32];
} XButtonState;

/*******************************************************************
 *
 * Function definitions.
 *
 */

_XFUNCPROTOBEGIN

extern int	XChangeKeyboardDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XChangePointerDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* xaxis */,
    int			/* yaxis */
#endif
);

extern int	XGrabDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* grab_window */,
    Bool		/* ownerEvents */,
    int			/* event count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */,
    Time		/* time */
#endif
);

extern int	XUngrabDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Time 		/* time */
#endif
);

extern int	XGrabDeviceKey(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* key */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_device */,
    Window		/* grab_window */,
    Bool		/* owner_events */,
    unsigned int	/* event_count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */
#endif
);

extern int	XUngrabDeviceKey(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* key */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_dev */,
    Window		/* grab_window */
#endif
);

extern int	XGrabDeviceButton(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* button */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_device */,
    Window		/* grab_window */,
    Bool		/* owner_events */,
    unsigned int	/* event_count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */
#endif
);

extern int	XUngrabDeviceButton(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* button */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_dev */,
    Window		/* grab_window */
#endif
);

extern int	XAllowDeviceEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* event_mode */,
    Time		/* time */
#endif
);

extern int	XGetDeviceFocus(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window*		/* focus */,
    int*		/* revert_to */,
    Time*		/* time */
#endif
);

extern int	XSetDeviceFocus(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* focus */,
    int			/* revert_to */,
    Time		/* time */
#endif
);

extern XFeedbackState	*XGetFeedbackControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int*		/* num_feedbacks */
#endif
);

extern void	XFreeFeedbackList(
#if NeedFunctionPrototypes
    XFeedbackState*	/* list */
#endif
);

extern int	XChangeFeedbackControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned long	/* mask */,
    XFeedbackControl*	/* f */
#endif
);

extern int	XDeviceBell(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    XID			/* feedbackclass */,
    XID			/* feedbackid */,
    int			/* percent */
#endif
);

extern KeySym	*XGetDeviceKeyMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
#if NeedWidePrototypes
    unsigned int	/* first */,
#else
    KeyCode		/* first */,
#endif
    int			/* keycount */,
    int*		/* syms_per_code */
#endif
);

extern int	XChangeDeviceKeyMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* first */,
    int			/* syms_per_code */,
    KeySym*		/* keysyms */,
    int			/* count */
#endif
);

extern XModifierKeymap	*XGetDeviceModifierMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XSetDeviceModifierMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    XModifierKeymap*	/* modmap */
#endif
);

extern int	XSetDeviceButtonMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned char*	/* map[] */,
    int			/* nmap */
#endif
);

extern int	XGetDeviceButtonMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned char*	/* map[] */,
    unsigned int	/* nmap */
#endif
);

extern XDeviceState	*XQueryDeviceState(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern void	XFreeDeviceState(
#if NeedFunctionPrototypes
    XDeviceState*	/* list */
#endif
);

extern XExtensionVersion	*XGetExtensionVersion(
#if NeedFunctionPrototypes
    Display*		/* display */,
    _Xconst char*	/* name */
#endif
);

extern XDeviceInfo	*XListInputDevices(
#if NeedFunctionPrototypes
    Display*		/* display */,
    int*		/* ndevices */
#endif
);

extern void	XFreeDeviceList(
#if NeedFunctionPrototypes
    XDeviceInfo*	/* list */
#endif
);

extern XDevice	*XOpenDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XID			/* id */
#endif
);

extern int	XCloseDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XSetDeviceMode(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* mode */
#endif
);

extern int	XSetDeviceValuators(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int*		/* valuators */,
    int			/* first_valuator */,
    int			/* num_valuators */
#endif
);

extern XDeviceControl	*XGetDeviceControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* control */
#endif
);

extern int	XChangeDeviceControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* control */,
    XDeviceControl*	/* d */
#endif
);

extern int	XSelectExtensionEvent(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* w */,
    XEventClass*	/* event_list */,
    int			/* count */
#endif
);

extern int XGetSelectedExtensionEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* w */,
    int*		/* this_client_count */,
    XEventClass**	/* this_client_list */,
    int*		/* all_clients_count */,
    XEventClass**	/* all_clients_list */
#endif
);

extern int	XChangeDeviceDontPropagateList(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* window */,
    int			/* count */,
    XEventClass*	/* events */,
    int			/* mode */
#endif
);

extern XEventClass	*XGetDeviceDontPropagateList(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* window */,
    int*		/* count */
#endif
);

extern Status	XSendExtensionEvent(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* dest */,
    Bool		/* prop */,
    int			/* count */,
    XEventClass*	/* list */,
    XEvent*		/* event */
#endif
);

extern XDeviceTimeCoord	*XGetDeviceMotionEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Time		/* start */,
    Time		/* stop */,
    int*		/* nEvents */,
    int*		/* mode */,
    int*		/* axis_count */
#endif
);

extern void	XFreeDeviceMotionEvents(
#if NeedFunctionPrototypes
    XDeviceTimeCoord*	/* events */
#endif
);

extern void	XFreeDeviceControl(
#if NeedFunctionPrototypes
    XDeviceControl*	/* control */
#endif
);

_XFUNCPROTOEND

#endif /* _XINPUT_H_ */
