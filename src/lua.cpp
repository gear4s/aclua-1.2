#include "cube.h"
#include "server.h"

extern "C" {
  #include "lua.h"
  #include "lauxlib.h"
  #include "lualib.h"
  #include "lanes/src/threading.h"
}

#include "module.hpp"
#include "luamod.h"
#include "lua_functions.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h> /// http://softagalleria.net/dirent.php
#include <string>
#include <signal.h>

#define ALL_SCRIPTS for ( int _i = 0; _i < Lua::numberOfScripts; _i++ )
#define SCRIPT Lua::scripts[_i]

static const char *extensions[] = { ".lua", ".luac" };

const double Lua::version = 2.0;
lua_State **Lua::scripts = NULL;
int Lua::numberOfScripts = 0;
void Lua_atExit(int);

extern "C" int luaopen_lanes_core(lua_State *L);
extern "C" int luaG_inter_copy(lua_State*, lua_State*, uint n);
extern "C" int luaG_inter_copy_from(lua_State*, lua_State*, uint n, uint from);

/// from serverfiles.h {
#define SERVERMAP_PATH          "packages/maps/servermaps/"
#define SERVERMAP_PATH_BUILTIN  "packages/maps/official/"
#define SERVERMAP_PATH_INCOMING "packages/maps/servermaps/incoming/"
enum { MAP_NOTFOUND = 0, MAP_TEMP, MAP_CUSTOM, MAP_LOCAL, MAP_OFFICIAL, MAP_VOID };
#define readonlymap(x) ((x) >= MAP_CUSTOM)
/// }

extern void serverdamage(client*, client*, int, int, bool, const vec&, bool fromLua);
extern void sendserveropinfo(int);
extern void sdropflag(int);
extern servercommandline scl;
extern void sendresume(client&, bool);
extern void sendinitclient(client&);
extern void shuffleteams(int = FTR_AUTOTEAM);
extern int sendservermode(bool = true);
extern bool checkgban(enet_uint32);
extern void addgban(const char*);
extern void cleargbans();
extern void updatemasterserver(int, int, int, bool fromLua);
extern void checkintermission();
extern void sendspawn(client*, bool fromLua);
extern bool mapisok(mapstats*);
extern void flagaction(int, int, int, bool fromLua);
extern void changemastermode(int);
extern client& addclient();
extern void sendteamtext(char*, int, int);
extern void sendvoicecomteam(int, int);
extern void rereadcfgs(bool fromLua = true);
extern int clienthasflag(int);

struct pwddetail {
  string pwd;
  int line;
  bool denyadmin;
};
struct serverpasswords: serverconfigfile {
  vector <pwddetail> adminpwds;
  int staticpasses;
};
struct servernickblacklist: serverconfigfile {};
struct configset {
  string mapname;
  int mode, time, vote, minplayer, maxplayer, skiplines;
};
struct servermaprot: serverconfigfile {
  vector <configset> configsets;
  virtual int get_next(bool fromLua = true);
  virtual configset* get(int);
};
struct serveripblacklist: serverconfigfile {
  vector <iprange> ipranges;
};
struct voteinfo {};

extern int intermission_check_shear;
extern void checkintermission();
extern int masterservers;
extern unsigned long long int Lua_totalSent;
extern unsigned long long int Lua_totalReceived;
/**
 * Неприятный баг на уровне конструкций языка ассемблера вынуждает к этому действию.
 * (Неожиданная подмена адреса таблицы виртуальных функций для объектов дочерних классов класса serveraction.)
 **/
extern void Lua_evaluateVote(bool = false);
extern void Lua_endVote(int result);

extern voteinfo *curvote;
extern int server_protocol_version;
extern struct sflaginfo { int state, actor_cn; } sflaginfos[2];
extern vector<client*> clients;
extern int gamemillis, servmillis, gamelimit;
extern string servdesc_current;
extern char *global_name;
extern serverpasswords passwords;
extern serveripblacklist ipblacklist;
extern servernickblacklist nickblacklist;
extern servermaprot maprot;
extern int smode;
extern int nextgamemode;
extern string nextmapname;
extern bool items_blocked, autoteam;
extern int arenaround, gamemillis;
extern bool forceintermission;
extern int mastermode;
extern bool autoteam;
extern vector <ban> bans;
extern guninfo guns[NUMGUNS];
extern itemstat ammostats[NUMGUNS];
extern vector<server_entity> sents;
extern ENetHost *serverhost;

struct IteratorData { // для cla(), rvsf(), players()
  int player_cn;
  bool alive_only;
};

