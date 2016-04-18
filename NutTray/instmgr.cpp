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

#include "instmgr.h"
#include "wintray.h"
#include <stdio.h>

const TCHAR *InstanceManager::INSTANCES_KEY = TEXT("Software\\Apcupsd\\Apctray\\instances");

const MonitorConfig InstanceManager::DEFAULT_CONFIG =
{
   "",            // id
   "127.0.0.1",   // host
   3493,          // port
   5,             // refresh
   true           // popups
};

InstanceManager::InstanceManager(HINSTANCE appinst) :
   _appinst(appinst)
{
}

InstanceManager::~InstanceManager()
{
   // Destroy all instances
   for (list<InstanceConfig>::const_iterator inst = _instances.begin(); inst !=_instances.end();inst++)
   {
      inst->menu->Destroy();
      delete inst->menu;
      //_instances.remove(*inst);
   }
   _instances.clear();
}

void InstanceManager::CreateMonitors()
{
   list<InstanceConfig> unsorted;
   InstanceConfig config;

   // Open registry key for instance configs
   HKEY nuttray;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, INSTANCES_KEY, 0, KEY_READ, &nuttray)
         == ERROR_SUCCESS)
   {
      // Iterate though all instance keys, reading the config for each 
      // instance into a list.
      int i = 0;
      char name[128];
      DWORD len = sizeof(name);
      while (RegEnumKeyExA(nuttray, i++, name, &len, NULL, NULL,
                          NULL, NULL) == ERROR_SUCCESS)
      {
         unsorted.push_back(ReadConfig(nuttray, name));
         len = sizeof(name);
      }
      RegCloseKey(nuttray);

      // Now that we've read all instance configs, place them in a sorted list.
      // This is a really inefficient algorithm, but even O(N^2) is tolerable
      // for small N.
      list<InstanceConfig>::iterator iter, lowest;
	  for (lowest = unsorted.begin(); lowest != unsorted.end(); lowest++)
      {
         lowest = unsorted.begin();
         for (iter = unsorted.begin(); iter != unsorted.end(); ++iter)
         {
            if (iter->order < lowest->order)
               lowest = iter;
         }

         _instances.push_back(*lowest);
         //unsorted.remove(*lowest);
      }
	  unsorted.clear();
   }

   // If no instances were found, create a default one
   if (_instances.empty())
   {
      InstanceConfig config;
      config.mcfg = DEFAULT_CONFIG;
      config.mcfg.id = CreateId();
      _instances.push_back(config);
      Write();
   }

   // Loop thru sorted instance list and create an upsMenu for each
   list<InstanceConfig>::iterator iter;
   for (iter = _instances.begin(); iter != _instances.end(); ++iter)
      iter->menu = new upsMenu(_appinst, iter->mcfg, &_balmgr, this);
}

InstanceManager::InstanceConfig InstanceManager::ReadConfig(HKEY key, const string & id)
{
   InstanceConfig config;
   config.mcfg = DEFAULT_CONFIG;
   config.mcfg.id = id;

   // Read instance config from registry
   HKEY subkey;
   if (RegOpenKeyExA(key, id.c_str(), 0, KEY_READ, &subkey) == ERROR_SUCCESS)
   {
	   DWORD port;
      RegQueryString(subkey, "host", config.mcfg.host);
      RegQueryDWORD(subkey, "port", port);
      RegQueryDWORD(subkey, "refresh", config.mcfg.refresh);
      RegQueryDWORD(subkey, "popups", config.mcfg.popups);
      RegQueryDWORD(subkey, "order", config.order);
      RegCloseKey(subkey);
	  config.mcfg.port = (unsigned short) port;
   }

   return config;
}

void InstanceManager::Write()
{
   // Remove all existing instances from the registry
   HKEY nuttray;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, INSTANCES_KEY,
                    0, KEY_READ|KEY_WRITE, &nuttray) == ERROR_SUCCESS)
   {
      // Iterate though all nuttray keys locating instance keys of the form
      // "instanceN". Add the id of each key to a list. We don't want to
      // remove keys while we're enumerating them since Win32 docs say to not
      // change the key while enumerating it.
      int i = 0;
      TCHAR name[128];
      DWORD len = sizeof(name);
      list<LPCWSTR> ids;
      while (RegEnumKeyEx(nuttray, i++, name, &len, NULL, NULL,
                          NULL, NULL) == ERROR_SUCCESS)
      {
         ids.push_back(name);
         len = sizeof(name);
      }

      // Now that we have a list of all ids, loop thru and delete each one
      list<LPCWSTR>::iterator iter;
      for (iter = ids.begin(); iter != ids.end(); ++iter)
         RegDeleteKey(nuttray, *iter);
   }
   else
   {
      // No nuttray\instances key (and therefore no instances) yet
      if (RegCreateKey(HKEY_LOCAL_MACHINE, INSTANCES_KEY, &nuttray) 
            != ERROR_SUCCESS)
         return;
   }

   // Write out instances. Our instance list is in sorted order but may
   // have gaps in the numbering, so regenerate 'order' value, ignoring what's
   // in the InstanceConfig.order field.
   int count = 0;
   list<InstanceConfig>::iterator iter;
   for (iter = _instances.begin(); iter != _instances.end(); ++iter)
   {
      // Create the instance and populate it
      HKEY instkey;
      if (RegCreateKeyA(nuttray, iter->mcfg.id.c_str(), &instkey) == ERROR_SUCCESS)
      {
         RegSetString(instkey, "host", iter->mcfg.host);
         RegSetDWORD(instkey, "port", iter->mcfg.port);
         RegSetDWORD(instkey, "refresh", iter->mcfg.refresh);
         RegSetDWORD(instkey, "popups", iter->mcfg.popups);
         RegSetDWORD(instkey, "order", count);
         RegCloseKey(instkey);
         count++;
      }
   }
   RegCloseKey(nuttray);
}

