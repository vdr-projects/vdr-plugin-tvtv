/*
 * update.c: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "update.h"


cUpdate::cUpdate()
{
    active = false;
    data.memory = NULL;
    data.size = 0;
    chanmap = new cChannelMap();
}

cUpdate::~cUpdate()
{
    if (active) {
	active = false;
	Cancel(3);
    }

    if (data.memory) {
        free(data.memory);
        data.memory = NULL;
        data.size = 0;
    }
}

void cUpdate::StartUpdate()
{
    if (!active)
	Start();
    else {
	active = false;
	Cancel(3);
	usleep(250);
	Start();
    }
}

int cUpdate::ReloadChannelMap() {
  return chanmap->ReloadChannelMap();
}

void cUpdate::Action(void)
{
    dsyslog("TVTV: Timer Thread started (pid=%d)", getpid());
    active = true;
    do {
	MakeTimerUpdate();
	if (TVTVConfig.autoupdate && TVTVConfig.updatetime > 0)
	    for (long i = 0; active && i < TVTVConfig.updatetime * 60 * 2; i++)
		usleep(500000);
    }
    while (active && TVTVConfig.autoupdate && TVTVConfig.updatetime > 0);
    
    active = false;
    dsyslog("TVTV: Timer Thread ended (pid=%d)", getpid());
    Cancel(0);
}


void cUpdate::Get_Packed_String(char *sOut)
{
  char *s;
  unsigned char signature[17];

  asprintf(&s, "EPGSync%s%s", TVTVConfig.username, TVTVConfig.password);

  MD5_CTX ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, (unsigned char *)s, strlen(s));
  MD5Final(signature, &ctx);
  signature[16] = 0;

  char aStr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  char aOut[30];
  int j = 0;

  aOut[16] = '\0';

  for (int i = 0; i < 16; i+= 3) {
      uint8_t x, y;

      x = signature[i];
      x >>= 2;
      x = x & 0x3f;
      aOut[j++] = aStr[x];

      y = signature[i];
      y = y & 0x3;
      y <<= 4;

      x = signature[i + 1];
      x >>= 4;
      x = x & 0x0f;
      x = x ^ y;
      aOut[j++] = aStr[x];

      if (i + 1 < 16) {
	  y = signature[i + 1];
	  y = y & 0x0f;
	  y <<= 2;

	  // 3
	  x = signature[i + 2];
	  x = x & 0xff;
	  x >>= 6;
	  x = x ^ y;
	  aOut[j++] = aStr[x];

	  // 4
	  x = signature[i + 2];
	  x &= 0x3f;
	  aOut[j++] = aStr[x];
        }
      else {
	  aOut[j++] = (char) 0x3d;
	  aOut[j++] = (char) 0x3d;
        }
      aOut[j] = '\0';
    }

  strcpy(sOut, aOut);
  free(s);
}


static size_t WriteMemoryCallback( void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)data;

    if (mem->memory) 
        mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
    else
	mem->memory = (char *)malloc(mem->size + realsize + 1);
    
    if (mem->memory) {
        memcpy(&(mem->memory[mem->size]), ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }
    return realsize;
}


int cUpdate::DownloadCSVData(const char *url) {
    CURL *curl_handle;

    // Initialize 'data' struct
    if (data.memory) free(data.memory);
    data.memory = NULL;
    data.size = 0;
    
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        esyslog("TVTV: Something went wrong with curl_global_init()");
	return -1;
    }
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        esyslog("TVTV: Unable to get handle from curl_easy_init()");
	return -1;
    }
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);  // Specify URL to get
    
    if (TVTVConfig.useproxy) {
        curl_easy_setopt(curl_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
	curl_easy_setopt(curl_handle, CURLOPT_PROXY, TVTVConfig.httpproxy);  // Specify HTTP proxy
    }

    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);  // Send all data to this function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&data);  // Pass our 'data' struct to the callback function
    curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, 1048576);  // Set maximum file size to get (bytes)
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);  // No progress meter
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);  // No signaling
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30);  // Set timeout to 30 seconds
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, TVTV_USERAGENT);  // Some servers don't like requests that are made without a user-agent field
    
    if (curl_easy_perform(curl_handle) != 0) {
	curl_easy_cleanup(curl_handle);  // Cleanup curl stuff
	if (data.memory) {
	    free(data.memory);
	    data.memory = NULL;
	    data.size = 0;
	}
	esyslog("TVTV: Couldn't download the stream");
	return -1;
    }

    curl_easy_cleanup(curl_handle);  // Cleanup curl stuff
    return data.size;
}


uint8_t cUpdate::toHex(const uint8_t &x)
{
    return x > 9 ? x + 55: x + 48;
}
    
void cUpdate::URLEncode(const char *sIn, char *sOut)
{
    register char *pOutTmp = NULL;
    char *pOutBuf = NULL;
    register char *pInTmp = NULL;
    char *pInBuf = (char *)sIn;
    
    //alloc out buffer
    pOutBuf = sOut;
	
    if(pOutBuf) {
        pInTmp	= pInBuf;
        pOutTmp = pOutBuf;
				    
	// do encoding
	while (*pInTmp) {
	    if(isalnum(*pInTmp))
	        *pOutTmp++ = *pInTmp;
	    else
	        if(isspace(*pInTmp))
		    *pOutTmp++ = '+';
	        else {
	            *pOutTmp++ = '%';
		    *pOutTmp++ = toHex(*pInTmp>>4);
	            *pOutTmp++ = toHex(*pInTmp%16);
		}
	    pInTmp++;
	}
	*pOutTmp = '\0';
    }
}


bool cUpdate::MakeTimerUpdate(void)
{
    char *sEnCryptS = NULL;
    char *sPackedString = NULL;
    char *url = NULL;
    
    sPackedString = (char *)calloc(30, sizeof (char));
    sEnCryptS = (char *)calloc(100, sizeof (char));

    Get_Packed_String(sPackedString);
    dsyslog("TVTV: Packed String: %s", sPackedString);
    URLEncode(sPackedString, sEnCryptS);    

    asprintf(&url, "http://%s/cgi-bin/WebObjects/TVSync.woa/wa/getjobs?&serial=0&account=%s&product=35&target=%s&doctype=csv&access=%s",
             TVTV_SERVERS[TVTVConfig.tvtv_server], TVTVConfig.username, TVTV_SERVERS[TVTVConfig.tvtv_server], sEnCryptS);

    isyslog("TVTV: Timer Update started");
    
    int iFileSize = DownloadCSVData(url);
    
    if (iFileSize > 0) {
	dsyslog("TVTV: Received %d Bytes", iFileSize);
	/* process CSV file */
	ProcessImportedFile(data.memory);
	isyslog("TVTV: Timer File processed");
    }
    else {
	esyslog("TVTV: Download Error");
    }

    // Clean up
    if (data.memory) {
	free(data.memory);
	data.memory = NULL;
	data.size = 0;
    }
    if (url) free(url);
    if (sEnCryptS) free(sEnCryptS);
    if (sPackedString) free(sPackedString);
    
    return true;
}


