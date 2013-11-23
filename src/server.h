// server.h
#ifndef __SERVER_H__
#define __SERVER_H__

#define gamemode smode   // allows the gamemode macros to work with the server mode

#define SERVER_PROTOCOL_VERSION    (PROTOCOL_VERSION)    // server without any gameplay modification
//#define SERVER_PROTOCOL_VERSION   (-PROTOCOL_VERSION)  // server with gameplay modification but compatible to vanilla client (using /modconnect)
//#define SERVER_PROTOCOL_VERSION  (PROTOCOL_VERSION)    // server with incompatible protocol (change PROTOCOL_VERSION in file protocol.h to a negative number!)

#define valid_flag(f) (f >= 0 && f < 2)

enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_AKIMBO, GE_RELOAD, GE_SUICIDE, GE_PICKUP };
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

extern int smode, servmillis;

struct shotevent
{
    int type;
    int millis, id;
    int gun;
    float from[3], to[3];
};

struct explodeevent
{
    int type;
    int millis, id;
    int gun;
};

struct hitevent
{
    int type;
    int target;
    int lifesequence;
    union
    {
        int info;
        float dist;
    };
    float dir[3];
};

struct suicideevent
{
    int type;
};

struct pickupevent
{
    int type;
    int ent;
};

struct akimboevent
{
    int type;
    int millis, id;
};

struct reloadevent
{
    int type;
    int millis, id;
    int gun;
};

union gameevent
{
    int type;
    shotevent shot;
    explodeevent explode;
    hitevent hit;
    suicideevent suicide;
    pickupevent pickup;
    akimboevent akimbo;
    reloadevent reload;
};

template <int N>
struct projectilestate
{
    int projs[N];
    int numprojs;

    projectilestate() : numprojs(0) {}

    void reset() { numprojs = 0; }

    void add(int val)
    {
        if(numprojs>=N) numprojs = 0;
        projs[numprojs++] = val;
    }

    bool remove(int val)
    {
        loopi(numprojs) if(projs[i]==val)
        {
            projs[i] = projs[--numprojs];
            return true;
        }
        return false;
    }
};

static const int DEATHMILLIS = 300;

struct clientstate : playerstate
{
    vec o, vel;
    bool onfloor, onladder, crouching, scoping; /// Lua mod
    int state;
    int lastdeath, lastspawn, spawn, lifesequence;
    bool forced;
    int lastshot;
    projectilestate<8> grenades;
    int akimbomillis;
    bool scoped;
    int flagscore, frags, teamkills, deaths, shotdamage, damage, points, events, lastdisc, reconnections;

    clientstate() : state(CS_DEAD) {}

    bool isalive(int gamemillis)
    {
        return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
    }

    bool waitexpired(int gamemillis)
    {
        int wait = gamemillis - lastshot;
        loopi(NUMGUNS) if(wait < gunwait[i]) return false;
        return true;
    }

    void reset()
    {
        onfloor = false;
        vel.x = vel.y = vel.z = 0.0f;
        state = CS_DEAD;
        lifesequence = -1;
        grenades.reset();
        akimbomillis = 0;
        scoped = forced = false;
        flagscore = frags = teamkills = deaths = shotdamage = damage = points = events = lastdisc = reconnections = 0;
        respawn();
    }

    void respawn()
    {
        playerstate::respawn();
        o = vec(-1e10f, -1e10f, -1e10f);
        lastdeath = 0;
        lastspawn = -1;
        spawn = 0;
        lastshot = 0;
        akimbomillis = 0;
        scoped = false;
    }
};

struct savedscore
{
    string name;
    uint ip;
    int frags, flagscore, deaths, teamkills, shotdamage, damage, team, points, events, lastdisc, reconnections;
    bool valid, forced;

    void reset()
    {
        // to avoid 2 connections with the same score... this can disrupt some laggers that eventually produces 2 connections (but it is rare)
        frags = flagscore = deaths = teamkills = shotdamage = damage = points = events = lastdisc = reconnections = 0;
    }