static MUTEX_T callHandlerMutex;
static void as_player(int player_cn, const char *format, ...) {
  int exclude_cn = -1;
  packetbuf packet(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), buffer(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
  putint(packet, SV_CLIENT);
  putint(packet, player_cn);

  va_list args;
  va_start(args, format);
  while(*format) switch(*format++) {
    case 'x': {
        exclude_cn = va_arg(args, int);
        break;
      }
    case 'i': {
        int n = isdigit(*format) ? *format++ - '0' : 1;
        loopi(n) putint(buffer, va_arg(args, int));
        //putint( buffer, va_arg( args, int ) );
        break;
      }
    case 's': {
        sendstring(va_arg(args, const char*), buffer);
        break;
      }
    }
  va_end(args);

  putuint(packet, buffer.length());
  packet.put(buffer.buf, buffer.length());
  sendpacket(-1, 1, packet.finalize(), exclude_cn);
}



void Lua::initialize(const char *scriptsPath) {
  printf("<Lua> Initializing framework...\n");

  MUTEX_RECURSIVE_INIT(&callHandlerMutex);

  DIR *dp = opendir(scriptsPath);
  if(dp) {
    dirent *entry;
    while((entry = readdir(dp)) != NULL) {
      bool match = false;
      for(unsigned i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
        if((match = !strcasecmp(entry->d_name + (strlen(entry->d_name) - strlen(extensions[i])), extensions[i]))) break;
      if(match) {
        scripts = (lua_State**) realloc(scripts, (++numberOfScripts) * sizeof(lua_State*));
        scripts[ numberOfScripts-1 ] = luaL_newstate();
        lua_State *L = scripts[ numberOfScripts-1 ];
        luaL_openlibs(L);
        // Lua mod appendix {
        openExtraLibs(L);
        registerGlobals(L);
        lua::module::openAll(L);
        // }
        string scriptFilename = { '\0' };
        copystring(scriptFilename, scriptsPath);
        strcat(scriptFilename, entry->d_name);
        if(luaL_dofile(L, scriptFilename)) {
          printf("<Lua> Could not load %s:\n%s\n", entry->d_name, lua_tostring(L, lua_gettop(L)));
          lua_close(L);
          scripts = (lua_State**) realloc(scripts, (--numberOfScripts) * sizeof(lua_State*));
        } else {
          printf("<Lua> Script loaded: %s\n", entry->d_name);
          lua_getglobal(L, LUA_ON_INIT);
          if(lua_isfunction(L, lua_gettop(L))) {
            if(lua_pcall(L, 0, 0, 0)) {
              printf("<Lua> Error when calling %s handler:\n%s\n", LUA_ON_INIT, lua_tostring(L, lua_gettop(L)));
              lua_pop(L, 1);
            }
          } else lua_pop(L, 1);
          lua_getglobal(L, "PLUGIN_NAME");
          printf("<Lua> -- Name: %s\n", lua_tostring(L, lua_gettop(L)));
          lua_getglobal(L, "PLUGIN_AUTHOR");
          printf("<Lua> -- Author: %s\n", lua_tostring(L, lua_gettop(L)));
          lua_getglobal(L, "PLUGIN_VERSION");
          printf("<Lua> -- Version: %s\n", lua_tostring(L, lua_gettop(L)));
          lua_pop(L, 3);
        }
      }
    }
    signal(SIGINT, Lua_atExit);
  } else printf("<Lua> Could not enter scripts' directory\n");
  printf("<Lua> Framework initialized, %d scripts loaded\n", numberOfScripts);
}

void Lua::destroy() {
  printf("<Lua> Destroying framework...\n");
  if(numberOfScripts) {
    callHandler(LUA_ON_DESTROY, "");
    tmr_free();
    ALL_SCRIPTS {
      lua_gc(SCRIPT, LUA_GCCOLLECT, 0);
      lua_close(SCRIPT);
    }
    numberOfScripts = 0;
    free(scripts);
    scripts = NULL;
  }

  MUTEX_FREE(&callHandlerMutex);

  printf("<Lua> Framework destroyed\n");
}

void Lua::openExtraLibs(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  #define LUA_OPEN_EXTRA_LIB(opener, libName) \
    lua_pushcfunction( L, opener ); \
    lua_pushstring( L, libName ); \
    lua_call( L, 1, 1 ); \
    lua_setfield( L, 1, libName )

  LUA_OPEN_EXTRA_LIB(luaopen_tmr, "tmr");
  LUA_OPEN_EXTRA_LIB(luaopen_cfg, "cfg");
  LUA_OPEN_EXTRA_LIB(luaopen_lanes_core, "lanes.core");

  lua_pop(L, 1);   // _LOADED
}

void Lua::registerGlobals(lua_State *L) {
  luaL_newmetatable(L, "IteratorData");
  /*
  lua_pushstring( L, "__gc" );
  lua_pushcfunction( L, __IteratorData_gc );
  lua_settable( L, -3 );
  */
  lua_pop(L, 1);

  lua_pushnumber(L, version);
  lua_setglobal(L, "LUA_API_VERSION");

  #define LUA_SET_CONSTANTN(name, constant) lua_pushinteger( L, constant ); lua_setglobal( L, name )
  #define LUA_SET_CONSTANT(constant) LUA_SET_CONSTANTN(#constant, constant)
  LUA_SET_CONSTANT(PLUGIN_BLOCK);
  LUA_SET_CONSTANTN("DAMAGE_MAX", INT_MAX);
  // client roles
  LUA_SET_CONSTANT(CR_ADMIN);
  LUA_SET_CONSTANT(CR_DEFAULT);
  // client states
  LUA_SET_CONSTANT(CS_ALIVE);
  LUA_SET_CONSTANT(CS_DEAD);
  LUA_SET_CONSTANT(CS_SPAWNING);
  LUA_SET_CONSTANT(CS_LAGGED);
  LUA_SET_CONSTANT(CS_EDITING);
  LUA_SET_CONSTANT(CS_SPECTATE);
  // weapons
  LUA_SET_CONSTANT(GUN_KNIFE);
  LUA_SET_CONSTANT(GUN_PISTOL);
  LUA_SET_CONSTANT(GUN_CARBINE);
  LUA_SET_CONSTANT(GUN_SHOTGUN);
  LUA_SET_CONSTANT(GUN_SUBGUN);
  LUA_SET_CONSTANT(GUN_SNIPER);
  LUA_SET_CONSTANT(GUN_ASSAULT);
  LUA_SET_CONSTANT(GUN_CPISTOL);
  LUA_SET_CONSTANT(GUN_GRENADE);
  LUA_SET_CONSTANT(GUN_AKIMBO);
  // game modes
  LUA_SET_CONSTANTN("GM_DEMO", GMODE_DEMO);
  LUA_SET_CONSTANTN("GM_TDM", GMODE_TEAMDEATHMATCH);
  LUA_SET_CONSTANTN("GM_COOP", GMODE_COOPEDIT);
  LUA_SET_CONSTANTN("GM_DM", GMODE_DEATHMATCH);
  LUA_SET_CONSTANTN("GM_SURV", GMODE_SURVIVOR);
  LUA_SET_CONSTANTN("GM_TSURV", GMODE_TEAMSURVIVOR);
  LUA_SET_CONSTANTN("GM_CTF", GMODE_CTF);
  LUA_SET_CONSTANTN("GM_PF", GMODE_PISTOLFRENZY);
  LUA_SET_CONSTANTN("GM_LSS", GMODE_LASTSWISSSTANDING);
  LUA_SET_CONSTANTN("GM_OSOK", GMODE_ONESHOTONEKILL);
  LUA_SET_CONSTANTN("GM_TOSOK", GMODE_TEAMONESHOTONEKILL);
  LUA_SET_CONSTANTN("GM_HTF", GMODE_HUNTTHEFLAG);
  LUA_SET_CONSTANTN("GM_TKTF", GMODE_TEAMKEEPTHEFLAG);
  LUA_SET_CONSTANTN("GM_KTF", GMODE_KEEPTHEFLAG);
  LUA_SET_CONSTANTN("GM_NUM", GMODE_NUM);
  // disconnect reasons
  LUA_SET_CONSTANT(DISC_NONE);
  LUA_SET_CONSTANT(DISC_EOP);
  LUA_SET_CONSTANT(DISC_CN);
  LUA_SET_CONSTANT(DISC_MKICK);
  LUA_SET_CONSTANT(DISC_MBAN);
  LUA_SET_CONSTANT(DISC_TAGT);
  LUA_SET_CONSTANT(DISC_BANREFUSE);
  LUA_SET_CONSTANT(DISC_WRONGPW);
  LUA_SET_CONSTANT(DISC_SOPLOGINFAIL);
  LUA_SET_CONSTANT(DISC_MAXCLIENTS);
  LUA_SET_CONSTANT(DISC_MASTERMODE);
  LUA_SET_CONSTANT(DISC_AUTOKICK);
  LUA_SET_CONSTANT(DISC_AUTOBAN);
  LUA_SET_CONSTANT(DISC_DUP);
  LUA_SET_CONSTANT(DISC_BADNICK);
  LUA_SET_CONSTANT(DISC_OVERFLOW);
  LUA_SET_CONSTANT(DISC_ABUSE);
  LUA_SET_CONSTANT(DISC_AFK);
  LUA_SET_CONSTANT(DISC_FFIRE);
  LUA_SET_CONSTANT(DISC_CHEAT);
  // flag actions
  LUA_SET_CONSTANT(FA_PICKUP);
  LUA_SET_CONSTANT(FA_STEAL);
  LUA_SET_CONSTANT(FA_DROP);
  LUA_SET_CONSTANT(FA_LOST);
  LUA_SET_CONSTANT(FA_RETURN);
  LUA_SET_CONSTANT(FA_SCORE);
  LUA_SET_CONSTANT(FA_RESET);
  // vote actions
  LUA_SET_CONSTANT(VOTE_NEUTRAL);
  LUA_SET_CONSTANT(VOTE_YES);
  LUA_SET_CONSTANT(VOTE_NO);
  // vote types
  LUA_SET_CONSTANT(SA_KICK);
  LUA_SET_CONSTANT(SA_BAN);
  LUA_SET_CONSTANT(SA_REMBANS);
  LUA_SET_CONSTANT(SA_MASTERMODE);
  LUA_SET_CONSTANT(SA_AUTOTEAM);
  LUA_SET_CONSTANT(SA_FORCETEAM);
  LUA_SET_CONSTANT(SA_GIVEADMIN);
  LUA_SET_CONSTANT(SA_MAP);
  LUA_SET_CONSTANT(SA_RECORDDEMO);
  LUA_SET_CONSTANT(SA_STOPDEMO);
  LUA_SET_CONSTANT(SA_CLEARDEMOS);
  LUA_SET_CONSTANT(SA_SERVERDESC);
  LUA_SET_CONSTANT(SA_SHUFFLETEAMS);
  // master modes
  LUA_SET_CONSTANT(MM_OPEN);
  LUA_SET_CONSTANT(MM_PRIVATE);
  LUA_SET_CONSTANT(MM_MATCH);
  // teams
  LUA_SET_CONSTANT(TEAM_CLA);
  LUA_SET_CONSTANT(TEAM_RVSF);
  LUA_SET_CONSTANT(TEAM_CLA_SPECT);
  LUA_SET_CONSTANT(TEAM_RVSF_SPECT);
  LUA_SET_CONSTANT(TEAM_SPECT);
  // entity types
  LUA_SET_CONSTANT(NOTUSED);
  LUA_SET_CONSTANT(LIGHT);
  LUA_SET_CONSTANT(PLAYERSTART);
  LUA_SET_CONSTANT(I_CLIPS);
  LUA_SET_CONSTANT(I_AMMO);
  LUA_SET_CONSTANT(I_GRENADE);
  LUA_SET_CONSTANT(I_HEALTH);
  LUA_SET_CONSTANT(I_HELMET);
  LUA_SET_CONSTANT(I_ARMOUR);
  LUA_SET_CONSTANT(I_AKIMBO);
  LUA_SET_CONSTANT(MAPMODEL);
  LUA_SET_CONSTANT(CARROT);
  LUA_SET_CONSTANT(LADDER);
  LUA_SET_CONSTANT(CTF_FLAG);
  LUA_SET_CONSTANT(SOUND);
  LUA_SET_CONSTANT(CLIP);
  LUA_SET_CONSTANT(PLCLIP);
  // sounds
  LUA_SET_CONSTANT(S_AFFIRMATIVE);
  LUA_SET_CONSTANT(S_ALLRIGHTSIR);
  LUA_SET_CONSTANT(S_COMEONMOVE);
  LUA_SET_CONSTANT(S_COMINGINWITHTHEFLAG);
  LUA_SET_CONSTANT(S_COVERME);
  LUA_SET_CONSTANT(S_DEFENDTHEFLAG);
  LUA_SET_CONSTANT(S_ENEMYDOWN);
  LUA_SET_CONSTANT(S_GOGETEMBOYS);
  LUA_SET_CONSTANT(S_GOODJOBTEAM);
  LUA_SET_CONSTANT(S_IGOTONE);
  LUA_SET_CONSTANT(S_IMADECONTACT);
  LUA_SET_CONSTANT(S_IMATTACKING);
  LUA_SET_CONSTANT(S_IMONDEFENSE);
  LUA_SET_CONSTANT(S_IMONYOURTEAMMAN);
  LUA_SET_CONSTANT(S_NEGATIVE);
  LUA_SET_CONSTANT(S_NOCANDO);
  LUA_SET_CONSTANT(S_RECOVERTHEFLAG);
  LUA_SET_CONSTANT(S_SORRY);
  LUA_SET_CONSTANT(S_SPREADOUT);
  LUA_SET_CONSTANT(S_STAYHERE);
  LUA_SET_CONSTANT(S_STAYTOGETHER);
  LUA_SET_CONSTANT(S_THERESNOWAYSIR);
  LUA_SET_CONSTANT(S_WEDIDIT);
  LUA_SET_CONSTANT(S_YES);
  LUA_SET_CONSTANT(S_ONTHEMOVE1);
  LUA_SET_CONSTANT(S_ONTHEMOVE2);
  LUA_SET_CONSTANT(S_GOTURBACK);
  LUA_SET_CONSTANT(S_GOTUCOVERED);
  LUA_SET_CONSTANT(S_INPOSITION1);
  LUA_SET_CONSTANT(S_INPOSITION2);
  LUA_SET_CONSTANT(S_REPORTIN);
  LUA_SET_CONSTANT(S_NICESHOT);
  LUA_SET_CONSTANT(S_THANKS1);
  LUA_SET_CONSTANT(S_THANKS2);
  LUA_SET_CONSTANT(S_AWESOME1);
  LUA_SET_CONSTANT(S_AWESOME2);
  // socket types
  LUA_SET_CONSTANT(ST_EMPTY);
  LUA_SET_CONSTANT(ST_LOCAL);
  LUA_SET_CONSTANT(ST_TCPIP);
  // log levels
  LUA_SET_CONSTANT(ACLOG_DEBUG);
  LUA_SET_CONSTANT(ACLOG_VERBOSE);
  LUA_SET_CONSTANT(ACLOG_INFO);
  LUA_SET_CONSTANT(ACLOG_WARNING);
  LUA_SET_CONSTANT(ACLOG_ERROR);
  // vote errors
  LUA_SET_CONSTANTN("VOTEE_NOERROR", -1);
  LUA_SET_CONSTANT(VOTEE_DISABLED);
  LUA_SET_CONSTANT(VOTEE_CUR);
  LUA_SET_CONSTANT(VOTEE_MUL);
  LUA_SET_CONSTANT(VOTEE_MAX);
  LUA_SET_CONSTANT(VOTEE_AREA);
  LUA_SET_CONSTANT(VOTEE_PERMISSION);
  LUA_SET_CONSTANT(VOTEE_INVALID);
  LUA_SET_CONSTANT(VOTEE_WEAK);
  LUA_SET_CONSTANT(VOTEE_NEXT);
  // forceteam reasons
  LUA_SET_CONSTANT(FTR_INFO);
  LUA_SET_CONSTANT(FTR_PLAYERWISH);
  LUA_SET_CONSTANT(FTR_AUTOTEAM);
  LUA_SET_CONSTANT(FTR_SILENTFORCE);
  // upload map errors
  LUA_SET_CONSTANTN("UE_IGNORE", -2);
  LUA_SET_CONSTANT(UE_NOERROR);
  LUA_SET_CONSTANT(UE_READONLY);
  LUA_SET_CONSTANT(UE_LIMITACHIEVED);
  LUA_SET_CONSTANT(UE_NOUPLOADPERMISSION);
  LUA_SET_CONSTANT(UE_NOUPDATEPERMISSION);
  LUA_SET_CONSTANT(UE_NOREVERTPERMISSION);
  // remove map errors
  LUA_SET_CONSTANTN("RE_IGNORE", -2);
  LUA_SET_CONSTANT(RE_NOERROR);
  LUA_SET_CONSTANT(RE_NOPERMISSION);
  LUA_SET_CONSTANT(RE_READONLY);
  LUA_SET_CONSTANT(RE_NOTFOUND);

#define LUA_SET_FUNCTION_PREFIX(prefix, function) lua_register( L, #function, prefix##function )
#define LUA_SET_FUNCTION(function) LUA_SET_FUNCTION_PREFIX(__, function)
  LUA_SET_FUNCTION(include);
  LUA_SET_FUNCTION(clientprint);
  LUA_SET_FUNCTION(kill);
  LUA_SET_FUNCTION(forcedeath);
  LUA_SET_FUNCTION(spawn);
  LUA_SET_FUNCTION(dodamage);
  LUA_SET_FUNCTION(isconnected);
  LUA_SET_FUNCTION(isadmin);
  LUA_SET_FUNCTION(getpriv);
  LUA_SET_FUNCTION(getstate);
  LUA_SET_FUNCTION(getname);
  LUA_SET_FUNCTION(getip);
  LUA_SET_FUNCTION(getweapon);
  LUA_SET_FUNCTION(getpos);
  LUA_SET_FUNCTION(gethealth);
  LUA_SET_FUNCTION(getarmor);
  LUA_SET_FUNCTION(getfrags);
  LUA_SET_FUNCTION(getdeaths);
  LUA_SET_FUNCTION(getping);
  LUA_SET_FUNCTION(getteam);
  LUA_SET_FUNCTION(getflags);
  LUA_SET_FUNCTION(getammo);
  LUA_SET_FUNCTION(getnextprimary);
  LUA_SET_FUNCTION(getvelocity);
  LUA_SET_FUNCTION(getangle);
  LUA_SET_FUNCTION(getskin);
  LUA_SET_FUNCTION(setpos);
  LUA_SET_FUNCTION(setammo);
  LUA_SET_FUNCTION(setname);
  LUA_SET_FUNCTION(sethealth);
  LUA_SET_FUNCTION(setarmor);
  LUA_SET_FUNCTION(setweapon);
  LUA_SET_FUNCTION(setrole);
  LUA_SET_FUNCTION(setteam);
  LUA_SET_FUNCTION(setfrags);
  LUA_SET_FUNCTION(setflags);
  LUA_SET_FUNCTION(setdeaths);
  LUA_SET_FUNCTION(setskin);
  LUA_SET_FUNCTION(voteend);
  LUA_SET_FUNCTION(setservname);
  LUA_SET_FUNCTION(setmaxcl);
  LUA_SET_FUNCTION(setpwd);
  LUA_SET_FUNCTION(setadminpwd);
  LUA_SET_FUNCTION(setmotd);
  LUA_SET_FUNCTION(setblacklist);
  LUA_SET_FUNCTION(setnickblist);
  LUA_SET_FUNCTION(setmaprot);
  LUA_SET_FUNCTION(setkickthres);
  LUA_SET_FUNCTION(setbanthres);
  LUA_SET_FUNCTION(getmapname);
  LUA_SET_FUNCTION(gettimeleft);
  LUA_SET_FUNCTION(getgamemode);
  LUA_SET_FUNCTION(getsvtick);
  LUA_SET_FUNCTION(maxclient);
  LUA_SET_FUNCTION(changemap);
  LUA_SET_FUNCTION(flagaction);
  LUA_SET_FUNCTION(setnextmode);
  LUA_SET_FUNCTION(setnextmap);
  LUA_SET_FUNCTION(setmastermode);
  LUA_SET_FUNCTION(forcearenawin);
  LUA_SET_FUNCTION(forcemapend);
  LUA_SET_FUNCTION(getmastermode);
  LUA_SET_FUNCTION(settimeleft);
  LUA_SET_FUNCTION(getautoteam);
  LUA_SET_FUNCTION(setautoteam);
  LUA_SET_FUNCTION(disconnect);
  LUA_SET_FUNCTION(ban);
  LUA_SET_FUNCTION(setvelocity);
  LUA_SET_FUNCTION(getflagkeeper);
  LUA_SET_FUNCTION(getprimary);
  LUA_SET_FUNCTION(setprimary);
  LUA_SET_FUNCTION(getdefaultammo);
  LUA_SET_FUNCTION(getservermaps);
  LUA_SET_FUNCTION(giveitem);
  LUA_SET_FUNCTION(shuffleteams);
  LUA_SET_FUNCTION(setprotocol);
  LUA_SET_FUNCTION(getdefaultprotocol);
  LUA_SET_FUNCTION(getprotocol);
  LUA_SET_FUNCTION(luanotice);
  LUA_SET_FUNCTION(getnextmode);
  LUA_SET_FUNCTION(getnextmap);
  LUA_SET_FUNCTION(getmapitems);
  LUA_SET_FUNCTION(spawnitem);
  LUA_SET_FUNCTION(removebans);
  LUA_SET_FUNCTION(getscore);
  LUA_SET_FUNCTION(setscore);
  LUA_SET_FUNCTION(getadminpwd);
  LUA_SET_FUNCTION(removeadminpwd);
  LUA_SET_FUNCTION(setmasterserver);
  LUA_SET_FUNCTION(getmaprotnextmap);
  LUA_SET_FUNCTION(getmaprotnextmode);
  LUA_SET_FUNCTION(getadminpwds);
  LUA_SET_FUNCTION(getwholemaprot);
  LUA_SET_FUNCTION(setwholemaprot);
  LUA_SET_FUNCTION(getwholebl);
  LUA_SET_FUNCTION(setwholebl);
  LUA_SET_FUNCTION(strtoiprange);
  LUA_SET_FUNCTION(setsockettype);
  LUA_SET_FUNCTION(addclient);
  LUA_SET_FUNCTION(initclient);
  LUA_SET_FUNCTION(delclient);
  LUA_SET_FUNCTION(sethostname);
  LUA_SET_FUNCTION(sayas);
  LUA_SET_FUNCTION(voiceas);
  LUA_SET_FUNCTION(getgamemillis);
  LUA_SET_FUNCTION(getgamelimit);
  LUA_SET_FUNCTION(gettimeleftmillis);
  LUA_SET_FUNCTION(settimeleftmillis);
  LUA_SET_FUNCTION(setping);
  LUA_SET_FUNCTION(setangle);
  LUA_SET_FUNCTION(removeban);
  LUA_SET_FUNCTION(getbans);
  LUA_SET_FUNCTION(getmaprotnextentry);
  LUA_SET_FUNCTION(getspawnmillis);
  LUA_SET_FUNCTION(players);
  LUA_SET_FUNCTION(spectators);
  LUA_SET_FUNCTION(rvsf);
  LUA_SET_FUNCTION(cla);
  LUA_SET_FUNCTION(logline);
  LUA_SET_FUNCTION(setmaprotnextentry);
  LUA_SET_FUNCTION(setmaprotnextmap);
  LUA_SET_FUNCTION(setmaprotnextmode);
  LUA_SET_FUNCTION(setmaprotnexttimeleft);
  LUA_SET_FUNCTION(getreloadtime);
  LUA_SET_FUNCTION(getattackdelay);
  LUA_SET_FUNCTION(getmagsize);
  LUA_SET_FUNCTION(getsessionid);
  LUA_SET_FUNCTION(setscoping);
  LUA_SET_FUNCTION(setcrouching);
  LUA_SET_FUNCTION(shootas);
  LUA_SET_FUNCTION(reloadas);
  LUA_SET_FUNCTION(pickupas);
  LUA_SET_FUNCTION(setip);
  LUA_SET_FUNCTION(getmaprotnexttimeleft);
  LUA_SET_FUNCTION(removemap);
  LUA_SET_FUNCTION(mapisok);
  LUA_SET_FUNCTION(mapexists);
  LUA_SET_FUNCTION(isonfloor);
  LUA_SET_FUNCTION(isonladder);
  LUA_SET_FUNCTION(isscoping);
  LUA_SET_FUNCTION(iscrouching);
  LUA_SET_FUNCTION(voteas);
  LUA_SET_FUNCTION(getitempos);
  LUA_SET_FUNCTION(isitemspawned);
  LUA_SET_FUNCTION(setstate);
  LUA_SET_FUNCTION(sendspawn);
  LUA_SET_FUNCTION(clienthasflag);
  LUA_SET_FUNCTION(getmappath);
  LUA_SET_FUNCTION(getvote);
  LUA_SET_FUNCTION(getpushfactor);
  LUA_SET_FUNCTION(genpwdhash);
  LUA_SET_FUNCTION(getacversion);
  LUA_SET_FUNCTION(getacbuildtype);
  LUA_SET_FUNCTION(getconnectmillis);
  LUA_SET_FUNCTION(gettotalsent);
  LUA_SET_FUNCTION(gettotalreceived);
  LUA_SET_FUNCTION(cleargbans);
  LUA_SET_FUNCTION(addgban);
  LUA_SET_FUNCTION(checkgban);
  LUA_SET_FUNCTION(getteamkills);
  LUA_SET_FUNCTION(setteamkills);
  LUA_SET_FUNCTION(rereadcfgs);
  LUA_SET_FUNCTION(callhandler);
  LUA_SET_FUNCTION(isediting);
  LUA_SET_FUNCTION(flashonradar);
  LUA_SET_FUNCTION(isalive);
  LUA_SET_FUNCTION(getwaterlevel);
  LUA_SET_FUNCTION(isinwater);
  LUA_SET_FUNCTION(isunderwater);
  LUA_SET_FUNCTION(callgenerator);
}

static LuaArg* prepareArgs(const char *arguments, va_list vl, int &argc) {
  LuaArg *args = NULL; argc = 0;
  while(*arguments) {
    LuaArg arg;
    arg.type = *arguments;

    switch(*arguments++) {
    case 'i': { // integer
        arg.i = va_arg(vl, int);
      } break;
    case 's': { // string
        arg.ccp = va_arg(vl, const char*);
      } break;
    case 'b': { // boolean
        arg.b = va_arg(vl, int);
      } break;
    case 'r': { // real
        arg.d = va_arg(vl, double);
      } break;
    case 'o': { // object(s); the objects that are currently on the stack top of the script passed as an argument
        arg.lsp = va_arg(vl, lua_State*);
        arg.origin = va_arg(vl, int);
        arg.count = isdigit(*arguments) ? ((*arguments++) - '0') : 1;
      } break;
    case 'R': { // result [out]
        arg.bp = va_arg(vl, bool*);
      } break;
    case 't': { // table
        arg.luatbl = (*arguments++) - '0';
      } break;
    case 'S': { // sender [out]
        arg.lsp = va_arg(vl, lua_State*);
        arg.origin = va_arg(vl, int);
      } break;
    }

    args = (LuaArg*) realloc(args, (argc + 1) * sizeof(LuaArg));
    args[argc++] = arg;
  }
  return args;
}

static void parseArgs(int argc, const LuaArg *args, int &numberOfArguments, ...) {
  va_list vl; va_start(vl, numberOfArguments);
  numberOfArguments = 0;
  for(int i = 0; i < argc; ++i) {
    switch(args[i].type) {
    case 'i': {
        int integer = args[i].i;
        ALL_SCRIPTS lua_pushinteger(SCRIPT, integer);
        numberOfArguments++;
      } break;
    case 's': {
        const char *string = args[i].ccp;
        ALL_SCRIPTS lua_pushstring(SCRIPT, string);
        numberOfArguments++;
      } break;
    case 'b': {
        int boolean = args[i].b;
        ALL_SCRIPTS lua_pushboolean(SCRIPT, boolean);
        numberOfArguments++;
      } break;
    case 'r': {
        double real = args[i].d;
        ALL_SCRIPTS lua_pushnumber(SCRIPT, real);
        numberOfArguments++;
      } break;
    case 'o': { // the objects that are currently on the stack top of the script passed as an argument
        int n = args[i].count;
        if(n > 0) {
          lua_State *donor = args[i].lsp;
          int origin = args[i].origin;

          ALL_SCRIPTS {
            if(SCRIPT == donor)
              for(int i = 0; i < n; ++i)
                lua_pushvalue(SCRIPT, origin + i);
            else
            {
              /*
              int excess = lua_gettop( donor ) - origin - n + 1;
              luaG_inter_copy( donor, SCRIPT, n + excess );
              lua_pop( SCRIPT, excess );
              */

              // The lualanes module isn't able to interchange debug.traceback() function ._.
              luaG_inter_copy_from(donor, SCRIPT, n, origin);
            }
          }
          for(int i = 0; i < n; ++i)
            lua_remove(donor, origin);

          numberOfArguments += n;
        }
      } break;
    case 'R': {
        bool **actionPerformed = va_arg(vl, bool**);
        *actionPerformed = args[i].bp;
      } break;
    case 't': {
        int n = args[i].luatbl;
        numberOfArguments -= n;
        ALL_SCRIPTS {
          lua_newtable(SCRIPT);
          loopi(n) {
            int table = lua_gettop(SCRIPT);
            lua_pushinteger(SCRIPT, i + 1);
            lua_pushvalue(SCRIPT, table - n + i);
            lua_settable(SCRIPT, table);
            lua_remove(SCRIPT, table - n + i);
          }
        }
        numberOfArguments++;
      } break;
    case 'S': {
        lua_State **L = va_arg(vl, lua_State**);
        int *origin = va_arg(vl, int*);
        *L = args[i].lsp;
        if(origin) *origin = args[i].origin;
      } break;
    }
  }
  va_end(vl);
}

int Lua::callHandler(const char *handler, const char *arguments, ...) {
  if(numberOfScripts == 0) return -PLUGIN_BLOCK;

  LuaArg *args = NULL; int argc = 0;
  va_list vl; va_start(vl, arguments);
  args = prepareArgs(arguments, vl, argc);
  va_end(vl);

  int result = callHandler(handler, argc, args);
  if(args) free(args);

  return result;
}

int Lua::callHandler(const char *handler, int argc, const LuaArg *args) {
  if(numberOfScripts == 0) return -PLUGIN_BLOCK;

  MUTEX_LOCK(&callHandlerMutex);

  ALL_SCRIPTS {
    lua_getglobal(SCRIPT, "debug");
    lua_getfield(SCRIPT, lua_gettop(SCRIPT), "traceback");
    lua_getglobal(SCRIPT, handler);
  }

  /* The outcome interrupted by using PLUGIN_BLOCK action (takes place only when at least one of the handlers has returned PLUGIN_BLOCK):
   * True - the operation in one handler is called, which returns 2 results: PLUGIN_BLOCK and return value (true / false);
   * False - no action was performed and the handler is invoked.
   */
  bool *actionPerformed = NULL;

  int numberOfArguments = 0;
  parseArgs(argc, args, numberOfArguments, &actionPerformed);

  int result = -PLUGIN_BLOCK; // handler result
  bool fakeActionOutcome;
  if(actionPerformed == NULL) actionPerformed = &fakeActionOutcome;
  *actionPerformed = false;
  ALL_SCRIPTS {
    if(lua_isfunction(SCRIPT, lua_gettop(SCRIPT) - numberOfArguments)) {
      if(!lua_pcall(SCRIPT, numberOfArguments, 2, lua_gettop(SCRIPT) - numberOfArguments - 1)) {
        if(!(*actionPerformed) && lua_isboolean(SCRIPT, lua_gettop(SCRIPT)))
          *actionPerformed = (bool) lua_toboolean(SCRIPT, lua_gettop(SCRIPT));
        lua_pop(SCRIPT, 1);   // actionPerformed
        if(result != PLUGIN_BLOCK && lua_isnumber(SCRIPT, lua_gettop(SCRIPT)))
          result = (int) lua_tonumber(SCRIPT, lua_gettop(SCRIPT));
        lua_pop(SCRIPT, 1);   // result
      }
      else {
        printf("<Lua> Error when calling %s handler:\n%s\n", handler, lua_tostring(SCRIPT, lua_gettop(SCRIPT)));
        lua_pop(SCRIPT, 1);   // error text
      }
      lua_pop(SCRIPT, 2);   // error handlers
    }
    else lua_pop(SCRIPT, numberOfArguments + 1 /* function */ + 2 /* error handlers */);
  }

  MUTEX_UNLOCK(&callHandlerMutex);

  return result;
}

void* Lua::getFakeVariable(const char *generator, int *numberOfReturns, int luaTypeReturned, const char* arguments, ...) {
  if(numberOfScripts == 0) return NULL;

  LuaArg *args = NULL; int argc = 0;
  va_list vl; va_start(vl, arguments);
  args = prepareArgs(arguments, vl, argc);
  va_end(vl);

  void *result = getFakeVariable(generator, numberOfReturns, luaTypeReturned, argc, args);
  if(args) free(args);

  return result;
}

void* Lua::getFakeVariable(const char *generator, int *numberOfReturns, int luaTypeReturned, int argc, const LuaArg *args) {
  // cyclically call all functions generators with the given name; stop at the first result.

  if(numberOfScripts == 0) return NULL;

  MUTEX_LOCK(&callHandlerMutex);

  ALL_SCRIPTS {
    lua_getglobal(SCRIPT, "debug");
    lua_getfield(SCRIPT, lua_gettop(SCRIPT), "traceback");
    lua_getglobal(SCRIPT, generator);
  }

  int numberOfArguments = 0;
  lua_State *senderL = NULL;
  int senderOrigin = 0;

  parseArgs(argc, args, numberOfArguments, &senderL, &senderOrigin);

  void *result = NULL;
  bool resultGotten = false;
  ALL_SCRIPTS {
    if(!resultGotten && lua_isfunction(SCRIPT, lua_gettop(SCRIPT) - numberOfArguments)) {
      int top1 = lua_gettop(SCRIPT);
      if(!lua_pcall(SCRIPT, numberOfArguments, LUA_MULTRET, lua_gettop(SCRIPT) - numberOfArguments - 1)) {
        top1 -= numberOfArguments + 1;
        int top2 = lua_gettop(SCRIPT);
        int n = top2 - top1;

        if(n > 0) {
          if(numberOfReturns) *numberOfReturns = n;
          resultGotten = true;
          switch(luaTypeReturned) {
          case LUA_TNUMBER: {
              result = (void*) new double[n];
              for(int i = 0; i < n; i++) {
                if(lua_isnumber(SCRIPT, lua_gettop(SCRIPT) - n + i + 1))
                  ((double*) result)[i] = (double) lua_tonumber(SCRIPT, lua_gettop(SCRIPT) - n + i + 1);
                else {
                  delete[](double*) result; result = NULL;
                  resultGotten = false;
                  break;
                }
              }
            } break;
          case LUA_TBOOLEAN: {
              result = (void*) new bool[n];
              for(int i = 0; i < n; i++) {
                if(lua_isboolean(SCRIPT, lua_gettop(SCRIPT) - n + i + 1))
                  ((bool*) result)[i] = (bool) lua_toboolean(SCRIPT, lua_gettop(SCRIPT) - n + i + 1);
                else {
                  delete[](bool*) result; result = NULL;
                  resultGotten = false;
                  break;
                }
              }
            } break;
          case LUA_TNONE: {
              if(SCRIPT != senderL)
                luaG_inter_copy(SCRIPT, senderL, n);
              for(int i = 0; i < n; ++i)
                lua_insert(senderL, senderOrigin);
              if(SCRIPT == senderL)
                lua_settop(SCRIPT, top2 + n);   // push n nils
            } break;
          }
          lua_pop(SCRIPT, n);
        }
      }
      else {
        printf("<Lua> Error when calling %s generator:\n%s\n", generator, lua_tostring(SCRIPT, lua_gettop(SCRIPT)));
        lua_pop(SCRIPT, 1);   // error text
      }
      lua_pop(SCRIPT, 2);   // error handlers
    }
    else lua_pop(SCRIPT, numberOfArguments + 1 /* function */ + 2 /* error handlers */);
  }

  MUTEX_UNLOCK(&callHandlerMutex);

  return result;
}

void Lua_atExit(int) {
  signal(SIGINT, SIG_DFL);
  Lua::destroy();
  raise(SIGINT);
}

//BEGIN Lua global functions

LUA_FUNCTION(include) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;

  string filenameINC = { '\0' };
  copystring(filenameINC, LUA_INCLUDES_PATH);
  strcat(filenameINC, lua_tostring(L, 1));
  strcat(filenameINC, ".inc");
  if(luaL_dofile(L, filenameINC) != 0) {
    for(unsigned i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
      string filenameLUA = { '\0' };
      copystring(filenameLUA, LUA_INCLUDES_PATH);
      strcat(filenameLUA, lua_tostring(L, 1));
      strcat(filenameLUA, extensions[i]);
      if(luaL_dofile(L, filenameLUA) == 0) break;
    }
  }
  return 0;
}

LUA_FUNCTION(clientprint) {
  lua_checkstack(L, 3);
  if(!lua_isnumber(L, 1) || !lua_isstring(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn) && target_cn != -1) return 0;
  const char *message = lua_tostring(L, 2);
  if(!strlen(message)) return 0;
  int exclude_cn = -1; // no one to be excluded
  if(lua_isnumber(L, 3)) exclude_cn = (int) lua_tonumber(L, 3);
  sendf(target_cn, 1, "risx", SV_SERVMSG, message, exclude_cn);
  return 0;
}

LUA_FUNCTION(kill) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isboolean(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  bool gib = (bool) lua_toboolean(L, 2);
  sendf(-1, 1, "ri5", gib ? SV_GIBDIED : SV_DIED, target_cn, target_cn, clients[target_cn]->state.frags, GUN_KNIFE);
  return 0;
}

LUA_FUNCTION(getflagkeeper) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int flag = (int) lua_tonumber(L, 1);
  if(!valid_flag(flag)) return 0;
  int keeper_cn = sflaginfos[flag].actor_cn;
  if(sflaginfos[flag].state != CTFF_STOLEN || keeper_cn == -1) return 0;
  lua_pushinteger(L, keeper_cn);
  return 1;
}

