/*
 * config.c: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include "config.h"

cTVTVConfig TVTVConfig;

cTVTVConfig::cTVTVConfig(void) {
  tvtv_server=0;
  strn0cpy(username,  "", sizeof(username));
  strn0cpy(password,  "", sizeof(password));
  autoupdate=0;
  updatetime=30;
  FormatRecordName = eRecordName_FormatNatureTitle;
  usestation=1;
  show_in_mainmenu=1;
  useproxy=0;
  strn0cpy(httpproxy, "127.0.0.1:8000", sizeof(httpproxy));
// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
  usevps=1;
#endif	        
#if VDRVERSNUM >= 10344
  usetvtvdescr=0;
#endif
  tvtv_bugfix=0;
  tvtv_bugfix_hrs=1;
} 