int cUpdate::calc_field_cnt(string *s) {
  int n=0;
  size_t p = 0;

  if ((s == NULL) || (s->empty())) return 0;

  p = s->find(',', 0);
  while (p != string::npos) {
    n++;
    p = s->find(',', p+1);
  }
  n++;
  return n;
}

char *cUpdate::strip_str(char *s) {
  char *m=NULL;
  unsigned int i=0;

  if (s==NULL) return NULL;

  m=(char *)calloc(strlen(s)+1, sizeof(char));

  for (i=0; i<strlen(s); i++) {
    if (s[i] == '"') {
      if (s[i+1] == '"') {
	strncat(m, s+i, 1);
	i++;
      }
    } else if ((s[i] == '\\')&&(s[i+1] == 'n')) {
      strncat(m, "\n", 1);
      i++;
    } else {
      strncat(m, s+i, 1);
    }
  }
  return m;
}


char **cUpdate::split_csv(const char *job_line, int field_cnt) {

  char **arr=NULL;
  char *sp=NULL, *ep=NULL;
  char *s=NULL;
  int i=0;
  int n=0;
  int fini=0;

  if ((job_line == NULL)||(field_cnt == 0)) { return NULL; }

  arr=(char **)calloc(field_cnt, sizeof(char *));
  sp=(char *)job_line;
  
  while ((n < field_cnt)&&(fini == 0)) {
    if (*sp == '"') {
      sp++;
      ep=index(sp, '"');
      if (ep != NULL) {
        while ( *(ep+1) == '"' ) { ep=index(ep+2, '"'); }
        ep++;
      } else {
 	ep=(char *)job_line + strlen(job_line);
	fini=1;
      }
    } else {
      ep=index(sp, ',');
      if (ep == NULL) {
	ep=(char *)job_line + strlen(job_line);
	fini=1;
      }
    }
    s=(char *)calloc(ep-sp+1, sizeof(char));
    strncpy(s, sp, ep-sp);
    arr[n]=strip_str(s);
    free(s);
    
    sp=ep+1;
    n++;
    
    if ((n < field_cnt)&&(fini==1)) {
      for(i=0; i<n; i++) free(arr[i]);
      free(arr);
      arr=NULL;
    }
    
  }
  return arr;
}


