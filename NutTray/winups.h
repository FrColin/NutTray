// This file has been adapted to the Win32 version of Apcupsd
// by Kern E. Sibbald.  Many thanks to ATT and James Weatherall,
// the original author, for providing an excellent template.
//
// Rewrite/Refactoring by Adam Kropelin
//
// Copyright (2007) Adam D. Kropelin
// Copyright (2000) Kern E. Sibbald
//

#ifndef WINUPS_H
#define WINUPS_H

// WinUPS header file

#include <windows.h>

// Application specific messages
enum {
   // Message used for system tray notifications
   WM_NUTTRAY_NOTIFY = WM_USER+1,

   // Message used to remove all nuttray instances from the registry
   WM_NUTTRAY_REMOVEALL,

   // Message used to remove specified nuttray instance from the registry
   WM_NUTTRAY_REMOVE,

   // Messages used to trigger redraw of tray icons
   WM_NUTTRAY_RESET,

   // Message used to add a new nuttray instance
   WM_NUTTRAY_ADD
};


// nuttray window constants
#define NUTTRAY_WINDOW_CLASS	TEXT("nuttray")
#define NUTTRAY_WINDOW_NAME		TEXT("nuttray")



// Names of various global events
#define NUTTRAY_STOP_EVENT_NAME  TEXT("Global\\ApctrayStopEvent")

#endif // WINUPS_H
