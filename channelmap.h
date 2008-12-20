/*
 * channelmap.h: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _CHANNELMAP__H
#define _CHANNELMAP__H

#include <map>
#include <fstream>
#include <string>
#include <stdlib.h>

#include <vdr/plugin.h>
#include <vdr/tools.h>

using namespace std;


class cChannelMap {
private:
  map<int, char *> chanmap;
  void remove_whitespaces(char *s);
  int read_config_file();
public:
  cChannelMap();
  ~cChannelMap();
  int ReloadChannelMap();
  char *GetChanStr(int tvtvid);
  tChannelID GetChanID(int tvtvid);
};

#endif