string *cUpdate::read_line_from_buffer(const char *buf, unsigned int *idx) {
  string s;

  if (buf == NULL) return NULL;

  while ((buf[*idx] != 0)&&(buf[*idx] != '\n')) {
    if (!iscntrl(buf[*idx])) s.append(1, buf[*idx]);
    (*idx)++;
  }
  while ( (buf[*idx] != 0) && (iscntrl(buf[*idx]) )) (*idx)++;
  return new string(s);
}

void cUpdate::ProcessImportedFile(const char *sBuffer)
{
    char **fields=NULL;
    char **tvtvjob=NULL;
    int field_cnt=0;
    unsigned int p=0;
    size_t rp=0;
    bool vps = false;
    bool tzfix_start = false;
    bool tzfix_end = false;
    bool tzfix_vps = false;
    tChannelID vdrch, channelID;
    bool timer_update=false;
#if VDRVERSNUM < 10336
    unsigned int max_desc_len=0;
#endif

    time_t tStartTime, tVpsTime, tEndTime, tCurrentTime;
    struct tm tStart, tVps, tEnd;
    int StartTZ, VpsTZ, EndTZ;

    int tvtv_bugfix_secs = 0; /* seconds of TVTV bugfix (sends sometimes UTC instead of local time) */
    struct tm *timelocal;
    time_t tloc;
		
    cTimer *oTimer = NULL;
    cTimer *ti = NULL;
    cChannel *oTVTVChannel = NULL;

    map<string, string> tvtv_timer;
    string *sLine=NULL;
    string s;

    const string sTokenTvtvUid = "TVTV-UID: ";
    
    dsyslog("TVTV: Processing CSV data");

    // skip header
    do {
      sLine = read_line_from_buffer(sBuffer, &p);
      field_cnt = calc_field_cnt(sLine);
    } while (( field_cnt < 10 ) && ( !sLine->empty() ));
    fields=split_csv(sLine->c_str(), field_cnt);
    dsyslog("TVTV: Found %d fields in CSV data", field_cnt);

    // read timer jobs and create timers
    dsyslog("TVTV: Start reading timer jobs");
    sLine = read_line_from_buffer(sBuffer, &p);
    while (!sLine->empty()) {
      isyslog("TVTV: Received '%s...'", sLine->substr(0,130).c_str());
      tvtvjob=split_csv(sLine->c_str(), field_cnt);
      if (tvtvjob != NULL) {
        for (int i=0; i<field_cnt; i++) {
	  tvtv_timer[fields[i]] = tvtvjob[i];
	  free(tvtvjob[i]);
        }
        free(tvtvjob);
      }

      // replace newline characters by '|'
      rp = tvtv_timer[DEF_TVTV_SCHEDULE_DESC].find('\n',0 );
      while (rp != string::npos) { tvtv_timer[DEF_TVTV_SCHEDULE_DESC].replace(rp, 1, 1, '|'); rp = tvtv_timer[DEF_TVTV_SCHEDULE_DESC].find('\n',0 ); }
      rp = tvtv_timer[DEF_TVTV_SCHEDULE_PERS].find('\n',0 );
      while (rp != string::npos) { tvtv_timer[DEF_TVTV_SCHEDULE_PERS].replace(rp, 1, 1, '|'); rp = tvtv_timer[DEF_TVTV_SCHEDULE_PERS].find('\n',0 ); }

      // Get ChannelID from channel map
      vdrch=chanmap->GetChanID(atoi(tvtv_timer[DEF_TVTV_SCHEDULE_CHID].c_str()));
      if (!(vdrch == tChannelID::InvalidID)) {

#if VDRVERSNUM >= 10305
	oTVTVChannel = Channels.GetByChannelID(vdrch, true, true);
#else
        oTVTVChannel = Channels.GetByChannelID(vdrch, true);
#endif
	if (oTVTVChannel) {

	  oTimer = new cTimer;
			
	  // Calc Start and Stop Time
	  sscanf(tvtv_timer[DEF_TVTV_SCHEDULE_STM].c_str(), "%04d-%02d-%02d %02d:%02d:%02d %d",
		 &tStart.tm_year, &tStart.tm_mon, &tStart.tm_mday,
		 &tStart.tm_hour, &tStart.tm_min, &tStart.tm_sec, &StartTZ);
	      
	  sscanf(tvtv_timer[DEF_TVTV_SCHEDULE_VTM].c_str(), "%04d-%02d-%02d %02d:%02d:%02d %d",
		 &tVps.tm_year, &tVps.tm_mon, &tVps.tm_mday,
		 &tVps.tm_hour, &tVps.tm_min, &tVps.tm_sec, &VpsTZ);

	  sscanf(tvtv_timer[DEF_TVTV_SCHEDULE_ETM].c_str(), "%04d-%02d-%02d %02d:%02d:%02d %d",
		 &tEnd.tm_year, &tEnd.tm_mon, &tEnd.tm_mday,
		 &tEnd.tm_hour, &tEnd.tm_min, &tEnd.tm_sec, &EndTZ);
		  
	  tStart.tm_year -= 1900; tStart.tm_mon  -= 1; tStart.tm_isdst = -1;
	  tVps.tm_year   -= 1900; tVps.tm_mon    -= 1; tVps.tm_isdst   = -1;
	  tEnd.tm_year   -= 1900; tEnd.tm_mon    -= 1; tEnd.tm_isdst   = -1;		

	  time(&tCurrentTime); /* current time */

// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
	  vps = Setup.UseVps && (tVpsTime != -1) && TVTVConfig.usevps;
#else
	  vps = false;
#endif	        
	  if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixIgnore) {
	    // TimeZone handling not active, clear TimeZones
              isyslog("TVTV: timezones are ignored by configuration: tSTartTime: %+05d  tEndTime: %+05d", StartTZ, EndTZ);

	      tStartTime = mktime(&tStart);
	      tVpsTime   = mktime(&tVps);
	      tEndTime   = mktime(&tEnd);
	  } else {
	    // TimeZone handling active
	      tStartTime = timegm(&tStart);
	      tVpsTime   = timegm(&tVps);
	      tEndTime   = timegm(&tEnd);

	    if ((TVTVConfig.TimeZoneShiftBugFix == eTimeShiftBugfixManual) || (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) || (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST) ) {
	      tzfix_start = false;
	      tzfix_end   = false;
	      tzfix_vps   = false;

	      timelocal = localtime(&tStartTime);
	      if ((timelocal->tm_isdst < 0) && ((TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) || (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST))) {
                isyslog("TVTV: manual timezone shift skipped: DST information is not available for tSTartTime");
	      } else if (timelocal->tm_isdst == 0) {
		if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) {
                  isyslog("TVTV: manual timezone shift skipped: tSTartTime is non-DST but only enabled for DST");
		} else {
		  tzfix_start = true;
		};
	      } else if (timelocal->tm_isdst > 0) {
		if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST) {
                  isyslog("TVTV: manual timezone shift skipped: tSTartTime is DST but only enabled for non-DST");
		} else {
		  tzfix_start = true;
		};
	      };

	      timelocal = localtime(&tEndTime);
	      if ((timelocal->tm_isdst < 0) && ((TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) || (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST))) {
                isyslog("TVTV: manual timezone shift skipped: DST information is not available for tEndTime");
	      } else if (timelocal->tm_isdst == 0) {
		if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) {
                  isyslog("TVTV: manual timezone shift skipped: tEndTime is non-DST but only enabled for DST");
		} else {
		  tzfix_end = true;
		};
	      } else if (timelocal->tm_isdst > 0) {
		if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST) {
                  isyslog("TVTV: manual timezone shift skipped: tEndTime is DST but only enabled for non-DST");
		} else {
		  tzfix_end = true;
		};
	      };

	      if (vps) {
	        timelocal = localtime(&tVpsTime);
  	        if ((timelocal->tm_isdst < 0) && ((TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) || (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST))) {
                  isyslog("TVTV: manual timezone shift skipped: DST information is not available for tVpsTime");
	        } else if (timelocal->tm_isdst == 0) {
	  	  if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualDST) {
                    isyslog("TVTV: manual timezone shift skipped: tVpsTime is non-DST but only enabled for DST");
		  } else {
		    tzfix_vps = true;
		  };
	        } else if (timelocal->tm_isdst > 0) {
		  if (TVTVConfig.TimeZoneShiftBugFix == eTimeZoneBugfixManualNonDST) {
                    isyslog("TVTV: manual timezone shift skipped: tVpsTime is DST but only enabled for non-DST");
		  } else {
		    tzfix_vps = true;
		  };
	        };
	      };

	      if (tzfix_start == true) {
	        // shift buggy received timezones by manual given offset
                isyslog("TVTV: manual timezone shift tSTartTime: %+05d, fix to: %+05d", StartTZ, (int) (StartTZ + TVTVConfig.TimeZoneShiftHours * 100));
	        StartTZ += TVTVConfig.TimeZoneShiftHours * 100;
                tStartTime -= StartTZ*36;
	      };

	      if (tzfix_end == true) {
                isyslog("TVTV: manual timezone shift tEndTime: %+05d, fix to: %+05d", EndTZ, (int) (EndTZ + TVTVConfig.TimeZoneShiftHours * 100));
	        EndTZ += TVTVConfig.TimeZoneShiftHours * 100;
	        tEndTime -= EndTZ*36;
	      };
			
	      if (tzfix_vps == true) {
                isyslog("TVTV: manual timezone shift tVpsTime: %+05d, fix to: %+05d", VpsTZ, (int) (VpsTZ + TVTVConfig.TimeZoneShiftHours * 100));
	        VpsTZ += TVTVConfig.TimeZoneShiftHours * 100;
  	        tVpsTime -= VpsTZ*36;
	      };

	    } else if (TVTVConfig.TimeZoneShiftBugFix == eTimeShiftBugfixAuto) {
	      // fix buggy received timezones by autodetection (works only in case of CET only timers)
	      timelocal = localtime(&tStartTime);
	      if (timelocal->tm_gmtoff / 36 != StartTZ) {
                isyslog("TVTV: buggy timezone in tSTartTime autodetected: %+05d, fix to: %+05d", StartTZ, (int) (timelocal->tm_gmtoff / 36));
	        StartTZ = timelocal->tm_gmtoff / 36;
 	      };

	      timelocal = localtime(&tEndTime);
	      if (timelocal->tm_gmtoff / 36 != EndTZ) {
                isyslog("TVTV: buggy timezone in tEndTime autodetected: %+05d, fix to: %+05d", EndTZ, (int) (timelocal->tm_gmtoff / 36));
	        EndTZ = timelocal->tm_gmtoff / 36;
 	      };

	      tStartTime -= StartTZ*36;
	      tEndTime -= EndTZ*36;
			
	      if (vps) {
  	        timelocal = localtime(&tVpsTime);
	        if (timelocal->tm_gmtoff / 36 != VpsTZ) {
                  isyslog("TVTV: buggy timezone in tVpsTime autodetected: %+05d, fix to: %+05d", VpsTZ, (int) (timelocal->tm_gmtoff / 36));
	          VpsTZ = timelocal->tm_gmtoff / 36;
 	        };
	        tVpsTime -= VpsTZ*36;
	      };
	    };
	  };


	  tStartTime -= Setup.MarginStart * 60;
	  if (!vps) tEndTime += Setup.MarginStop * 60;
	  localtime_r(&tStartTime, &tStart);
	  localtime_r(&tVpsTime, &tVps);
	  localtime_r(&tEndTime, &tEnd);

	  s = ""; // start

	  // Adding optional Station Name to File Name to make it more unique
	  if (TVTVConfig.usestation) 
