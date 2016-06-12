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

#include <windows.h>
#include "upsclient.h"
#include "statmgr.h"

StatMgr::StatMgr(const char *host, unsigned short port)
   : m_host(host),
     m_port(port)
{
   memset(m_stats, 0, sizeof(m_stats));
}

StatMgr::~StatMgr()
{
   lock();
   close();
}

bool StatMgr::Update()
{
   lock();

   int tries;
   for (tries = 2; tries; tries--)
   {
	   if (m_ups.fd == INVALID_SOCKET) {
		   if (upscli_connect(&m_ups, m_host.c_str(), m_port, 0)) {
			   // Hard failure: bail immediately
			   unlock();
			   return false;
		   }
	   }
	  const char *query_ups[] = { "UPS" };
	  if ( upscli_list_start(&m_ups, 1, query_ups)<0) {
         // Soft failure: close and try again
         close();
         continue;
      }
	  // free up strdup !!!
	  for (int idx = 0; idx < MAX_STATS && m_stats[idx].key; idx++)
	  {
		  if (m_stats[idx].key) free((void*)m_stats[idx].key);
		  if (m_stats[idx].value) free((void*)m_stats[idx].value);
	  }
      memset(m_stats, 0, sizeof(m_stats));

      int len = 0;
      int i = 0;
	  unsigned int numa;
	  char **answer;
	  
      while (i < MAX_STATS && upscli_list_next(&m_ups, 1, query_ups, &numa, &answer) > 0)
      {
		  /*
         for (unsigned int j = 0; j < numa; j++) {
			 _RPT1(_CRT_WARN, "UPS Arg %s\n", answer[j]);
		 }
		 */
		 if (numa > 0) m_stats[i].key = _strdup(answer[0]);
		 if (numa > 1) m_stats[i].value = _strdup(answer[1]);
		 if (numa > 2) m_stats[i].data = _strdup(answer[2]);
		 i++;
		
      }
	  const char *query_vars[] = { "VAR", m_stats[0].value };
	  if (upscli_list_start(&m_ups, 2, query_vars)<0) {
		  // Soft failure: close and try again
		  close();
		  continue;
	  }
	  while (i < MAX_STATS && upscli_list_next(&m_ups, 2, query_vars, &numa, &answer) > 0)
	  {
		  /*
		  for (unsigned int j = 0; j < numa; j++) {
			  _RPT1(_CRT_WARN, "VAR Arg %s\n", answer[j]);
		  }
		  */
		  if (numa > 2) m_stats[i].key = _strdup(answer[2]);
		  if (numa > 3) m_stats[i].value = _strdup(answer[3]);
		  i++;
	  }

      // Good update, bail now
      if (i > 0 && len == 0)
         break;

      // Soft failure: close and try again
      close();
   }

   unlock();
   return tries > 0;
}

string StatMgr::Get(const char* key)
{
   string ret;

   lock();
   for (int idx=0; idx < MAX_STATS && m_stats[idx].key; idx++) {
      if (strcmp(key, m_stats[idx].key) == 0) {
         if (m_stats[idx].value)
            ret = m_stats[idx].value;
         break;
      }
   }
   unlock();

   return ret;
}

bool StatMgr::GetAll(list<string> &keys, list<string> &values)
{
   keys.clear();
   values.clear();

   lock();
   for (int idx=0; idx < MAX_STATS && m_stats[idx].key; idx++)
   {
	   if (m_stats[idx].key) {
		   keys.push_back(m_stats[idx].key);
		   values.push_back(m_stats[idx].value);
	   }
   }
   unlock();

   return true;
}


char *StatMgr::ltrim(char *str)
{
   while(isspace(*str))
      *str++ = '\0';

   return str;
}

void StatMgr::rtrim(char *str)
{
   char *tmp = str + strlen(str) - 1;

   while (tmp >= str && isspace(*tmp))
      *tmp-- = '\0';
}

char *StatMgr::trim(char *str)
{
   str = ltrim(str);
   rtrim(str);
   return str;
}

void StatMgr::lock()
{
   WaitForSingleObject(m_mutex, INFINITE);
}

void StatMgr::unlock()
{
   ReleaseMutex(m_mutex);
}

bool StatMgr::open()
{
   if (m_ups.fd != INVALID_SOCKET)
      close();
   upscli_connect(&m_ups, m_host.c_str() , m_port, 0);
   return m_ups.fd != INVALID_SOCKET;
}

void StatMgr::close()
{
   if (m_ups.fd != INVALID_SOCKET) {
	   upscli_disconnect(&m_ups);
	   m_ups.fd = INVALID_SOCKET;
   }
}

bool StatMgr::GetSummary(int &battstat, wstring &statstr, wstring &upsname)
{
   // Fetch data from the UPS
   if (!Update()) {
      battstat = -1;
      statstr = TEXT("NETWORK ERROR");
      return false;
   }

   // Lookup the STATFLAG key
   string status = Get("ups.status");
   if (status.empty()) {
      battstat = -1;
      statstr = TEXT("ERROR");
      return false;
   }
   

   // Lookup BCHARGE key
   string bcharge = Get("battery.charge");

   // Determine battery charge percent
   if (status == "OB")
      battstat = 0;
   else if (!bcharge.empty())
      battstat = atoi(bcharge.c_str());
   else
      battstat = 100;

   // Fetch UPSNAME
   string uname = Get("UPS");
   if (!uname.empty())
      upsname = wstring(uname.begin(),uname.end());

   // Now output status in human readable form
   statstr = TEXT("");
   if (status == "calibration")
      statstr += TEXT("CAL ");
   if (status == "trim")
      statstr += TEXT("TRIM ");
   if (status == "boost")
      statstr += TEXT("BOOST ");
   if (status == "OL")
      statstr += TEXT("ONLINE ");
   if (status == "OB")
      statstr += TEXT("ONBATT ");
   if (status == "overload")
      statstr += TEXT("OVERLOAD ");
   if (status == "BL")
      statstr += TEXT("LOWBATT ");
   if (status == "RB")
      statstr += TEXT("REPLACEBATT ");
   if (status == "NB")
      statstr += TEXT("NOBATT ");

   // This overrides the above
   if (status == "CL") {
      statstr = TEXT("COMMLOST");
      battstat = -1;
   }

   // This overrides the above
   if (status == "SD")
      statstr = TEXT("SHUTTING DOWN");

   // This overrides the above
   if (status == "OB") {
      string reason = Get("input.transfer.reason");
      if (strstr(reason.c_str(), "self test"))
         statstr = TEXT("SELFTEST");
   }

   // Remove trailing space, if present
   //statstr.rtrim();
   return true;
}