LUA_FUNCTION(forcedeath) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  forcedeath(clients[target_cn]);
  return 0;
}

LUA_FUNCTION(spawn) {
  lua_checkstack(L, 8);
  for(int i = 1; i <= 8; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  if(team_isspect(clients[target_cn]->team)) return 0;
  clientstate &gs = clients[target_cn]->state;
  int health = (int) lua_tonumber(L, 2);
  int armour = (int) lua_tonumber(L, 3);
  int ammo = (int) lua_tonumber(L, 4);
  int mag = (int) lua_tonumber(L, 5);
  int weapon = (int) lua_tonumber(L, 6);
  if(!(weapon >= 0 && weapon < NUMGUNS)) weapon = gs.gunselect;
  int gamemode = (int) lua_tonumber(L, 7);
  if(!(gamemode >= 0 && gamemode < GMODE_NUM)) gamemode = smode;
  int primaryweapon = (int) lua_tonumber(L, 8);
  if(!(primaryweapon >= 0 && primaryweapon < NUMGUNS)) primaryweapon = gs.primary;
  gs.respawn();
  gs.spawnstate(gamemode);
  gs.lifesequence++;
  gs.ammo[primaryweapon] = ammo;
  gs.mag[primaryweapon] = mag;
  gs.health = health;
  gs.armour = armour;
  gs.primary = gs.nextprimary = primaryweapon;
  gs.gunselect = weapon;
  sendf(target_cn, 1, "ri7vv", SV_SPAWNSTATE, gs.lifesequence, gs.health, gs.armour, gs.primary,
        gs.gunselect, (m_arena) ? clients[target_cn]->spawnindex : -1, NUMGUNS, gs.ammo, NUMGUNS, gs.mag);
  gs.lastspawn = gamemillis;
  return 0;
}

LUA_FUNCTION(dodamage) {
  lua_checkstack(L, 4);
  for(int i = 1; i <= 4; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  int actor_cn = (int) lua_tonumber(L, 2);
  if(!valid_client(target_cn) || !valid_client(actor_cn)) return 0;
  if(team_isspect(clients[target_cn]->team)) return 0;
  int damage = (int) lua_tonumber(L, 3);
  int weapon = (int) lua_tonumber(L, 4);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  serverdamage(clients[target_cn], clients[actor_cn], damage, weapon, damage == INT_MAX, vec(0,0,0), true);
  return 0;
}

LUA_FUNCTION(isconnected) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!clients.inrange(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->type != ST_EMPTY));
  return 1;
}

