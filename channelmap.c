/*
 * channelmap.c: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "channelmap.h"


cChannelMap::cChannelMap() {
  chanmap.clear();
  read_config_file();
}

cChannelMap::~cChannelMap() {
  chanmap.clear();
}


void cChannelMap::remove_whitespaces(char *s)  // Whitespaces werden entfernt
{
  unsigned int i=0;
  char *dum;
  if (s != NULL) {
    dum=(char *)calloc(1,strlen(s)+1);
    for (i=0;i<strlen(s);i++) if (!isspace(s[i])) strncat(dum,&(s[i]),1);
    memset(s,0,strlen(s));
    strcpy(s,dum);
    free(dum);
  }
}


int cChannelMap::read_config_file() {
  ifstream cmfile;
  string s;
  size_t p;
  int tvtvid;
  int n;
  char *cfg_fname = NULL;

  asprintf(&cfg_fname, "%s/tvtv_channelmap.conf", cPlugin::ConfigDirectory());
  cmfile.open(cfg_fname);
  if (!cmfile) {
    esyslog("TVTV: Error reading '%s'!", cfg_fname);
    return -1;
  }
  isyslog("TVTV: Loading '%s'", cfg_fname);
  n=0;
  while (!cmfile.eof()) {
    getline(cmfile, s);

    if (!s.empty()) {
      remove_whitespaces((char *)s.c_str());

      // remove comments
      p=s.find_first_of("//");
      if (p != string::npos) s.erase(p);

      // split line
      p=s.find_first_of("=");
      if (p != string::npos) {
        tvtvid=atoi(s.substr(0,p).c_str());
	chanmap[tvtvid] = (char *)calloc(s.substr(p+1).length()+1, sizeof(char));
	strcpy(chanmap[tvtvid], s.substr(p+1).c_str());
	n++;
      }
    }
  }
  cmfile.close();
  isyslog("TVTV: %d channel mappings read.", n);
  return n;
}

char *cChannelMap::GetChanStr(int tvtvid) {
  return chanmap[tvtvid];
}

tChannelID cChannelMap::GetChanID(int tvtvid) {
  if (chanmap[tvtvid] == NULL) return tChannelID::InvalidID;
  return tChannelID::FromString(chanmap[tvtvid]);
}


int cChannelMap::ReloadChannelMap() {
  chanmap.clear();
  return read_config_file();
}