#if VDRVERSNUM >= 10315
            s = s + oTVTVChannel->ShortName(true) + "~";
#else
            s = s + oTVTVChannel->Name() + "~";
#endif

	  // Format as 1st
	  if (((TVTVConfig.FormatRecordName == eRecordName_FormatNatureTitle) || (TVTVConfig.FormatRecordName == eRecordName_FormatTitleNature)) && ! tvtv_timer[DEF_TVTV_SCHEDULE_FRM].empty()) {
	    s = tvtv_timer[DEF_TVTV_SCHEDULE_FRM] + "~";
	  };

	  // Nature as 1st or 2nd before title
	  if (((TVTVConfig.FormatRecordName == eRecordName_NatureTitle) || (TVTVConfig.FormatRecordName == eRecordName_FormatNatureTitle)) && ! tvtv_timer[DEF_TVTV_SCHEDULE_NAT].empty()) {
	    s = tvtv_timer[DEF_TVTV_SCHEDULE_NAT] + "~";
	  };

	  // Add Title to File Name
	  s += tvtv_timer[DEF_TVTV_SCHEDULE_TIT];

	  // Nature after title
	  if (((TVTVConfig.FormatRecordName == eRecordName_TitleNature) || (TVTVConfig.FormatRecordName == eRecordName_FormatTitleNature)) && ! tvtv_timer[DEF_TVTV_SCHEDULE_NAT].empty()) {
	    s = "~" + tvtv_timer[DEF_TVTV_SCHEDULE_NAT];
	  };

	  // Colons should be replaced by '|' in title
	  rp = s.find(':',0 ); 
	  while (rp != string::npos) { s.replace(rp, 1, 1, '|'); rp = s.find(':',0 ); }
	  
	  // create timer string
	  ostringstream oss(ostringstream::out);
	  oss.fill('0');
