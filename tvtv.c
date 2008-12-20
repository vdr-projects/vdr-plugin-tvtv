/*
 * tvtv.c: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "i18n.h"
#include "config.h"
#include "update.h"


static const char *VERSION        = "0.3.3";
static const char *DESCRIPTION    = "TVTV Timers update";
static const char *MAINMENUENTRY  = "TVTV";

cUpdate *oUpdate;

class cPluginTVTV : public cPlugin {
private:
#if VDRVERSNUM >= 10307
  cSkinDisplayMessage *displayMessage;
#endif
public:
  cPluginTVTV(void);
  virtual ~cPluginTVTV();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
#if VDRVERSNUM >= 10331
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Cmd, const char *Option, int &ReplyCode);
#endif
  virtual bool Start(void);
  virtual const char *MainMenuEntry(void) { return (TVTVConfig.show_in_mainmenu ? MAINMENUENTRY : NULL); }
  virtual cOsdMenu *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);

  void DisplayMessage(const char *s);
  };

// --- cMenuSetupTVTV -------------------------------------------------------

class cMenuSetupTVTV : public cMenuSetupPage {
private:
  cTVTVConfig data; 
  virtual void Setup(void);
protected:
  virtual eOSState ProcessKey(eKeys Key);
  virtual void Store(void);
public:
  cMenuSetupTVTV(void);
  };

cMenuSetupTVTV::cMenuSetupTVTV(void) {
  data = TVTVConfig;
  Setup();
}

void cMenuSetupTVTV::Setup(void) {
  int current = Current();
  
  Clear();

  Add(new cMenuEditStraItem(  tr("TVTV Server"), &data.tvtv_server, TVTVSRV_CNT, TVTV_SERVERS));
  Add(new cMenuEditStrItem(   tr("TVTV User Name"), data.username, sizeof(data.username), tr(FileNameChars)));
  Add(new cMenuEditStrItem(   tr("TVTV Password"), data.password, sizeof(data.password), tr(FileNameChars)));
  Add(new cMenuEditBoolItem(  tr("Use HTTP Proxy"), &data.useproxy));
  if (data.useproxy)
    Add(new cMenuEditStrItem( tr("  HTTP Proxy"), data.httpproxy, sizeof(data.httpproxy), tr(FileNameChars)));
  Add(new cMenuEditBoolItem(  tr("Auto Update"), &data.autoupdate));
  Add(new cMenuEditIntItem(   tr("Updatetime (min)"), &data.updatetime));
  Add(new cMenuEditBoolItem(  tr("Show In Main Menu"), &data.show_in_mainmenu));

// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
  Add(new cMenuEditBoolItem(  tr("Use VPS"), &data.usevps));
#endif
  
  Add(new cMenuEditBoolItem(  tr("Use Nature within File Name"), &data.usegenre));
  Add(new cMenuEditBoolItem(  tr("Use Station Name within File Name"), &data.usestation));

// Starting with VDR 1.3.44, the description of a recording is 
// taken from EPG. This switch decides whether or not the description
// coming from TVTV will be added as a comment to info.vdr.
#if VDRVERSNUM >= 10344
  Add(new cMenuEditBoolItem(  tr("Use TVTV description in info.vdr"), &data.usetvtvdescr));
#endif

  Add(new cMenuEditBoolItem(  tr("Time shift bugfix"), &data.tvtv_bugfix));
  if (data.tvtv_bugfix)
    Add(new cMenuEditIntItem( tr("  Time shift (hrs)"), &data.tvtv_bugfix_hrs, 1, 4));

  Add(new cOsdItem(tr("Reload ChannelMap"),osUser9));

  SetCurrent(Get(current));
  Display();
}


eOSState cMenuSetupTVTV::ProcessKey(eKeys Key) {
    int olduseproxy = data.useproxy;
    int oldtvtvbugfix = data.tvtv_bugfix;
    eOSState state = cMenuSetupPage::ProcessKey(Key);

    switch(state) {
      case osUser9:
        state=osContinue;
        if(Interface->Confirm(tr("Really reload ChannelMap?"))) {
	  if (oUpdate) oUpdate->ReloadChannelMap();
	  state=osEnd;
	}
        break;

      case osContinue:
        if(NORMALKEY(Key)==kUp || NORMALKEY(Key)==kDown) {
          cOsdItem *item=Get(Current());
          if(item) item->ProcessKey(kNone);
        }
      break;
					
      default:
      break;
    }

    if (Key != kNone && ((data.useproxy != olduseproxy) || (data.tvtv_bugfix != oldtvtvbugfix))) Setup();
    return state;
}


void cMenuSetupTVTV::Store(void)
{
  dsyslog("TVTV: cMenuSetupTVTV::Store()");
  TVTVConfig = data;
  SetupStore("TVTVServer", TVTVConfig.tvtv_server);
  SetupStore("UserName", TVTVConfig.username);
  SetupStore("Password", TVTVConfig.password);
  SetupStore("UseProxy", TVTVConfig.useproxy);
  SetupStore("HTTPProxy", TVTVConfig.httpproxy);
  SetupStore("AutoUpdate", TVTVConfig.autoupdate);
  SetupStore("UpdateTime", TVTVConfig.updatetime);
  SetupStore("UseGenreInFName", TVTVConfig.usegenre);
  SetupStore("UseStationInFName", TVTVConfig.usestation);
  SetupStore("ShowInMainMenu", TVTVConfig.show_in_mainmenu);

// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
  SetupStore("UseVps", TVTVConfig.usevps);
#endif	        

#if VDRVERSNUM >= 10344
  SetupStore("UseTVTVDescr", TVTVConfig.usetvtvdescr);
#endif

  SetupStore("TVTVBugfix", TVTVConfig.tvtv_bugfix);
  SetupStore("TVTVBugfixHrs", TVTVConfig.tvtv_bugfix_hrs);

  if (TVTVConfig.autoupdate)
    if (oUpdate)
	oUpdate->StartUpdate();
}

// --- cPluginTVTV ----------------------------------------------------------

cPluginTVTV::cPluginTVTV(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    oUpdate = NULL;
}

cPluginTVTV::~cPluginTVTV()
{
  // Clean up after yourself!
    if (oUpdate)
	delete oUpdate;
}


void cPluginTVTV::DisplayMessage(const char *s) {

  isyslog("TVTV: %s", s);

#if VDRVERSNUM >= 10307
  if (!Skins.Current()) return;
  if (!cSkinDisplay::Current() && !displayMessage)
     displayMessage = Skins.Current()->DisplayMessage();
  cSkinDisplay::Current()->SetMessage(mtWarning, s);
  cSkinDisplay::Current()->Flush();
  cStatus::MsgOsdStatusMessage(s);
  Interface->Wait(0);
  if (displayMessage) {
     delete displayMessage;
     displayMessage = NULL;
     cStatus::MsgOsdClear();
  }
  else {
    cSkinDisplay::Current()->SetMessage(mtWarning, NULL);
    cStatus::MsgOsdStatusMessage(NULL);
  }
#else
  Interface->Open(Setup.OSDwidth, -1);
  Interface->Status(s, clrBlack, clrYellow);

  // Wait some seconds
  time_t timeout = time(NULL) + Setup.OSDMessageTime;
  for (;;) { if (time(NULL) > timeout) break; }

  Interface->Status(NULL);
  Interface->Close();
#endif
}


const char *cPluginTVTV::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL; 
}

#if VDRVERSNUM >= 10331
const char **cPluginTVTV::SVDRPHelpPages(void) {
  static const char *HelpPages[] = {
      "RELOAD\n"
      "    Reloads the channel map configuration file.",
      "UPDATE\n"
      "    Update TVTV timer list.",
      NULL
      };
  return HelpPages;
}

cString cPluginTVTV::SVDRPCommand(const char *Cmd, const char *Option, int &ReplyCode) {
  if (strcasecmp(Cmd, "RELOAD") == 0) {
    oUpdate->ReloadChannelMap();
    return "Channel map reloaded.";
  }
  else if (strcasecmp(Cmd, "UPDATE") == 0) {
    oUpdate->StartUpdate();
    return "TVTV update done.";
  }
  return NULL;
}
#endif

bool cPluginTVTV::Start(void)
{
    // Start any background activities the plugin shall perform.
    RegisterI18n(Phrases);
    
    oUpdate = new cUpdate();
    
    if (oUpdate && TVTVConfig.autoupdate)
	oUpdate->StartUpdate();
    
    return (oUpdate != NULL);
}

cOsdMenu *cPluginTVTV::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  if (oUpdate) oUpdate->StartUpdate();
  DisplayMessage(tr("TVTV update will be started"));
  return NULL;
}


cMenuSetupPage *cPluginTVTV::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cMenuSetupTVTV;
}

bool cPluginTVTV::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.

  if      (!strcasecmp(Name, "TVTVServer"))  TVTVConfig.tvtv_server = atoi(Value);
  else if (!strcasecmp(Name, "UserName"))    strcpy(TVTVConfig.username, Value);
  else if (!strcasecmp(Name, "Password"))    strcpy(TVTVConfig.password, Value);
  else if (!strcasecmp(Name, "UseProxy"))    TVTVConfig.useproxy = atoi(Value);
  else if (!strcasecmp(Name, "HTTPProxy"))   strcpy(TVTVConfig.httpproxy, Value);
  else if (!strcasecmp(Name, "AutoUpdate"))  TVTVConfig.autoupdate = atoi(Value);
  else if (!strcasecmp(Name, "UpdateTime"))  TVTVConfig.updatetime = atoi(Value);
  else if (!strcasecmp(Name, "UseGenreInFName")) TVTVConfig.usegenre = atoi(Value);
  else if (!strcasecmp(Name, "UseStationInFName")) TVTVConfig.usestation = atoi(Value);
  else if (!strcasecmp(Name, "ShowInMainMenu")) TVTVConfig.show_in_mainmenu = atoi(Value);
// VPS was introduced with VDR 1.3.5
#if VDRVERSNUM >= 10305
  else if (!strcasecmp(Name, "UseVps"))         TVTVConfig.usevps = atoi(Value);
#endif
  
#if VDRVERSNUM >= 10344
  else if (!strcasecmp(Name, "UseTVTVDescr"))   TVTVConfig.usetvtvdescr = atoi(Value);
#endif

  else if (!strcasecmp(Name, "TVTVBugfix"))  TVTVConfig.tvtv_bugfix = atoi(Value);
  else if (!strcasecmp(Name, "TVTVBugfixHrs"))  TVTVConfig.tvtv_bugfix_hrs = atoi(Value);

  else
     return false;
  return true;
}

VDRPLUGINCREATOR(cPluginTVTV); // Don't touch this!