LUA_FUNCTION(isadmin) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->role == CR_ADMIN));
  return 1;
}

LUA_FUNCTION(getpriv) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, (int)(clients[player_cn]->role));
  return 1;
}

LUA_FUNCTION(getstate) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.state);
  return 1;
}

LUA_FUNCTION(getname) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushstring(L, clients[player_cn]->name);
  return 1;
}

LUA_FUNCTION(getip) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushstring(L, clients[player_cn]->hostname);
  return 1;
}

LUA_FUNCTION(getweapon) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.gunselect);
  return 1;
}

LUA_FUNCTION(getprimary) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.primary);
  return 1;
}

LUA_FUNCTION(getpos) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  /*
  lua_newtable( L ); int table = lua_gettop( L );
  lua_pushnumber( L, (lua_Number) clients[player_cn]->state.o[0] );
  lua_setfield( L, table, "x" );
  lua_pushnumber( L, (lua_Number) clients[player_cn]->state.o[1] );
  lua_setfield( L, table, "y" );
  lua_pushnumber( L, (lua_Number) clients[player_cn]->state.o[2] );
  lua_setfield( L, table, "z" );
  */
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.o[0]);
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.o[1]);
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.o[2]);
  return 3;
}

LUA_FUNCTION(gethealth) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.health);
  return 1;
}