#if VDRVERSNUM >= 10305
	  oss << (tfActive|(vps?tfVps:tfNone)) << ":";
#else
          oss << "1:";
#endif
	  channelID = oTVTVChannel->GetChannelID();
	  oss << *channelID.ToString();
	  oss << ":";

// Starting with VDR 1.3.23 timer entries support a full date in ISO notation. 
#if VDRVERSNUM >= 10323
          oss.width(4); oss << ((vps?(tVps.tm_year):(tStart.tm_year))+1900) << "-";
	  oss.width(2); oss << ((vps?(tVps.tm_mon):(tStart.tm_mon))+1) << "-";
#endif
          oss.width(2); oss << (vps?(tVps.tm_mday):(tStart.tm_mday)) << ":";
	  oss.width(4); oss << (vps?(tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min)) << ":";
	  oss.width(4); oss << tEnd.tm_hour * 100 + tEnd.tm_min << ":";
	  oss << Setup.DefaultPriority << ":";
	  oss << Setup.DefaultLifetime << ":";
	  oss << s.c_str() << ":";
	  
	  dsyslog("TVTV: Timer entry '%s'", oss.str().c_str());
	  s=oss.str();

// Starting with VDR 1.3.44 the description within info.vdr is taken
// just from EPG. A description coming with a timer is added as a comment
// to info.vdr. This setup parameter decides whether to add this description
// as a comment or not.
#if VDRVERSNUM >= 10344
	  if (TVTVConfig.usetvtvdescr) {
#endif
	  
// Recording summary changed in VDR 1.3.25
#if VDRVERSNUM < 10325
	    s += tvtv_timer[DEF_TVTV_SCHEDULE_TIT];
	    if ((tvtv_timer[DEF_TVTV_SCHEDULE_PERS].length() > 0)||(tvtv_timer[DEF_TVTV_SCHEDULE_DESC].length() > 0)) s += "||";
#endif

	    if (tvtv_timer[DEF_TVTV_SCHEDULE_PERS].length() > 0) {
	      s += "Darsteller: " + tvtv_timer[DEF_TVTV_SCHEDULE_PERS];
  	      if (tvtv_timer[DEF_TVTV_SCHEDULE_DESC].length() > 0) s += "||" + tvtv_timer[DEF_TVTV_SCHEDULE_DESC];
	    } else {
	      if (tvtv_timer[DEF_TVTV_SCHEDULE_DESC].length() > 0) s += tvtv_timer[DEF_TVTV_SCHEDULE_DESC];
	    }

// Length of timer entry is limited to less than 10240 characters. Keeping off another
// 15 to avoid problems with additional characters (like newline). 
// This is not needed for VDR 1.3.36+ anymore!
#if VDRVERSNUM < 10336
	    max_desc_len = MAXPARSEBUFFER - sTokenTvtvUid.length() - tvtv_timer[DEF_TVTV_SCHEDULE_UID].length() - 15;
	    if (s.length() > max_desc_len) {
	      dsyslog("TVTV: received description too long, cutted (%d -> %d)", s.length(), max_desc_len);
	      s.erase(max_desc_len);
	    }
#endif

#if VDRVERSNUM >= 10344
	  }