list<InstanceManager::InstanceConfig>::iterator 
   InstanceManager::FindInstance(const string & id)
{
   list<InstanceConfig>::iterator iter;
   for (iter = _instances.begin(); iter != _instances.end(); ++iter)
   {
      if (iter->mcfg.id == id)
         return iter;
   }
   return _instances.end();
}

int InstanceManager::RemoveInstance(const string & id)
{
   list<InstanceConfig>::iterator inst = FindInstance(id);
   if (inst != _instances.end())
   {
      inst->menu->Destroy();
      delete inst->menu;
      //_instances.remove(*inst);
	  _instances.erase(inst);
      Write();
   }

   return _instances.size();
}

void InstanceManager::AddInstance()
{
   InstanceConfig config;
   config.mcfg = DEFAULT_CONFIG;
   config.mcfg.id = CreateId();
   config.menu = new upsMenu(_appinst, config.mcfg, &_balmgr, this);
   _instances.push_back(config);
   Write();
}

void InstanceManager::UpdateInstance(const MonitorConfig &mcfg)
{
   list<InstanceConfig>::iterator inst = FindInstance(mcfg.id.c_str());
   if (inst != _instances.end())
   {
      inst->mcfg = mcfg;
      inst->menu->Reconfigure(mcfg);
      Write();
   }
}

void InstanceManager::RemoveAll()
{
   while (!_instances.empty())
      RemoveInstance(_instances.begin()->mcfg.id);
}

void InstanceManager::ResetInstances()
{
   list<InstanceConfig>::iterator iter;
   for (iter = _instances.begin(); iter != _instances.end(); ++iter)
      iter->menu->Redraw();
}

string InstanceManager::CreateId()
{
   // Create binary UUID
   UUID uuid;
   UuidCreate(&uuid);

   // Convert binary UUID to RPC string
   RPC_CSTR tmpstr;
   UuidToStringA(&uuid, &tmpstr);   
  
   // Copy string UUID to string and free RPC version
   string uuidstr((char*)tmpstr);
   RpcStringFreeA(&tmpstr);

   return uuidstr;
}

bool InstanceManager::IsAutoStart()
{
   HKEY hkey;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                    TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                    0, KEY_READ, &hkey) != ERROR_SUCCESS)
      return false;

   string path;
   RegQueryString(hkey, "Apctray", path);
   RegCloseKey(hkey);

   return !path.empty();
}

void InstanceManager::SetAutoStart(bool start)
{
   HKEY runkey= 0;
   if (start)
   {
      // Get the full path/filename of this executable
      char path[1024];
      GetModuleFileNameA(NULL, path, sizeof(path));

      // Add double quotes
      char cmd[1024];
      snprintf(cmd, sizeof(cmd), "\"%s\"", path);

      // Open registry key for auto-run programs
      if (RegCreateKey(HKEY_LOCAL_MACHINE, 
                       TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       &runkey) != ERROR_SUCCESS)
      {
         return;
      }

      // Add nuttray key
      RegSetString(runkey, "Apctray", cmd);
   }
   else
   {
      // Open registry key nuttray
      HKEY runkey;
      if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
					TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       0, KEY_READ|KEY_WRITE, &runkey) == ERROR_SUCCESS)
      {
         RegDeleteValue(runkey, TEXT("Apctray"));
      }
   }

   RegCloseKey(runkey);
}

bool InstanceManager::RegQueryDWORD(HKEY hkey, const char* name, DWORD &result)
{
   DWORD data;
   DWORD len = sizeof(data);
   DWORD type;

   // Retrieve DWORD
   if (RegQueryValueExA(hkey, name, NULL, &type, (BYTE*)&data, &len)
         == ERROR_SUCCESS && type == REG_DWORD)
   {
      result = data;
      return true;
   }

   return false;
}

bool InstanceManager::RegQueryString(HKEY hkey, const char* name, string &result)
{
   char data[512]; // Arbitrary max
   DWORD len = sizeof(data);
   DWORD type;

   // Retrieve string
   if (RegQueryValueExA(hkey, name, NULL, &type, (BYTE*)data, &len)
         == ERROR_SUCCESS && type == REG_SZ)
   {
      result = string(data);
      return true;
   }

   return false;
}

void InstanceManager::RegSetDWORD(HKEY hkey, const char* name, DWORD value)
{
   RegSetValueExA(hkey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
}

void InstanceManager::RegSetString(HKEY hkey, const char* name, const string &value)
{
   RegSetValueExA(hkey, name, 0, REG_SZ, (BYTE*)value.c_str(), (value.length()+1)*sizeof(TCHAR));
}