LUA_FUNCTION(getarmor) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.armour);
  return 1;
}

LUA_FUNCTION(getfrags) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.frags);
  return 1;
}

LUA_FUNCTION(getdeaths) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.deaths);
  return 1;
}

LUA_FUNCTION(getping) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->ping);
  return 1;
}

LUA_FUNCTION(getteam) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->team);
  return 1;
}

LUA_FUNCTION(getflags) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.flagscore);
  return 1;
}

LUA_FUNCTION(getammo) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  int weapon = (int) lua_tonumber(L, 2);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.ammo[weapon]);
  lua_pushinteger(L, clients[player_cn]->state.mag[weapon]);
  return 2;
}

LUA_FUNCTION(getnextprimary) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.primary);
  return 1;
}

LUA_FUNCTION(getvelocity) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.vel.x);
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.vel.y);
  lua_pushnumber(L, (lua_Number) clients[player_cn]->state.vel.z);
  return 3;
}

LUA_FUNCTION(getangle) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->y);   // hor
  lua_pushinteger(L, clients[player_cn]->p);   // ver
  return 2;
}

LUA_FUNCTION(getskin) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  if(!team_isactive(clients[player_cn]->team)) return 0;
  lua_pushinteger(L, clients[player_cn]->skin[clients[player_cn]->team]);
  return 1;
}

LUA_FUNCTION(setpos) {
  lua_checkstack(L, 4);
  for(int i = 1; i <= 4; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.o[0] = lua_tonumber(L, 2);
  clients[target_cn]->state.o[1] = lua_tonumber(L, 3);
  clients[target_cn]->state.o[2] = lua_tonumber(L, 4);
  return 0;
}

LUA_FUNCTION(setammo) {
  lua_checkstack(L, 4);
  for(int i = 1; i <= 4; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int weapon = (int) lua_tonumber(L, 2);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  clients[target_cn]->state.ammo[weapon] = (int) lua_tonumber(L, 3);
  clients[target_cn]->state.mag[weapon] = (int) lua_tonumber(L, 4);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setname) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isstring(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  copystring(clients[target_cn]->name, lua_tostring(L, 2));
  //sendinitclient( *clients[target_cn] );
  as_player(target_cn, "is", SV_SWITCHNAME, clients[target_cn]->name);
  return 0;
}

LUA_FUNCTION(sethealth) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.health = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setarmor) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.armour = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setweapon) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int weapon = (int) lua_tonumber(L, 2);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  clients[target_cn]->state.gunselect = weapon;
  //sendf( target_cn, 1, "ri2", SV_WEAPCHANGE, weapon );
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setprimary) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int weapon = (int) lua_tonumber(L, 2);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  clients[target_cn]->state.primary = weapon;
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setrole) {
  lua_checkstack(L, 3);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int role = (int) lua_tonumber(L, 2);
  if(!(role == CR_ADMIN || role == CR_DEFAULT)) return 0;
  if(lua_toboolean(L, 3)) {
    if(role == CR_ADMIN)
      for(int i = 0; i < clients.length(); i++)
        clients[i]->role = CR_DEFAULT;
    clients[target_cn]->role = role;
  }
  else clients[target_cn]->role = role;
  sendserveropinfo(-1);
  if(curvote) Lua_evaluateVote();
  return 0;
}

LUA_FUNCTION(setteam) {
  lua_checkstack(L, 3);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int team = (int) lua_tonumber(L, 2);
  if(!team_isvalid(team)) return 0;
  if(clients[target_cn]->team != team) {
    //sdropflag( target_cn );
    int ftr = FTR_INFO;
    if(lua_isnumber(L, 3)) ftr = (int) lua_tonumber(L, 3);
    clients[target_cn]->team = team;
    sendf(-1, 1, "riii", SV_SETTEAM, target_cn, team | (ftr << 4));
  }
  return 0;
}

LUA_FUNCTION(setfrags) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.frags = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setflags) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.flagscore = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setdeaths) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.deaths = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(setskin) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int skin = (int) lua_tonumber(L, 2);
  for(int i = 0; i < 2; i++) clients[target_cn]->skin[i] = skin;
  //sendinitclient( *clients[target_cn] );
  as_player(target_cn, "iii", SV_SWITCHSKIN, clients[target_cn]->skin[0], clients[target_cn]->skin[1]);
  return 0;
}

LUA_FUNCTION(voteend) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int result = (int) lua_tonumber(L, 1);
  if(!(result >= 0 && result < VOTE_NUM)) return 0;
  Lua_endVote(result);
  return 0;
}

LUA_FUNCTION(setservname) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  filterrichtext(scl.servdesc_full, lua_tostring(L, 1));
  filterservdesc(scl.servdesc_full, scl.servdesc_full);
  copystring(servdesc_current, scl.servdesc_full);
  global_name = scl.servdesc_full;
  return 0;
}

LUA_FUNCTION(setmaxcl) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int max_clients = (int) lua_tonumber(L, 1);
  if(max_clients <= 0) return 0;
  scl.maxclients = (max_clients > MAXCLIENTS) ? MAXCLIENTS : max_clients;
  return 0;
}

LUA_FUNCTION(setpwd) {
  static string pwd = { '\0' };
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(pwd, lua_tostring(L, 1));
  scl.serverpassword = pwd;
  return 0;
}

LUA_FUNCTION(setadminpwd) {
  lua_checkstack(L, 3);
  if(!lua_isstring(L, 1)) return 0;
  int line = 0;
  if(lua_isnumber(L, 2)) line = (int) lua_tonumber(L, 2);
  bool denyadmin = false;
  if(lua_isboolean(L, 3)) denyadmin = (bool) lua_toboolean(L, 3);
  int i;
  for(i = 0; i < passwords.adminpwds.length(); i++)
    if(passwords.adminpwds[i].line == line) {
      i = -1;
      copystring(passwords.adminpwds[i].pwd, lua_tostring(L, 1));
      if(line != 0) passwords.adminpwds[i].denyadmin = denyadmin;
      if(line == 0) scl.adminpasswd = passwords.adminpwds[i].pwd;
      break;
    }
  if(i != -1) {
    pwddetail adminpwd;
    copystring(adminpwd.pwd, lua_tostring(L, 1));
    adminpwd.line = line;
    if(line == 0) adminpwd.denyadmin = false;
    else adminpwd.denyadmin = denyadmin;
    passwords.adminpwds.insert(0, adminpwd);
    passwords.staticpasses++;
  }
  return 0;
}

LUA_FUNCTION(setmotd) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  filterrichtext(scl.motd, lua_tostring(L, 1));
  return 0;
}

LUA_FUNCTION(setblacklist) {
  static string filename = { '\0' };
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(filename, lua_tostring(L, 1));
  scl.blfile = filename;
  ipblacklist.init(scl.blfile);
  return 0;
}

LUA_FUNCTION(setnickblist) {
  static string filename = { '\0' };
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(filename, lua_tostring(L, 1));
  scl.nbfile = filename;
  nickblacklist.init(scl.nbfile);
  return 0;
}

LUA_FUNCTION(setmaprot) {
  static string filename = { '\0' };
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(filename, lua_tostring(L, 1));
  scl.maprot = filename;
  maprot.init(scl.maprot);
  return 0;
}

LUA_FUNCTION(setkickthres) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  scl.kickthreshold = (int) lua_tonumber(L, 1);
  return 0;
}

LUA_FUNCTION(setbanthres) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  scl.banthreshold = (int) lua_tonumber(L, 1);
  return 0;
}

LUA_FUNCTION(getmapname) {
  lua_pushstring(L, behindpath(smapname));
  return 1;
}

LUA_FUNCTION(gettimeleft) {
  lua_pushinteger(L, (gamelimit - gamemillis) / 1000 / 60);
  return 1;
}

LUA_FUNCTION(getgamemode) {
  lua_pushinteger(L, smode);
  return 1;
}

LUA_FUNCTION(getsvtick) {
  lua_pushinteger(L, servmillis);
  return 1;
}

LUA_FUNCTION(maxclient) {
  lua_pushinteger(L, scl.maxclients);
  return 1;
}

LUA_FUNCTION(changemap) {
  lua_checkstack(L, 3);
  if(!lua_isstring(L, 1)) return 0;
  const char *map = lua_tostring(L, 1);
  if(!lua_isnumber(L, 2)) return 0;
  int mode = (int) lua_tonumber(L, 2);
  if(!(mode >= 0 && mode < GMODE_NUM) || !lua_isnumber(L, 3)) return 0;
  int time = (int) lua_tonumber(L, 3);
  startgame(map, mode, time, true, true);
  return 0;
}

LUA_FUNCTION(flagaction) {
  lua_checkstack(L, 3);
  for(int i = 1; i <= 3; i++) if(!lua_isnumber(L, i)) return 0;
  int actor_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(actor_cn)) return 0;
  int action = (int) lua_tonumber(L, 2);
  int flag = (int) lua_tonumber(L, 3);
  flagaction(flag, action, actor_cn, true);
  return 0;
}

LUA_FUNCTION(setnextmode) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int mode = (int) lua_tonumber(L, 1);
  if(!(mode >= 0 && mode < GMODE_NUM)) return 0;
  nextgamemode = mode;
  return 0;
}

LUA_FUNCTION(setnextmap) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(nextmapname, lua_tostring(L, 1));
  return 0;
}

LUA_FUNCTION(setmastermode) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int master_mode = (int) lua_tonumber(L, 1);
  if(!(master_mode >= 0 && master_mode < MM_NUM)) return 0;
  changemastermode(master_mode);
  return 0;
}

LUA_FUNCTION(forcearenawin) {
  if(!m_arena) return 0;
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int alive = (int) lua_tonumber(L, 1);
  if(!(alive == -1 || valid_client(alive))) return 0;
  items_blocked = true;
  sendf(-1, 1, "ri2", SV_ARENAWIN, alive);
  arenaround = gamemillis + 5000;
  if(autoteam && m_teammode) refillteams(true);
  return 0;
}

LUA_FUNCTION(forcemapend) {
  forceintermission = true;
  return 0;
}

LUA_FUNCTION(getmastermode) {
  lua_pushinteger(L, mastermode);
  return 1;
}

LUA_FUNCTION(settimeleft) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int remaining_minutes = (int) lua_tonumber(L, 1);
  intermission_check_shear = gamemillis - gamemillis / 60 / 1000; // Chidori, спасибо!
  gamelimit = gamemillis + remaining_minutes * 60 * 1000;
  checkintermission();
  return 0;
}

LUA_FUNCTION(getautoteam) {
  lua_pushboolean(L, autoteam);
  return 1;
}

LUA_FUNCTION(setautoteam) {
  lua_checkstack(L, 1);
  if(!lua_isboolean(L, 1)) return 0;
  autoteam = (bool) lua_toboolean(L, 1);
  sendservermode();
  if(m_teammode && autoteam) refillteams(true);
  return 0;
}

LUA_FUNCTION(disconnect) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int reason = (int) lua_tonumber(L, 2);
  if(!(reason >= 0 && reason < DISC_NUM) && reason != -1) return 0;
  disconnect_client(target_cn, reason, true);
  return 0;
}

LUA_FUNCTION(ban) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int ban_time = scl.ban_time;
  if(lua_isnumber(L, 2)) ban_time = (int) lua_tonumber(L, 2);
  ban ban = { clients[target_cn]->peer->address, servmillis + ban_time };
  bans.add(ban);
  disconnect_client(target_cn, DISC_MBAN, true);
  return 0;
}