    void save(clientstate &cs, int t)
    {
        frags = cs.frags;
        flagscore = cs.flagscore;
        deaths = cs.deaths;
        teamkills = cs.teamkills;
        shotdamage = cs.shotdamage;
        damage = cs.damage;
        points = cs.points;
        forced = cs.forced;
        events = cs.events;
        lastdisc = cs.lastdisc;
        reconnections = cs.reconnections;
        team = t;
        valid = true;
    }

    void restore(clientstate &cs)
    {
        cs.frags = frags;
        cs.flagscore = flagscore;
        cs.deaths = deaths;
        cs.teamkills = teamkills;
        cs.shotdamage = shotdamage;
        cs.damage = damage;
        cs.points = points;
        cs.forced = forced;
        cs.events = events;
        cs.lastdisc = lastdisc;
        cs.reconnections = reconnections;
        reset();
    }
};

struct medals
{
    int dpt, lasthit, lastgun, ncovers, nhs;
    int combohits, combo, combofrags, combotime, combodamage, ncombos;
    int ask, askmillis, linked, linkmillis, linkreason, upmillis, flagmillis;
    int totalhits, totalshots;
    bool updated, combosend;
    vec pos, flagpos;
    void reset()
    {
        dpt = lasthit = lastgun = ncovers = nhs = 0;
        combohits = combo = combofrags = combotime = combodamage = ncombos = 0;
        askmillis = linkmillis = upmillis = flagmillis = 0;
        linkreason = linked = ask = -1;
        totalhits = totalshots = 0;
        updated = combosend = false;
        pos = flagpos = vec(-1e10f, -1e10f, -1e10f);
    }
};

struct client                   // server side version of "dynent" type
{
    bool connecting; /// Lua mod
    int type;
    int clientnum;
    ENetPeer *peer;
    string hostname;
    string name;
    int team;
    char lang[3];
    int ping;
    int skin[2];
    int vote;
    int role;
    int connectmillis, lmillis, ldt, spj;
    int mute, spam, lastvc; // server side voice comm spam control
    int acversion, acbuildtype;
    bool isauthed; // for passworded servers
    bool haswelcome;
    bool isonrightmap, loggedwrongmap, freshgame;
    bool timesync;
    int overflow;
    int gameoffset, lastevent, lastvotecall;
    int lastprofileupdate, fastprofileupdates;
    int demoflags;
    clientstate state;
    vector<gameevent> events;
    vector<uchar> position, messages;
    string lastsaytext;
    int saychars, lastsay, spamcount, badspeech, badmillis;
    int at3_score, at3_lastforce, eff_score;
    bool at3_dontmove;
    int spawnindex;
    int spawnperm, spawnpermsent;
    int salt;
    string pwd;
    uint authreq; // for AUTH
    string authname; // for AUTH
    int mapcollisions, farpickups;
    enet_uint32 bottomRTT;
    medals md;
    bool upspawnp;
    int lag;
    vec spawnp;
    int nvotes;
    int input, inputmillis;
    int ffire, wn, f, g, t, y, p;
    int yb, pb, oy, op, lda, ldda, fam;
    int nt[10], np, lp, ls, lsm, ld, nd, nlt, lem, led;
    vec cp[10], dp[10], d0, lv, lt, le;
    float dda, tr, sda;
    int ps, ph, tcn, bdt, pws;
    float pr;
    int yls, pls, tls;
    int bs, bt, blg, bp;

    gameevent &addevent()
    {
        static gameevent dummy;
        if(events.length()>100) return dummy;
        return events.add();
    }

