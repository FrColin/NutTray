/*
 * Copyright (C) 2007 Adam Kropelin
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

#ifndef BALLOONMGR_H
#define BALLOONMGR_H

#include <windows.h>
#include <string>
#include <list>
using namespace std;

class BalloonMgr
{
public:

   BalloonMgr();
   ~BalloonMgr();

   void PostBalloon(HWND hwnd, const wstring &title, const wstring &text);
   static DWORD WINAPI Thread(LPVOID param);

private:

   void post();
   void clear();
   void lock();
   void unlock();
   void signal();

   struct Balloon {
      HWND hwnd;
      wstring title;
      wstring text;
   };

   list<Balloon>       _pending;
   HANDLE               _mutex;
   bool                 _exit;
   bool                 _active;
   HANDLE               _event;
   HANDLE               _timer;
   DWORD 				_time;
   HANDLE               _thread;
};

#endif // BALLOONMGR_H
