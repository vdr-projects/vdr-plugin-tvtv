/*
 * config.h: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef __TVTV_CONFIG_H
#define __TVTV_CONFIG_H

#include <vdr/interface.h>
#include <vdr/plugin.h>
#include <vdr/tools.h>
#include <vdr/status.h>

enum eRecordName
{
  eRecordName_Title,
  eRecordName_NatureTitle,
  eRecordName_TitleNature,
  eRecordName_FormatNatureTitle,
  eRecordName_FormatTitleNature,
  eRecordName_MAX
};

enum eTimeShiftBugfix
{
  eTimeShiftBugfixOff,
  eTimeShiftBugfixAuto,
  eTimeShiftBugfixManual,
  eTimeShiftBugfixMAX
};

struct cTVTVConfig
{
public:
  cTVTVConfig(void);

  int  tvtv_server;
  char username[30];
  char password[30];
  int  updatetime;
  int  autoupdate;
  int  AddOngoingNonVpsTimers;
  int  FormatRecordName;
  int  usestation;
  int  show_in_mainmenu;
  int  useproxy;
  char httpproxy[256];
// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
  int  usevps;
#endif
#if VDRVERSNUM >= 10344
  int  usetvtvdescr;
#endif
  int tvtv_bugfix;
  int tvtv_bugfix_hrs;
};

extern cTVTVConfig TVTVConfig;

#endif // __TVTV_CONFIG_H 