LUA_FUNCTION(setvelocity) {
  lua_checkstack(L, 5);
  for(int i = 1; i <= 5; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  sendf(target_cn, 1, "ri6", SV_HITPUSH, GUN_GRENADE, (int) lua_tonumber(L, 5),
        int(lua_tonumber(L, 2) * DNF), int(lua_tonumber(L, 3) * DNF), int(lua_tonumber(L, 4) * DNF));
  return 0;
}

LUA_FUNCTION(getdefaultammo) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int weapon = (int) lua_tonumber(L, 1);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  int mag = guns[weapon].magsize, ammo = ((weapon == GUN_PISTOL) ? ammostats[weapon].max : ammostats[weapon].start) - mag;
  lua_pushinteger(L, ammo);
  lua_pushinteger(L, mag);
  return 2;
}

static int stringsort(const char **x, const char **y) { return strcmp(*x, *y); }
LUA_FUNCTION(getservermaps) {
  lua_checkstack(L, 1);
  vector<char*> maps;
  if(lua_toboolean(L, 1))
    listfiles(SERVERMAP_PATH_BUILTIN, "cgz", maps);
  listfiles(SERVERMAP_PATH, "cgz", maps);
  listfiles(SERVERMAP_PATH_INCOMING, "cgz", maps);
  maps.sort(stringsort);
  lua_newtable(L);
  loopv(maps) {
    lua_pushinteger(L, i + 1);
    lua_pushstring(L, behindpath(maps[i]));
    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(giveitem) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int item_type = (int) lua_tonumber(L, 2);
  int item_id = sents.length();
  sendf(target_cn, 1, "ri9", SV_EDITENT, item_id, item_type, 0, 0, 0, 0, 0, 0, 0);
  sendf(target_cn, 1, "ri3", SV_ITEMACC, item_id, target_cn);
  clients[target_cn]->state.pickup(item_type);
  return 0;
}

LUA_FUNCTION(shuffleteams) {
  sendf(-1, 1, "ri2", SV_SERVERMODE, sendservermode(false) | AT_SHUFFLE);
  shuffleteams();
  return 0;
}

LUA_FUNCTION(getprotocol) {
  lua_pushinteger(L, server_protocol_version);
  return 1;
}

LUA_FUNCTION(getdefaultprotocol) {
  lua_pushinteger(L, PROTOCOL_VERSION);
  return 1;
}

LUA_FUNCTION(setprotocol) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  server_protocol_version = (int) lua_tonumber(L, 1);
  return 0;
}

LUA_FUNCTION(luanotice) {
  lua_checkstack(L, 2);
  if(!lua_isstring(L, 2)) return 0;
  int origin = lua_gettop(L) + 1;
  lua_pushvalue(L, 1);
  Lua::callHandler(LUA_ON_NOTICE, "os", L,origin, lua_tostring(L, 2));
  return 0;
}

LUA_FUNCTION(getnextmode) {
  lua_pushinteger(L, nextgamemode);
  return 1;
}

LUA_FUNCTION(getnextmap) {
  lua_pushstring(L, nextmapname);
  return 1;
}

LUA_FUNCTION(getmapitems) {
  lua_newtable(L);
  loopv(sents) {
    lua_pushinteger(L, i);
    lua_pushinteger(L, sents[i].type);
    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(spawnitem) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int item_id = (int) lua_tonumber(L, 1);
  if(!sents.inrange(item_id)) return 0;
  sents[item_id].spawntime = 0;
  sents[item_id].spawned = true;
  sendf(-1, 1, "ri2", SV_ITEMSPAWN, item_id);
  return 0;
}

LUA_FUNCTION(removebans) {
  bans.shrink(0);
  return 0;
}

LUA_FUNCTION(getscore) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.points);
  return 1;
}

LUA_FUNCTION(setscore) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.points = (int) lua_tonumber(L, 2);
  sendresume(*clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(getadminpwd) {
  lua_checkstack(L, 1);
  int line = 0;
  if(lua_isnumber(L, 1)) line = (int) lua_tonumber(L, 1);
  for(int i = 0; i < passwords.adminpwds.length(); i++)
    if(passwords.adminpwds[i].line == line) {
      lua_pushstring(L, passwords.adminpwds[i].pwd);
      return 1;
    }
  return 0;
}

LUA_FUNCTION(removeadminpwd) {
  lua_checkstack(L, 1);
  int line = -1; bool byLine = false;
  string adminpwd; bool byPassword = false;
  if(lua_isnumber(L, 1)) {
    line = (int) lua_tonumber(L, 1);
    byLine = true;
  }
  else if(lua_isstring(L, 1)) {
    copystring(adminpwd, lua_tostring(L, 1));
    byPassword = true;
  }
  if(byLine || byPassword) for(int i = 0; i < passwords.adminpwds.length(); i++)
      if((byPassword && strcmp(adminpwd, passwords.adminpwds[i].pwd) == 0) || (byLine && passwords.adminpwds[i].line == line)) {
        passwords.adminpwds.remove(i);
        if(i < passwords.staticpasses) passwords.staticpasses--;
        break;
      }
  return 0;
}

LUA_FUNCTION(setmasterserver) {
  static string master = { '\0' };
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  copystring(master, lua_tostring(L, 1));
  scl.master = master;
  return 0;
}

LUA_FUNCTION(getmaprotnextmap) {
  configset *cs = maprot.get(maprot.get_next());
  if(cs) {
    lua_pushstring(L, cs->mapname);
    return 1;
  }
  return 0;
}

LUA_FUNCTION(getmaprotnextmode) {
  configset *cs = maprot.get(maprot.get_next());
  if(cs) {
    lua_pushinteger(L, cs->mode);
    return 1;
  }
  return 0;
}

LUA_FUNCTION(getadminpwds) {
  lua_newtable(L);
  for(int i = 0; i < passwords.adminpwds.length(); i++) {
    lua_pushinteger(L, passwords.adminpwds[i].line);
    lua_pushstring(L, passwords.adminpwds[i].pwd);
    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(getwholemaprot) {
  lua_newtable(L);
  loopv(maprot.configsets) {
    lua_pushinteger(L, i + 1);
    lua_newtable(L);
    int table = lua_gettop(L);

    lua_pushstring(L, maprot.configsets[i].mapname);
    lua_setfield(L, table, "map");

    lua_pushinteger(L, maprot.configsets[i].mode);
    lua_setfield(L, table, "mode");

    lua_pushinteger(L, maprot.configsets[i].time);
    lua_setfield(L, table, "time");

    lua_pushinteger(L, maprot.configsets[i].vote);
    lua_setfield(L, table, "allowVote");

    lua_pushinteger(L, maprot.configsets[i].minplayer);
    lua_setfield(L, table, "minplayer");

    lua_pushinteger(L, maprot.configsets[i].maxplayer);
    lua_setfield(L, table, "maxplayer");

    lua_pushinteger(L, maprot.configsets[i].skiplines);
    lua_setfield(L, table, "skiplines");

    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(setwholemaprot) {
  lua_checkstack(L, 1);
  if(!lua_istable(L, 1)) return 0;
  maprot.configsets.shrink(0);
  int n = luaL_getn(L, 1);
  for(int i = 1; i <= n; i++) {
    configset cs;
    lua_pushinteger(L, i);
    lua_gettable(L, 1);
    int table = lua_gettop(L);

    lua_getfield(L, table, "map");
    copystring(cs.mapname, lua_tostring(L, lua_gettop(L)));

    lua_getfield(L, table, "mode");
    cs.mode = (int) lua_tonumber(L, lua_gettop(L));

    lua_getfield(L, table, "time");
    cs.time = (int) lua_tonumber(L, lua_gettop(L));

    lua_getfield(L, table, "allowVote");
    if(lua_isnumber(L, lua_gettop(L)))
      cs.vote = (int) lua_tonumber(L, lua_gettop(L));
    else cs.vote = 1;

    lua_getfield(L, table, "minplayer");
    if(lua_isnumber(L, lua_gettop(L)))
      cs.minplayer = (int) lua_tonumber(L, lua_gettop(L));
    else cs.minplayer = 0;

    lua_getfield(L, table, "maxplayer");
    if(lua_isnumber(L, lua_gettop(L)))
      cs.maxplayer = (int) lua_tonumber(L, lua_gettop(L));
    else cs.maxplayer = 0;

    lua_getfield(L, table, "skiplines");
    if(lua_isnumber(L, lua_gettop(L)))
      cs.skiplines = (int) lua_tonumber(L, lua_gettop(L));
    else cs.skiplines = 0;

    lua_pop(L, 7);   // the values
    lua_pop(L, 1);   // the table
    maprot.configsets.add(cs);
  }
  return 0;
}

LUA_FUNCTION(getwholebl) {
  lua_newtable(L);
  loopv(ipblacklist.ipranges) {
    lua_pushinteger(L, i + 1);
    lua_newtable(L);
    int table = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushstring(L, iptoa(ipblacklist.ipranges[i].lr));
    lua_settable(L, table);

    lua_pushinteger(L, 2);
    lua_pushstring(L, iptoa(ipblacklist.ipranges[i].ur));
    lua_settable(L, table);

    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(setwholebl) {
  lua_checkstack(L, 1);
  if(!lua_istable(L, 1)) return 0;
  ipblacklist.ipranges.shrink(0);
  int n = luaL_getn(L, 1);
  for(int i = 1; i <= n; i++) {
    iprange ir;
    lua_pushinteger(L, i);
    lua_gettable(L, 1);
    int table = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_gettable(L, table);
    atoip(lua_tostring(L, lua_gettop(L)), &ir.lr);

    lua_pushinteger(L, 2);
    lua_gettable(L, table);
    atoip(lua_tostring(L, lua_gettop(L)), &ir.ur);

    lua_pop(L, 2);   // the values
    lua_pop(L, 1);   // the table
    ipblacklist.ipranges.add(ir);
  }
  return 0;
}

LUA_FUNCTION(strtoiprange) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  iprange ir;
  if(atoipr(lua_tostring(L, 1), &ir) == NULL) return 0;
  lua_newtable(L);

  lua_pushinteger(L, 1);
  lua_pushstring(L, iptoa(ir.lr));
  lua_settable(L, 2);

  lua_pushinteger(L, 2);
  lua_pushstring(L, iptoa(ir.ur));
  lua_settable(L, 2);
  return 1;
}

LUA_FUNCTION(setsockettype) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!clients.inrange(target_cn)) return 0;
  int socket_type = (int) lua_tonumber(L, 2);
  clients[target_cn]->type = socket_type;
  return 0;
}

LUA_FUNCTION(addclient) {
  client& cl = addclient();
  lua_pushinteger(L, cl.clientnum);
  return 1;
}

LUA_FUNCTION(initclient) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!clients.inrange(target_cn)) return 0;
  clients[target_cn]->isauthed = true;
  clients[target_cn]->haswelcome = true;
  sendinitclient(*clients[target_cn]);
  return 0;
}

LUA_FUNCTION(delclient) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!clients.inrange(target_cn)) return 0;
  clients[target_cn]->zap();
  sendf(-1, 1, "ri2", SV_CDIS, target_cn);
  return 0;
}

LUA_FUNCTION(sethostname) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isstring(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!clients.inrange(target_cn)) return 0;
  copystring(clients[target_cn]->hostname, lua_tostring(L, 2));
  return 0;
}

