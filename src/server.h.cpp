#include "server.h"

const char *messagenames[] =
{
    "SV_SERVINFO", "SV_WELCOME", "SV_INITCLIENT", "SV_POS", "SV_POSC", "SV_POSN", "SV_TEXT", "SV_TEAMTEXT", "SV_TEXTME", "SV_TEAMTEXTME",
    "SV_SOUND", "SV_VOICECOM", "SV_VOICECOMTEAM", "SV_CDIS",
    "SV_SHOOT", "SV_EXPLODE", "SV_SUICIDE", "SV_AKIMBO", "SV_RELOAD", "SV_AUTHT", "SV_AUTHREQ", "SV_AUTHTRY", "SV_AUTHANS", "SV_AUTHCHAL",
    "SV_GIBDIED", "SV_DIED", "SV_GIBDAMAGE", "SV_DAMAGE", "SV_HITPUSH", "SV_SHOTFX", "SV_THROWNADE",
    "SV_TRYSPAWN", "SV_SPAWNSTATE", "SV_SPAWN", "SV_SPAWNDENY", "SV_FORCEDEATH", "SV_RESUME",
    "SV_DISCSCORES", "SV_TIMEUP", "SV_EDITENT", "SV_ITEMACC",
    "SV_MAPCHANGE", "SV_ITEMSPAWN", "SV_ITEMPICKUP",
    "SV_PING", "SV_PONG", "SV_CLIENTPING", "SV_GAMEMODE",
    "SV_EDITMODE", "SV_EDITH", "SV_EDITT", "SV_EDITS", "SV_EDITD", "SV_EDITE", "SV_NEWMAP",
    "SV_SENDMAP", "SV_RECVMAP", "SV_REMOVEMAP",
    "SV_SERVMSG", "SV_ITEMLIST", "SV_WEAPCHANGE", "SV_PRIMARYWEAP",
    "SV_FLAGACTION", "SV_FLAGINFO", "SV_FLAGMSG", "SV_FLAGCNT",
    "SV_ARENAWIN",
    "SV_SETADMIN", "SV_SERVOPINFO",
    "SV_CALLVOTE", "SV_CALLVOTESUC", "SV_CALLVOTEERR", "SV_VOTE", "SV_VOTERESULT",
    "SV_SETTEAM", "SV_TEAMDENY", "SV_SERVERMODE",
    "SV_WHOIS", "SV_WHOISINFO",
    "SV_LISTDEMOS", "SV_SENDDEMOLIST", "SV_GETDEMO", "SV_SENDDEMO", "SV_DEMOPLAYBACK",
    "SV_CONNECT", "SV_SPECTCN",
    "SV_SWITCHNAME", "SV_SWITCHSKIN", "SV_SWITCHTEAM",
    "SV_CLIENT",
    "SV_EXTENSION",
    "SV_MAPIDENT", "SV_HUDEXTRAS", "SV_POINTS"
};

const char *entnames[MAXENTTYPES] =
{
    "none?",
	"light", "playerstart",	"pistol", "ammobox","grenades",
    "health", "helmet", "armour", "akimbo",
    "mapmodel", "trigger", "ladder", "ctf-flag", "sound", "clip", "plclip"
};

itemstat ammostats[NUMGUNS] =
{
    {1,  1,   1,   S_ITEMAMMO},   //knife dummy
    {16, 32,  72,  S_ITEMAMMO},   //pistol
    {15, 30,  30,  S_ITEMAMMO},   //rifle
    {14, 28,  21,  S_ITEMAMMO},   //shotgun
    {60, 90,  90,  S_ITEMAMMO},   //subgun
    {10, 20,  15,  S_ITEMAMMO},   //sniper
    {40, 60,  60,  S_ITEMAMMO},   //assault
    {30, 45,  75,  S_ITEMAMMO},   //cpistol
    {1,  0,   3,   S_ITEMAMMO},   //grenade
    {72, 0,   72,  S_ITEMAKIMBO}  //akimbo
};

itemstat powerupstats[I_ARMOUR-I_HEALTH+1] =
{
    {33, 0, 100, S_ITEMHEALTH}, // 0 health
    {25, 0, 100, S_ITEMARMOUR}, // 1 helmet
    {50, 0, 100, S_ITEMARMOUR}, // 2 armour
};

guninfo guns[NUMGUNS] =
{
    { "knife",      S_KNIFE,      S_NULL,     0,      500,    50,     0,   0,  1,    1,   1,    0,  0,    0,  0,      0,      0,    1,      false },
    { "pistol",     S_PISTOL,     S_RPISTOL,  1400,   160,    18,     0,   0, 55,   10,   8,    6,  5,    6,  35,     58,     125,  1,      false },
    { "carbine",    S_CARBINE,    S_RCARBINE, 1800,   720,    60,  40,     0,   0, 10,   60,   10,   4,  4,  10,  60,     60,     150,  1,      false },
    { "shotgun",    S_SHOTGUN,    S_RSHOTGUN, 2400,   1000,   5,      0,   0,  1,   35,   7,    9,  9,    10, 115,    115,    150,  1,      false },
    { "subgun",     S_SUBGUN,     S_RSUBGUN,  1650,   80,     15,     0,   0, 40,   15,   30,   1,  2,    4,  15,     40,     175,  1,      true },
    { "sniper",     S_SNIPER,     S_RSNIPER,  1950,   1500,   80,     0,   0, 60,   50,   5,    4,  4,    10, 85,     85,     100,  1,      false },
    { "assault",    S_ASSAULT,    S_RASSAULT, 2000,   120,    24,     0,   0, 20,   40,   20,   0,  2,    2,  18,     50,     125,  1,      true },
    { "cpistol",    S_PISTOL,     S_RPISTOL,  1400,   120,    19,     0,   0, 35,   10,   15,   6,  5,    6,  35,     58,     125,  1,      false },   // temporary
    { "grenade",    S_NULL,       S_NULL,     1000,   650,    200,    20,  6,  1,    1,   1,    3,  1,    0,  0,      0,      0,    3,      false },
    { "pistol",     S_PISTOL,     S_RAKIMBO,  1400,   80,     19,     0,   0, 45,   10,   16,   6,  5,    8,  10,     18,     150,  1,      true },
};

const char *teamnames[TEAM_NUM+1] = {"CLA", "RVSF", "CLA-SPECT", "RVSF-SPECT", "SPECTATOR", "void"};
const char *teamnames_s[TEAM_NUM+1] = {"CLA", "RVSF", "CSPC", "RSPC", "SPEC", "void"};

char killmessages[2][NUMGUNS][MAXKILLMSGLEN] = {{ "", "busted", "picked off", "peppered", "sprayed", "punctured", "shredded", "busted", "", "busted" }, { "slashed", "", "", "splattered", "", "headshot", "", "", "gibbed", "" }};