#endif
	  // Add Timer's UID to description to identify it later on
	  if (tvtv_timer[DEF_TVTV_SCHEDULE_UID].length() > 0) s += "||" + sTokenTvtvUid + tvtv_timer[DEF_TVTV_SCHEDULE_UID];

	  
          if (oTimer->Parse(s.c_str())) {
	    cTimer *t = Timers.GetTimer(oTimer);
	    if (t) { // Timer exists
	      if (tvtv_timer[DEF_TVTV_SCHEDULE_ACT] == "delete") {
		if (t->Recording()) {
		  isyslog("TVTV: timer %d recording, will be deleted after recording has ended (%s)", t->Index() + 1, t->File());
		} else {
		  isyslog("TVTV: timer %d deleted [%s/%s/%d/%04d-%04d/%s]", oTimer->Index() + 1, 
		                                                         tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
		                                                         tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
									 vps ? (tVps.tm_mday):(tStart.tm_mday), 
									 vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
									 tEnd.tm_hour * 100 + tEnd.tm_min,
									 vps ? "VPS":"-");
		  Timers.Del(t);
		}
	      } else {
		isyslog("TVTV: timer %d exist (%s)", t->Index() + 1, t->File());
	      }
	      delete oTimer;
	    } else { // if (t)

	      // try to figure out if timer needs to be updated
	      timer_update=false;
	      ti = Timers.First(); 
	      while (ti != NULL) { 
#if VDRVERSNUM >= 10344
		if (ti->Aux() != NULL) {
		  s=ti->Aux();
#else
                if (ti->Summary() != NULL) {
	          s=ti->Summary();
#endif
		  if (s.find(sTokenTvtvUid + tvtv_timer[DEF_TVTV_SCHEDULE_UID], 0) != string::npos) {
		    timer_update=true;
		    break;
		  } else {
		    ti = Timers.Next(ti); 
		  }
		} else {
		  ti = Timers.Next(ti);
		}
	      }

	      if (timer_update) { // Timer exists
		if (tvtv_timer[DEF_TVTV_SCHEDULE_ACT] == "rec") {
		  // avoid Timer updates if Timer is shifted (TVTV UTC Problem)
		  if (TVTVConfig.tvtv_bugfix == eTimeShiftBugfixManual) {
		  	  // exactly configured hours
		          isyslog("TVTV: manual configured timezone shift: %d hrs", TVTVConfig.tvtv_bugfix_hrs);
			  tvtv_bugfix_secs = TVTVConfig.tvtv_bugfix_hrs * 3600;
		  } else if (TVTVConfig.tvtv_bugfix == eTimeShiftBugfixAuto) {
			  // autodetect time zone distance
			  time(&tloc);
			  timelocal = localtime(&tloc);
		          isyslog("TVTV: autodetected timezone: %s, shift: %d hrs", timelocal->tm_zone, (int) (timelocal->tm_gmtoff / 3600));
			  tvtv_bugfix_secs = timelocal->tm_gmtoff;
		  };

		  if ((TVTVConfig.tvtv_bugfix != eTimeShiftBugfixOff) && ((((ti->StartTime() - tStartTime) == tvtv_bugfix_secs) && 
		                                    ((ti->StopTime() - tEndTime) == tvtv_bugfix_secs)) || 
		                                   (vps && ((ti->StopTime() - tEndTime) == tvtv_bugfix_secs)) )) {
		    isyslog("TVTV: timer %d update rejected (%s) [%s/%s/%d/%04d-%04d/%s]", ti->Index() + 1, 
		                                                                        ti->File(), 
											tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
											tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
											vps ? (tVps.tm_mday):(tStart.tm_mday), 
											vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
											tEnd.tm_hour * 100 + tEnd.tm_min,
											vps ? "VPS":"-"
											);
		  } else {
		    if (ti->Recording()) {
		      isyslog("TVTV: timer %d recording, will be deleted after recording has ended (%s)", ti->Index() + 1, ti->File());
		    } else {
		      Timers.Del(ti);
		    }
		    Timers.Add(oTimer);
		    isyslog("TVTV: timer %d updated (%s) [%s/%s/%d/%04d-%04d/%s]", oTimer->Index() + 1, 
		                                                                oTimer->File(), 
										tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
										tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
										vps ? (tVps.tm_mday):(tStart.tm_mday), 
										vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
										tEnd.tm_hour * 100 + tEnd.tm_min,
										vps ? "VPS":"-"
										);
		  }
		} else if (tvtv_timer[DEF_TVTV_SCHEDULE_ACT] == "delete") {
		  if (ti->Recording()) {
		    isyslog("TVTV: timer %d recording, will be deleted after recording has ended (%s)", ti->Index() + 1, ti->File());
		  } else {
		    isyslog("TVTV: timer %d deleted [%s/%s/%d/%04d-%04d/%s]", oTimer->Index() + 1, 
		                                                           tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
									   tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
									   vps ? (tVps.tm_mday):(tStart.tm_mday), 
									   vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
									   tEnd.tm_hour * 100 + tEnd.tm_min,
									   vps ? "VPS":"-");
		    Timers.Del(ti);
		  }
		} 
	      } else { // if (timer_update)
		if (tvtv_timer[DEF_TVTV_SCHEDULE_ACT] == "rec") {

		  if (((tEndTime - Setup.MarginStop * 60) < tCurrentTime) && (vps == false)) {
		    // Do not add timer entry in the past, if vps is not active
		    isyslog("TVTV: timer NOT added (EndTime in the past) (%s) [%s/%s/%d/%04d-%04d/%s]", 
		                                                            oTimer->File(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
									    vps ? (tVps.tm_mday):(tStart.tm_mday), 
									    vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
									    tEnd.tm_hour * 100 + tEnd.tm_min,
									    vps ? "VPS":"-");
		  } else { // if (tEndTime < tCurrentTime)

		    if (((tStartTime + Setup.MarginStart * 60) < tCurrentTime) && (vps == false) && (TVTVConfig.AddOngoingNonVpsTimers == 0)) {
  		      // Do not add timer entry with start time in the past, if vps is not active
		      isyslog("TVTV: timer NOT added (StartTime in the past & AddOngoingNonVpsTimers=off) (%s) [%s/%s/%d/%04d-%04d/%s]", 
		                                                            oTimer->File(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
									    vps ? (tVps.tm_mday):(tStart.tm_mday), 
									    vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
									    tEnd.tm_hour * 100 + tEnd.tm_min,
									    vps ? "VPS":"-");
                    } else {
		      Timers.Add(oTimer);

		      isyslog("TVTV: timer %d added (%s) [%s/%s/%d/%04d-%04d/%s]", oTimer->Index() + 1, 
		                                                            oTimer->File(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_UID].c_str(), 
									    tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), 
									    vps ? (tVps.tm_mday):(tStart.tm_mday), 
									    vps ? (tVps.tm_hour * 100 + tVps.tm_min):(tStart.tm_hour * 100 + tStart.tm_min), 
									    tEnd.tm_hour * 100 + tEnd.tm_min,
									    vps ? "VPS":"-");
		      if (((tStartTime + Setup.MarginStart * 60) < tCurrentTime) && (vps == false)) {
		        isyslog("TVTV: timer %d notice: StartTime is behind CurrentTime (AddOngoingNonVpsTimers=on)", oTimer->Index() + 1);
                      };

		    }
		  } // if (tEndTime < tCurrentTime)
		}
	      }
		  
	    }
	  } else {
	    isyslog("TVTV: Timer Error: %s", s.c_str());
	  }

	} else { // if (oTVTVChannel)
	  isyslog("TVTV: ChannelID <%s> for Channel <%s> was not found in channels.conf!", (const char *)vdrch.ToString(), tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str());
	}

      } else { // if (!(vdrch == tChannelID::InvalidID))
	isyslog("TVTV: Channel <%s (ID: %d)> is not found!", tvtv_timer[DEF_TVTV_SCHEDULE_CHN].c_str(), atoi(tvtv_timer[DEF_TVTV_SCHEDULE_CHID].c_str()));
      }

      // read next line
      sLine = read_line_from_buffer(sBuffer, &p);
      tvtv_timer.clear();

    } // while (!sLine->empty())

    Timers.Save();
}