    void mapchange(bool getmap = false)
    {
        state.reset();
        events.setsize(0);
        overflow = 0;
        timesync = false;
        isonrightmap = m_coop;
        spawnperm = SP_WRONGMAP;
        spawnpermsent = servmillis;
        if(!getmap)
        {
            loggedwrongmap = false;
            freshgame = true;         // permission to spawn at once
        }
        lastevent = 0;
        at3_lastforce = eff_score = 0;
        mapcollisions = farpickups = 0;
        md.reset();
        upspawnp = false;
        lag = 0;
        spawnp = vec(-1e10f, -1e10f, -1e10f);
        lmillis = ldt = spj = 0;
        ffire = 0;
        f = g = y = p = t = 0;
        yb = pb = oy = op = lda = ldda = fam = 0;
        np = lp = ls = lsm = ld = nd = nlt = lem = led = 0;
        d0 = lv = lt = le = vec(0,0,0);
        loopi(10) { cp[i] = dp[i] = vec(0,0,0); nt[i] = 0; }
        dda = tr = sda = 0;
        ps = ph = bdt = pws = 0;
        tcn = -1;
        pr = 0.0f;
        yls = pls = tls = 0;
    }

    void reset()
    {
        name[0] = pwd[0] = demoflags = 0;
        bottomRTT = ping = 9999;
        team = TEAM_SPECT;
        state.state = CS_SPECTATE;
        loopi(2) skin[i] = 0;
        position.setsize(0);
        messages.setsize(0);
        isauthed = haswelcome = false;
        role = CR_DEFAULT;
        lastvotecall = 0;
        lastprofileupdate = fastprofileupdates = 0;
        vote = VOTE_NEUTRAL;
        lastsaytext[0] = '\0';
        saychars = 0;
        spawnindex = -1;
        authreq = 0; // for AUTH
        mapchange();
        freshgame = false;         // don't spawn into running games
        mute = spam = lastvc = badspeech = badmillis = nvotes = 0;
        input = inputmillis = 0;
        wn = -1;
        bs = bt = blg = bp = 0;
    }

    void zap()
    {
        type = ST_EMPTY;
        role = CR_DEFAULT;
        isauthed = haswelcome = false;
    }
};

struct ban
{
    ENetAddress address;
    int millis, type;
};

struct worldstate
{
    enet_uint32 uses;
    vector<uchar> positions, messages;
};

struct server_entity            // server side version of "entity" type
{
    int type;
    bool spawned, legalpickup, twice;
    int spawntime;
    short x, y;
    short z; /// Lua mod
};

struct clientidentity
{
    uint ip;
    int clientnum;
};

struct demofile
{
    string info;
    string file;
    uchar *data;
    int len;
    vector<clientidentity> clientssent;
};

void startgame(const char *newname, int newmode, int newtime = -1, bool notify = true, bool fromLua = false);
void disconnect_client(int n, int reason = -1, bool fromLua = false);
void sendiplist(int receiver, int cn = -1);
int clienthasflag(int cn);
bool refillteams(bool now = false, int ftr = FTR_AUTOTEAM);
void changeclientrole(int client, int role, char *pwd = NULL, bool force=false);
mapstats *getservermapstats(const char *mapname, bool getlayout = false, int *maploc = NULL);
int findmappath(const char *mapname, char *filename = NULL);
int calcscores();
void recordpacket(int chan, void *data, int len);
void senddisconnectedscores(int cn);
void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(packetbuf &p, int n);
void sendwelcome(client *cl, int chan = 1);
void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1, bool demopacket = false);
int numclients();
bool updateclientteam(int cln, int newteam, int ftr);
void forcedeath(client *cl);
void sendf(int cn, int chan, const char *format, ...);

extern bool isdedicated;
extern string smapname;
extern mapstats smapstats;
extern char *maplayout;

/// Lua mod
enum UploadError
{
  UE_NOERROR = -1,
  UE_READONLY,
  UE_LIMITACHIEVED,
  UE_NOUPLOADPERMISSION,
  UE_NOUPDATEPERMISSION,
  UE_NOREVERTPERMISSION
};
enum RemoveError
{
  RE_NOERROR = -1,
  RE_NOPERMISSION,
  RE_READONLY,
  RE_NOTFOUND
};

extern char killmessages[2][NUMGUNS][MAXKILLMSGLEN];

#endif