LUA_FUNCTION(sayas) {
  lua_checkstack(L, 4);
  if(!lua_isstring(L, 1) || !lua_isnumber(L, 2)) return 0;
  bool team = false;
  if(lua_isboolean(L, 3)) team = (bool) lua_toboolean(L, 3);
  bool me = false;
  if(lua_isboolean(L, 4)) me = (bool) lua_toboolean(L, 4);
  int player_cn = (int) lua_tonumber(L, 2);
  if(!valid_client(player_cn)) return 0;
  char text[MAXTRANS];
  copystring(text, lua_tostring(L, 1));
  filtertext(text, text);
  trimtrailingwhitespace(text);
  if(team) sendteamtext(text, player_cn, me ? SV_TEAMTEXTME : SV_TEAMTEXT);
  else as_player(player_cn, "isx", me ? SV_TEXTME : SV_TEXT, text, player_cn);
  return 0;
}

LUA_FUNCTION(voiceas) {
  lua_checkstack(L, 3);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  bool team = false;
  if(lua_isboolean(L, 3)) team = (bool) lua_toboolean(L, 3);
  int player_cn = (int) lua_tonumber(L, 2);
  if(!valid_client(player_cn)) return 0;
  int sound = (int) lua_tonumber(L, 1);
  if(sound < 0 || sound >= S_NULL) return 0;
  if(team) sendvoicecomteam(sound, player_cn);
  else as_player(player_cn, "iix", SV_VOICECOM, sound, player_cn);
  return 0;
}

LUA_FUNCTION(getgamemillis) {
  lua_pushinteger(L, gamemillis);
  return 1;
}

LUA_FUNCTION(getgamelimit) {
  lua_pushinteger(L, gamelimit);
  return 1;
}

LUA_FUNCTION(gettimeleftmillis) {
  lua_pushinteger(L, gamelimit - gamemillis);
  return 1;
}

LUA_FUNCTION(settimeleftmillis) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int remaining_millis = (int) lua_tonumber(L, 1);
  intermission_check_shear = gamemillis - gamemillis / 60 / 1000;
  gamelimit = gamemillis + remaining_millis;
  checkintermission();
  return 0;
}

LUA_FUNCTION(setping) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int ping = (int) lua_tonumber(L, 2);
  clients[target_cn]->ping = ping;
  return 0;
}

LUA_FUNCTION(setangle) {
  lua_checkstack(L, 3);
  for(int i = 1; i <= 3; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->y = (int) lua_tonumber(L, 2);   // hor
  clients[target_cn]->p = (int) lua_tonumber(L, 3);   // ver
  return 0;
}

LUA_FUNCTION(removeban) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  enet_uint32 ip;
  if(atoip(lua_tostring(L, 1), &ip) == NULL) return 0;

  loopv(bans) {
    ban &ban = bans[i];
    if(ban.address.host == ip) {
      bans.remove(i);
      break;
    }
  }
  return 0;
}

LUA_FUNCTION(getbans) {
  lua_newtable(L);
  loopv(bans) {
    lua_pushinteger(L, i + 1);
    lua_newtable(L);
    int table = lua_gettop(L);

    lua_pushinteger(L, 1);
    lua_pushstring(L, iptoa(bans[i].address.host));
    lua_settable(L, table);

    lua_pushinteger(L, 2);
    lua_pushinteger(L, bans[i].millis);
    lua_settable(L, table);

    lua_settable(L, 1);
  }
  return 1;
}

LUA_FUNCTION(getmaprotnexttimeleft) {
  configset *cs = maprot.get(maprot.get_next());
  if(cs) {
    lua_pushinteger(L, cs->time);
    return 1;
  }
  return 0;
}

LUA_FUNCTION(getmaprotnextentry) {
  configset *cs = maprot.get(maprot.get_next());
  if(cs) {
    lua_newtable(L);
    int table = lua_gettop(L);

    lua_pushstring(L, cs->mapname);
    lua_setfield(L, table, "map");

    lua_pushinteger(L, cs->mode);
    lua_setfield(L, table, "mode");

    lua_pushinteger(L, cs->time);
    lua_setfield(L, table, "time");

    lua_pushinteger(L, cs->vote);
    lua_setfield(L, table, "allowVote");

    lua_pushinteger(L, cs->minplayer);
    lua_setfield(L, table, "minplayer");

    lua_pushinteger(L, cs->maxplayer);
    lua_setfield(L, table, "maxplayer");

    lua_pushinteger(L, cs->skiplines);
    lua_setfield(L, table, "skiplines");

    return 1;
  }
  return 0;
}

LUA_FUNCTION(getspawnmillis) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.spawn);
  return 1;
}

LUA_FUNCTION(cla_iterator) {
  IteratorData &it = *(IteratorData*) lua_touserdata(L, lua_upvalueindex(1));
  for(; it.player_cn < clients.length(); it.player_cn++) {
    if(valid_client(it.player_cn) &&
        (clients[it.player_cn]->team == TEAM_CLA || clients[it.player_cn]->team == TEAM_CLA_SPECT) &&
        (!it.alive_only || clients[it.player_cn]->state.state == CS_ALIVE)) {
      lua_pushinteger(L, it.player_cn++);
      return 1;
    }
  }
  return 0;
}
LUA_FUNCTION(cla) {
  lua_checkstack(L, 1);
  bool alive_only = false;
  if(lua_isboolean(L, 1)) alive_only = (bool) lua_toboolean(L, 1);

  IteratorData &it = *(IteratorData*) lua_newuserdata(L, sizeof(IteratorData));
  it.alive_only = alive_only;
  it.player_cn = 0;

  luaL_getmetatable(L, "IteratorData");
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, __cla_iterator, 1);
  return 1;
}

LUA_FUNCTION(rvsf_iterator) {
  IteratorData &it = *(IteratorData*) lua_touserdata(L, lua_upvalueindex(1));
  for(; it.player_cn < clients.length(); it.player_cn++) {
    if(valid_client(it.player_cn) &&
        (clients[it.player_cn]->team == TEAM_RVSF || clients[it.player_cn]->team == TEAM_RVSF_SPECT) &&
        (!it.alive_only || clients[it.player_cn]->state.state == CS_ALIVE)) {
      lua_pushinteger(L, it.player_cn++);
      return 1;
    }
  }
  return 0;
}
LUA_FUNCTION(rvsf) {
  lua_checkstack(L, 1);
  bool alive_only = false;
  if(lua_isboolean(L, 1)) alive_only = (bool) lua_toboolean(L, 1);

  IteratorData &it = *(IteratorData*) lua_newuserdata(L, sizeof(IteratorData));
  it.alive_only = alive_only;
  it.player_cn = 0;

  luaL_getmetatable(L, "IteratorData");
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, __rvsf_iterator, 1);
  return 1;
}

LUA_FUNCTION(spectators_iterator) {
  IteratorData &it = *(IteratorData*) lua_touserdata(L, lua_upvalueindex(1));
  for(; it.player_cn < clients.length(); it.player_cn++) {
    if(valid_client(it.player_cn) &&
        (clients[it.player_cn]->team == TEAM_SPECT ||
         clients[it.player_cn]->team == TEAM_RVSF_SPECT ||
         clients[it.player_cn]->team == TEAM_CLA_SPECT)) {
      lua_pushinteger(L, it.player_cn++);
      return 1;
    }
  }
  return 0;
}
LUA_FUNCTION(spectators) {
  IteratorData &it = *(IteratorData*) lua_newuserdata(L, sizeof(IteratorData));
  it.player_cn = 0;

  luaL_getmetatable(L, "IteratorData");
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, __spectators_iterator, 1);
  return 1;
}

LUA_FUNCTION(players_iterator) {
  IteratorData &it = *(IteratorData*) lua_touserdata(L, lua_upvalueindex(1));
  for(; it.player_cn < clients.length(); it.player_cn++) {
    if(valid_client(it.player_cn) &&
        (!it.alive_only || (clients[it.player_cn]->state.state == CS_ALIVE && clients[it.player_cn]->team != TEAM_SPECT))) {
      lua_pushinteger(L, it.player_cn++);
      return 1;
    }
  }
  return 0;
}
LUA_FUNCTION(players) {
  lua_checkstack(L, 1);
  bool alive_only = false;
  if(lua_isboolean(L, 1)) alive_only = (bool) lua_toboolean(L, 1);

  IteratorData &it = *(IteratorData*) lua_newuserdata(L, sizeof(IteratorData));
  it.alive_only = alive_only;
  it.player_cn = 0;

  luaL_getmetatable(L, "IteratorData");
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, __players_iterator, 1);
  return 1;
}

LUA_FUNCTION(logline) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isstring(L, 2)) return 0;
  logline(lua_tonumber(L, 1), lua_tostring(L, 2));
  return 0;
}

LUA_FUNCTION(setmaprotnextentry) {
  lua_checkstack(L, 1);
  if(!lua_istable(L, 1)) return 0;
  configset *cs = maprot.get(maprot.get_next());
  if(cs) {
    lua_getfield(L, 1, "map");
    copystring(cs->mapname, lua_tostring(L, lua_gettop(L)));

    lua_getfield(L, 1, "mode");
    cs->mode = (int) lua_tonumber(L, lua_gettop(L));

    lua_getfield(L, 1, "time");
    cs->time = (int) lua_tonumber(L, lua_gettop(L));

    lua_getfield(L, 1, "allowVote");
    if(lua_isnumber(L, lua_gettop(L)))
      cs->vote = (int) lua_tonumber(L, lua_gettop(L));
    else cs->vote = 1;

    lua_getfield(L, 1, "minplayer");
    if(lua_isnumber(L, lua_gettop(L)))
      cs->minplayer = (int) lua_tonumber(L, lua_gettop(L));
    else cs->minplayer = 0;

    lua_getfield(L, 1, "maxplayer");
    if(lua_isnumber(L, lua_gettop(L)))
      cs->maxplayer = (int) lua_tonumber(L, lua_gettop(L));
    else cs->maxplayer = 0;

    lua_getfield(L, 1, "skiplines");
    if(lua_isnumber(L, lua_gettop(L)))
      cs->skiplines = (int) lua_tonumber(L, lua_gettop(L));
    else cs->skiplines = 0;

    lua_pop(L, 7);
  }
  return 0;
}

LUA_FUNCTION(setmaprotnextmap) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  configset *cs = maprot.get(maprot.get_next());
  if(cs) copystring(cs->mapname, lua_tostring(L, 1));
  return 0;
}

LUA_FUNCTION(setmaprotnextmode) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  configset *cs = maprot.get(maprot.get_next());
  if(cs) cs->mode = (int) lua_tonumber(L, 1);
  return 0;
}

LUA_FUNCTION(setmaprotnexttimeleft) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  configset *cs = maprot.get(maprot.get_next());
  if(cs) cs->time = (int) lua_tonumber(L, 1);
  return 0;
}

LUA_FUNCTION(getreloadtime) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int gun = (int) lua_tonumber(L, 1);
  if(!(gun >= 0 && gun < NUMGUNS)) return 0;
  lua_pushinteger(L, reloadtime(gun));
  return 1;
}

LUA_FUNCTION(getattackdelay) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int gun = (int) lua_tonumber(L, 1);
  if(!(gun >= 0 && gun < NUMGUNS)) return 0;
  lua_pushinteger(L, attackdelay(gun));
  return 1;
}

LUA_FUNCTION(getmagsize) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int gun = (int) lua_tonumber(L, 1);
  if(!(gun >= 0 && gun < NUMGUNS)) return 0;
  lua_pushinteger(L, magsize(gun));
  return 1;
}

LUA_FUNCTION(getsessionid) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->salt);
  return 1;
}

LUA_FUNCTION(setscoping) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isboolean(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  bool scoping = (bool) lua_toboolean(L, 2);
  clients[target_cn]->g -= clients[target_cn]->g & (1 << 4);
  clients[target_cn]->g |= ((int) scoping) << 4;
  clients[target_cn]->state.scoping = scoping;
  return 0;
}

