/*
 * update.h: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __UPDATE_H
#define __UPDATE_H

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "i18n.h"
#include "config.h"
#include "channelmap.h"
#include "md5tools.h"

using namespace std;


#define TVTV_USERAGENT "libcurl-agent/1.0"

#define DEF_TVTV_SCHEDULE_UID	"uid"
#define DEF_TVTV_SCHEDULE_CHN	"channel"
#define DEF_TVTV_SCHEDULE_CHID  "channelid"
#define DEF_TVTV_SCHEDULE_STM	"starttime"
#define DEF_TVTV_SCHEDULE_VTM	"vps"
#define DEF_TVTV_SCHEDULE_ETM	"endtime"
#define DEF_TVTV_SCHEDULE_TIT	"title"
#define DEF_TVTV_SCHEDULE_NAT	"nature"
#define DEF_TVTV_SCHEDULE_DESC	"description"
#define DEF_TVTV_SCHEDULE_PERS	"persons"
#define DEF_TVTV_SCHEDULE_FRM	"format"
#define DEF_TVTV_SCHEDULE_FSK	"pg"
#define DEF_TVTV_SCHEDULE_ACT	"action"


#define TVTVSRV_CNT 8
static const char *TVTV_SERVERS[TVTVSRV_CNT] = {"www.tvtv.de", "www.tvtv.at", "www.tvtv.co.uk", "www.tvtv.fr",
                                                "www.tvtv.it", "www.tvtv.ch", "www.tvtv.es", "www.tvtv.nl"};


struct MemoryStruct {
    char   *memory;
    size_t  size;
};


class cUpdate : public cThread {
private:
    bool active;
    struct MemoryStruct data;
    cChannelMap *chanmap;

    uint8_t toHex(const uint8_t &x);
    void URLEncode(const char *sIn, char *sOut);
    void Get_Packed_String(char *sOut);

    void Action(void);
    int  DownloadCSVData(const char *url);
    bool MakeTimerUpdate(void);
    void ProcessImportedFile(const char *sBuffer);

    int  calc_field_cnt(string *s);
    char *strip_str(char *s);
    char **split_csv(const char *job_line, int field_cnt);
    string *read_line_from_buffer(const char *buf, unsigned int *idx);
public:
    cUpdate();
    ~cUpdate();
    void StartUpdate();
    int ReloadChannelMap();
};

#endif //__UPDATE_H
