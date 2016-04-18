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

#ifndef STATMGR_H
#define STATMGR_H

#include <Windows.h>
#include "upsclient.h"

#include <list>
#include <string>
using namespace std;

#define MAX_STATS 256
#define MAX_DATA  100


class StatMgr
{
public:

   StatMgr(const char *host, unsigned short port);
   ~StatMgr();

   bool Update();
   string Get(const char* key);
   bool GetAll(list<string> &keys, list<string> &values);
   bool GetSummary(int &battstat, wstring &statstr, wstring &upsname);

private:

   bool open();
   void close();

   char *ltrim(char *str);
   void rtrim(char *str);
   char *trim(char *str);

   void lock();
   void unlock();

   struct keyval {
      const char *key;
      const char *value;
	  const char *data;
   };

   keyval          m_stats[MAX_STATS];
   string          m_host;
   unsigned short  m_port;
   UPSCONN         m_ups;
   HANDLE          m_mutex;
};

#endif // STATMGR_H