LUA_FUNCTION(setcrouching) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_isboolean(L, 2)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  bool crouching = (bool) lua_toboolean(L, 2);
  clients[target_cn]->f -= clients[target_cn]->f & (1 << 7);
  clients[target_cn]->f |= ((int) crouching) << 7;
  clients[target_cn]->state.crouching = crouching;
  return 0;
}

LUA_FUNCTION(shootas) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1) || !lua_istable(L, 2)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;

  gameevent &event = clients[player_cn]->addevent();
  event.type = GE_SHOT;
  event.shot.id = event.shot.millis = gamemillis;
  event.shot.gun = clients[player_cn]->state.gunselect;
  loopk(3) { event.shot.from[k] = clients[player_cn]->state.o.v[k] + (k == 2 ? (((clients[player_cn]->f>>7)&1)?2.2f:4.2f) : 0); }

  lua_pushinteger(L, 1);
  lua_gettable(L, 2);
  event.shot.to[0] = (float) lua_tonumber(L, lua_gettop(L));
  lua_pushinteger(L, 2);
  lua_gettable(L, 2);
  event.shot.to[1] = (float) lua_tonumber(L, lua_gettop(L));
  lua_pushinteger(L, 3);
  lua_gettable(L, 2);
  event.shot.to[2] = (float) lua_tonumber(L, lua_gettop(L));
  lua_pop(L, 3);

  return 0;
}

LUA_FUNCTION(reloadas) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  gameevent &event = clients[player_cn]->addevent();
  event.type = GE_RELOAD;
  event.reload.id = event.reload.millis = gamemillis;
  event.reload.gun = clients[player_cn]->state.gunselect;
  return 0;
}

LUA_FUNCTION(pickupas) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  gameevent &event = clients[player_cn]->addevent();
  event.type = GE_PICKUP;
  event.pickup.ent = (int) lua_tonumber(L, 2);
  return 0;
}

LUA_FUNCTION(setip) {
  /*
  lua_checkstack( L, 2 );
  if ( !lua_isnumber( L, 1 ) || !lua_isstring( L, 2 ) ) return 0;
  int target_cn = (int) lua_tonumber( L, 1 );
  if ( !clients.inrange( target_cn ) ) return 0;
  copystring( clients[target_cn]->hostname, lua_tostring( L, 2 ) );
  return 0;
  */
  return __sethostname(L);
}

LUA_FUNCTION(removemap) {
  lua_checkstack(L, 1);
  string map;
  copystring(map, lua_tostring(L, 1));
  filtertext(map, map);
  const char *mapname = behindpath(map);
  int mp = findmappath(mapname);
  if(readonlymap(mp) || mp == MAP_NOTFOUND) lua_pushboolean(L, 0);
  else {
    string file;
    formatstring(file)(SERVERMAP_PATH_INCOMING "%s.cgz", mapname);
    remove(file);
    formatstring(file)(SERVERMAP_PATH_INCOMING "%s.cfg", mapname);
    remove(file);

    lua_pushboolean(L, 1);
  }
  return 1;
}

LUA_FUNCTION(mapisok) {
  lua_checkstack(L, 1);
  mapstats *ms;
  if(lua_isstring(L, 1))
    ms = getservermapstats(lua_tostring(L, 1), false, NULL);
  else if(lua_isnil(L, 1))
    ms = &smapstats;
  else return 0;
  lua_pushboolean(L, (ms != NULL && mapisok(ms)));
  return 1;
}

LUA_FUNCTION(mapexists) {
  lua_checkstack(L, 1);
  string map;
  copystring(map, lua_tostring(L, 1));
  filtertext(map, map);
  const char *mapname = behindpath(map);
  int mp = findmappath(mapname);
  lua_pushboolean(L, mp != MAP_NOTFOUND);
  return 1;
}

LUA_FUNCTION(isonfloor) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, clients[player_cn]->state.onfloor);
  return 1;
}

LUA_FUNCTION(isonladder) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, clients[player_cn]->state.onladder);
  return 1;
}

LUA_FUNCTION(isscoping) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, clients[player_cn]->state.scoping);
  return 1;
}

LUA_FUNCTION(iscrouching) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, clients[player_cn]->state.crouching);
  return 1;
}

LUA_FUNCTION(voteas) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  int vote = (int) lua_tonumber(L, 2);
  if(!curvote || vote < VOTE_YES || vote > VOTE_NO) return 0;
  if(clients[player_cn]->vote == VOTE_NEUTRAL) {
    packetbuf packet(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(packet, SV_VOTE);
    putint(packet, vote);
    sendpacket(-1, 1, packet.finalize(), player_cn);
    clients[player_cn]->vote = vote;
    Lua_evaluateVote();
  }
  return 0;
}

LUA_FUNCTION(getitempos) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int item_id = (int) lua_tonumber(L, 1);
  if(!sents.inrange(item_id)) return 0;
  lua_pushnumber(L, sents[item_id].x);
  lua_pushnumber(L, sents[item_id].y);
  lua_pushnumber(L, sents[item_id].z);
  return 3;
}

LUA_FUNCTION(isitemspawned) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int item_id = (int) lua_tonumber(L, 1);
  if(!sents.inrange(item_id)) return 0;
  lua_pushboolean(L, sents[item_id].spawned);
  return 1;
}

LUA_FUNCTION(setstate) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  int state = (int) lua_tonumber(L, 2);
  if(!(state >= CS_ALIVE && state <= CS_SPECTATE)) return 0;
  clients[target_cn]->state.state = state;
  return 0;
}

LUA_FUNCTION(sendspawn) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  sendspawn(clients[target_cn], true);
  return 0;
}

LUA_FUNCTION(clienthasflag) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn) || !m_flags) return 0;
  int flag = clienthasflag(player_cn);
  if(flag == -1) return 0;
  lua_pushnumber(L, flag);
  return 1;
  /*
  loopi(2)
  {
    if ( sflaginfos[i].state == CTFF_STOLEN && sflaginfos[i].actor_cn == player_cn )
    {
      lua_pushnumber( L, i );
      return 1;
    }
  }
  return 0;
  */
}

LUA_FUNCTION(getmappath) {
  lua_checkstack(L, 1);
  string map;
  copystring(map, lua_tostring(L, 1));
  filtertext(map, map);
  const char *mapname = behindpath(map);
  string mappath;
  int mp = findmappath(mapname, mappath);
  if(mp == MAP_NOTFOUND) return 0;
  lua_pushstring(L, mappath);
  return 1;
}

LUA_FUNCTION(getvote) {
  lua_checkstack(L, 2);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushnumber(L, clients[player_cn]->vote);
  return 1;
}

LUA_FUNCTION(getpushfactor) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int weapon = (int) lua_tonumber(L, 1);
  if(!(weapon >= 0 && weapon < NUMGUNS)) return 0;
  lua_pushnumber(L, guns[weapon].pushfactor);
  return 1;
}

LUA_FUNCTION(genpwdhash) {
  lua_checkstack(L, 3);
  if(!lua_isstring(L, 1)) return 0;
  const char *player_name = lua_tostring(L, 1);
  if(!lua_isstring(L, 2)) return 0;
  const char *password = lua_tostring(L, 2);
  if(!lua_isnumber(L, 3)) return 0;
  int session_id = (int) lua_tonumber(L, 3);
  lua_pushstring(L, genpwdhash(player_name, password, session_id));
  return 1;
}

LUA_FUNCTION(getacversion) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->acversion);
  return 1;
}

LUA_FUNCTION(getacbuildtype) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->acbuildtype);
  return 1;
}

LUA_FUNCTION(getconnectmillis) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->connectmillis);
  return 1;
}

LUA_FUNCTION(gettotalsent) {
  unsigned long long int totalSent = Lua_totalSent;
  std::string str;
  while(totalSent > 0ull) {
    str.insert(0, 1, char(totalSent % 10ull + '0'));
    totalSent /= 10ull;
  }
  lua_pushstring(L, str.c_str());
  return 1;
}

LUA_FUNCTION(gettotalreceived) {
  unsigned long long int totalReceived = Lua_totalReceived;
  std::string str;
  while(totalReceived > 0ull) {
    str.insert(0, 1, char(totalReceived % 10ull + '0'));
    totalReceived /= 10ull;
  }
  lua_pushstring(L, str.c_str());
  return 1;
}

LUA_FUNCTION(cleargbans) {
  cleargbans();
  return 0;
}

LUA_FUNCTION(addgban) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  addgban(lua_tostring(L, 1));
  return 0;
}

LUA_FUNCTION(checkgban) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  enet_uint32 ip;
  if(atoip(lua_tostring(L, 1), &ip) == NULL) return 0;
  lua_pushboolean(L, checkgban(ip));
  return 1;
}

LUA_FUNCTION(getteamkills) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushinteger(L, clients[player_cn]->state.teamkills);
  return 1;
}

LUA_FUNCTION(setteamkills) {
  lua_checkstack(L, 2);
  for(int i = 1; i <= 2; i++) if(!lua_isnumber(L, i)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  clients[target_cn]->state.teamkills = (int) lua_tonumber(L, 2);
  return 0;
}

LUA_FUNCTION(rereadcfgs) {
  rereadcfgs();
  return 0;
}

LUA_FUNCTION(callhandler) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  const char *handler = lua_tostring(L, 1);
  int argc = lua_gettop(L) - 1;

  int origin = lua_gettop(L) + 1;
  for(int i = 0; i < argc; ++i)
    lua_pushvalue(L, 2 + i);

  LuaArg *args = new LuaArg[1];

  args[0].type = 'o';
  args[0].count = argc;
  args[0].lsp = L;
  args[0].origin = origin;

  lua_pushinteger(L, Lua::callHandler(handler, 1, args));
  delete[] args;

  return 1;
}

LUA_FUNCTION(isediting) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->state.state == CS_EDITING));
  return 1;
}

LUA_FUNCTION(flashonradar) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int target_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(target_cn)) return 0;
  as_player(target_cn, "i8", SV_THROWNADE, 0,0,0, 0,0,0, -1);
  return 0;
}

LUA_FUNCTION(isalive) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->state.state == CS_ALIVE));
  return 1;
}

LUA_FUNCTION(getwaterlevel) {
  lua_checkstack(L, 1);
  mapstats *ms;
  if(lua_isstring(L, 1))
    ms = getservermapstats(lua_tostring(L, 1), false, NULL);
  else if(lua_isnil(L, 1))
    ms = &smapstats;
  else return 0;
  if(ms == NULL) return 0;
  lua_pushinteger(L, ms->hdr.waterlevel);
  return 1;
}

LUA_FUNCTION(isinwater) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->state.o.z < smapstats.hdr.waterlevel));
  return 1;
}

LUA_FUNCTION(isunderwater) {
  lua_checkstack(L, 1);
  if(!lua_isnumber(L, 1)) return 0;
  int player_cn = (int) lua_tonumber(L, 1);
  if(!valid_client(player_cn)) return 0;
  lua_pushboolean(L, (int)(clients[player_cn]->state.o.z + (((clients[player_cn]->f>>7)&1)?2.2f:4.2f) < smapstats.hdr.waterlevel));
  return 1;
}

LUA_FUNCTION(callgenerator) {
  lua_checkstack(L, 1);
  if(!lua_isstring(L, 1)) return 0;
  const char *handler = lua_tostring(L, 1);
  int argc = lua_gettop(L) - 1;

  int origin = lua_gettop(L) + 1;
  for(int i = 0; i < argc; ++i)
    lua_pushvalue(L, 2 + i);

  LuaArg *args = new LuaArg[2];

  args[0].type = 'o';
  args[0].count = argc;
  args[0].lsp = L;
  args[0].origin = origin;

  args[1].type = 'S';
  args[1].lsp = L;
  args[1].origin = origin;

  int n = 0;
  Lua::getFakeVariable(handler, &n, LUA_TNONE, 2, args);
  delete[] args;

  return n;
}


//END Lua global functions

