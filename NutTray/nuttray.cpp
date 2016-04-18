/*
 * Copyright (C) 2009 Adam Kropelin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include <windows.h>

#include <string>
#include "wintray.h"
#include "resource.h"
#include "winups.h"
#include "statmgr.h"

#include "instmgr.h"

#define CMDOPT_INSTALL  TEXT("/install")
#define CMDOPT_REMOVE   TEXT("/remove")
#define CMDOPT_KILL     TEXT("/kill")
#define CMDOPT_QUIET    TEXT("/quiet")

#define USAGE_TEXT   TEXT("[" CMDOPT_INSTALL "] "            \
                     "[" CMDOPT_REMOVE  "] "            \
                     "[" CMDOPT_KILL    "] "            \
                     "[" CMDOPT_QUIET   "]")


// Global variables
static HINSTANCE appinst;                       // Application handle
static bool quiet = false;                      // Suppress user dialogs
static const DWORD tSize = 20;
static TCHAR title[tSize];

void NotifyUser(DWORD format, ...)
{
   va_list args;
   const DWORD size = 2018;
   TCHAR buf[size];

   if (!quiet) {
      va_start(args, format);
	  if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE ,
				  appinst,
				  format,
				  0,
				  buf,
				  size,
				  &args))
			{
			  wprintf(L"Format message failed with 0x%x\n", GetLastError());
			  return;
			}
	  //vsnprintf(buf, sizeof(buf), format, args);
      va_end(args);

      MessageBox(NULL, buf, title, MB_OK|MB_ICONINFORMATION);
   }
}

void PostToNuttray(DWORD msg)
{
   HWND wnd = FindWindow(NUTTRAY_WINDOW_CLASS, NUTTRAY_WINDOW_NAME);
   if (wnd) {
	   PostMessage(wnd, msg, 0, 0);
	   CloseHandle(wnd);
   }
}

int Install()
{
   // Get the full path/filename of this executable
	TCHAR path[4096];
   GetModuleFileName(NULL, path, sizeof(path));

   // Add double quotes
   TCHAR cmd[1024];
   swprintf(cmd, sizeof(cmd), TEXT("\"%s\""), path);

   // Open registry key for auto-run programs
   HKEY runkey;
   if (RegCreateKey(HKEY_LOCAL_MACHINE, 
					TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                    &runkey) != ERROR_SUCCESS) {
      NotifyUser(IDS_ERROR_INSTALL);
      return 1;
   }

   // Attempt to add Apctray key
   if (RegSetValueEx(runkey, TEXT("Apctray"), 0, REG_SZ, (BYTE*)cmd, (wcslen(cmd)+1)*sizeof(TCHAR)) != ERROR_SUCCESS) {
      RegCloseKey(runkey);
      NotifyUser(IDS_ERROR_INSTALL);
      return 1;
   }

   RegCloseKey(runkey);

   NotifyUser(IDS_INSTALL_SUCCESS);
   return 0;
}

int Remove()
{
   // Open registry key nuttray
   HKEY runkey;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                    0, KEY_READ|KEY_WRITE, &runkey) == ERROR_SUCCESS) {
      RegDeleteValue(runkey, TEXT("Apctray"));
      RegCloseKey(runkey);
   }

   NotifyUser(IDS_REMOVE);
   return 0;
}

int Kill()
{
   PostToNuttray(WM_CLOSE);

  
   HANDLE evt = OpenEvent(EVENT_MODIFY_STATE, FALSE, NUTTRAY_STOP_EVENT_NAME);
   if (evt != NULL)
   {
       SetEvent(evt);
       CloseHandle(evt);
   }
 

   return 0;
}

void Usage(LPCWSTR text1, LPCWSTR text2)
{
   MessageBox(NULL, text1, text2, MB_OK);
   MessageBox(NULL, USAGE_TEXT, TEXT("Nuttray Usage"),
              MB_OK | MB_ICONINFORMATION);
}

// This thread runs on Windows 2000 and higher. It monitors for the
// global exit event to be signaled (/kill).
static bool runthread = false;
static HANDLE exitevt;
DWORD WINAPI EventThread(LPVOID param)
{
   // Create global exit event and allow Adminstrator access to it so any
   // member of the Administrators group can signal it.
   exitevt = CreateEvent(NULL, TRUE, FALSE, NUTTRAY_STOP_EVENT_NAME);
   if (exitevt) {
	   TCHAR name[] = TEXT("Administrators");
	   // GrantAccess(exitevt, EVENT_MODIFY_STATE, TRUSTEE_IS_GROUP, name);

		// Wait for event to be signaled or for an error
	   DWORD rc = WaitForSingleObject(exitevt, INFINITE);
	   if (rc == WAIT_OBJECT_0)
		   PostToNuttray(WM_CLOSE);

	   CloseHandle(exitevt);
   }
   return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR CmdLine, int iCmdShow)
{
   //InitWinAPIWrapper();
   // Initialize Winsock
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
   // Publicize application handle
   appinst = hInstance;
   LoadString(appinst, IDS_TITLE, title, tSize);
	   
   // Check command line options
   LPWSTR *szArgList;
   int argCount;

   szArgList = CommandLineToArgvW(GetCommandLine(), &argCount);
   if (szArgList == NULL)
   {
	   MessageBox(NULL, L"Unable to parse command line", L"Error", MB_OK);
	   return 10;
   }

   for (int i = 1; i < argCount; i++)
   {
	   if (_wcsicmp(szArgList[i], CMDOPT_INSTALL) == 0) {
		   return Install();
	   }
	   else if (_wcsicmp(szArgList[i], CMDOPT_REMOVE) == 0) {
		   return Remove();
	   }
	   else if (_wcsicmp(szArgList[i], CMDOPT_KILL) == 0) {
		   return Kill();
	   }
	   else if (_wcsicmp(szArgList[i], CMDOPT_QUIET) == 0) {
		   quiet = true;
	   }
	   else {
		   Usage(szArgList[i], TEXT("Unknown option"));
		   return 1;
	   }
   }

   LocalFree(szArgList);
   
    
   

   // Check to see if we're already running in this session
   LPCWSTR semname = TEXT("Local\\nuttray");
   HANDLE sem = CreateSemaphore(NULL, 0, 1, semname);
   if (sem == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
      NotifyUser(IDS_ALREADY_RUNNING);
      WSACleanup();
      return 0;
   }

   // On Win2K and above we spawn a thread to watch for exit requests.
   HANDLE evtthread = NULL;
   runthread = true;
   evtthread = CreateThread(NULL, 0, EventThread, NULL, 0, NULL);
   
   if (evtthread == NULL) return -1;
   WPARAM generation = 0;
   InstanceManager instmgr(appinst);
   instmgr.CreateMonitors();

   // Enter the Windows message handling loop until told to quit
   MSG msg;
   while (GetMessage(&msg, NULL, 0, 0) > 0) {

      TranslateMessage(&msg);

      switch (LOWORD(msg.message)) {
      case WM_NUTTRAY_REMOVEALL:
         // Remove all instances (and close)
         instmgr.RemoveAll();
         PostQuitMessage(0);
         break;

      case WM_NUTTRAY_REMOVE:
         // Remove the given instance and exit if there are no more instances
		 if (instmgr.RemoveInstance((const char *)msg.lParam) == 0)
			  PostQuitMessage(0);
         break;

      case WM_NUTTRAY_ADD:
         // Remove the given instance and exit if there are no more instances
         instmgr.AddInstance();
         break;

      case WM_NUTTRAY_RESET:
         // Redraw tray icons due to explorer exit/restart
         // We'll get a RESET message from each tray icon but we only want
         // to act on the first one. Check the generation and only perform the
         // reset if the generation matches.
         if (msg.wParam == generation)
         {
            // Bump generation so we ignore redundant reset commands
            generation++;
            // Delay while other tray icons refresh. This prevents our icons
            // from being scattered across the tray.
            Sleep(2000);
            // Now command the instances to reset
            instmgr.ResetInstances();
         }
         break;

      default:
         DispatchMessage(&msg);
      }
   }

   // Wait for event thread to exit cleanly
   
    runthread = false;
    SetEvent(exitevt); // Kick exitevt to wake up thread
    if (WaitForSingleObject(evtthread, 5000) == WAIT_TIMEOUT)
        TerminateThread(evtthread, 0);
    CloseHandle(evtthread);
   

   WSACleanup();
   return 0;
}
