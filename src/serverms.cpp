// all server side masterserver and pinging functionality

#include "cube.h"
#include "luamod.h"
#include <string>

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
}

int connectwithtimeout(ENetSocket sock, const char *hostname, ENetAddress &remoteaddress)
{
    int result = enet_socket_connect(sock, &remoteaddress);
    if(result<0) enet_socket_destroy(sock);
    return result;
}
#endif

int masterservers = 0;
ENetAddress serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };

vector<ENetSocket> mastersock;
vector<ENetAddress> masteraddress;

vector<std::string> mastername;
vector<int> masterport, mastertype;
//int masterport = AC_MASTER_PORT, mastertype = AC_MASTER_HTTP;
vector<int> lastupdatemaster;
vector<vector<char> > masterout, masterin;
vector<int> masteroutpos, masterinpos;

void addms(const char *name) {
  ENetSocket socket = ENET_SOCKET_NULL;
  mastersock.add(socket);
  ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
  masteraddress.add(addr);
  mastername.add(name);
  lastupdatemaster.add(0);
  masteroutpos.add(0);
  masterinpos.add(0);
  masterout.add();
  masterin.add();
  masterservers++;
}

void disconnectmaster(int m);
void remms(const char *name) {
  loopv(mastername) {
    if(i == 0) continue; // main master server can't be removed

    if(mastername[i] == name) {
      masterservers--;
      disconnectmaster(i);
      mastersock.remove(i);
      masteraddress.remove(i);
      mastername.remove(i);
      lastupdatemaster.remove(i);
      masteroutpos.remove(i);
      masterinpos.remove(i);
      masterout.remove(i);
      masterin.remove(i);
      break;
    }
  }
}

void disconnectmaster(int m) {
    if(mastersock[m] == ENET_SOCKET_NULL) return;

    enet_socket_destroy(mastersock[m]);
    mastersock[m] = ENET_SOCKET_NULL;

    masterout[m].setsize(0);
    masterin[m].setsize(0);
    masteroutpos[m] = masterinpos[m] = 0;

    masteraddress[m].host = ENET_HOST_ANY;
    masteraddress[m].port = ENET_PORT_ANY;

    //lastupdatemaster = 0;
}

ENetSocket connectmaster(int m)
{
    if(!mastername[m][0]) return ENET_SOCKET_NULL;
    extern servercommandline scl;
    if(scl.maxclients>MAXCL) { logline(ACLOG_WARNING, "maxclient exceeded: cannot register"); return ENET_SOCKET_NULL; }

    if(masteraddress[m].host == ENET_HOST_ANY)
    {
        logline(ACLOG_INFO, "looking up %s:%d...", mastername[m].c_str(), masterport[m]);
        masteraddress[m].port = masterport[m];
        if(!resolverwait(mastername[m].c_str(), &masteraddress[m])) return ENET_SOCKET_NULL;
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock != ENET_SOCKET_NULL && serveraddress.host != ENET_HOST_ANY && enet_socket_bind(sock, &serveraddress) < 0)
    {
        enet_socket_destroy(sock);
        sock = ENET_SOCKET_NULL;
    }
    if(sock == ENET_SOCKET_NULL || connectwithtimeout(sock, mastername[m].c_str(), masteraddress[m]) < 0)
    {
        logline(ACLOG_WARNING, sock==ENET_SOCKET_NULL ? "could not open socket" : "could not connect");
        return ENET_SOCKET_NULL;
    }

    enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
    return sock;
}

bool requestmaster(int m, const char *req)
{
    if(mastersock[m] == ENET_SOCKET_NULL)
    {
        mastersock[m] = connectmaster(m);
        if(mastersock[m] == ENET_SOCKET_NULL) return false;
    }

    masterout[m].put(req, strlen(req));
    return true;
}

bool requestmasterf(int m, const char *fmt, ...)
{
    defvformatstring(req, fmt, fmt);
    return requestmaster(m, req);
}

extern void processmasterinput(const char *cmd, int cmdlen, const char *args);

void processmasterinput(int m)
{
    if(masterinpos[m] >= masterin.length()) return;

    char *input = &masterin[m][masterinpos[m]], *end = (char *)memchr(input, '\n', masterin[m].length() - masterinpos[m]);
    while(end)
    {
        *end++ = '\0';

        const char *args = input;
        while(args < end && !isspace(*args)) args++;
        int cmdlen = args - input;
        while(args < end && isspace(*args)) args++;

        if(!strncmp(input, "failreg", cmdlen))
            logline(ACLOG_WARNING, "[%s] master server registration failed: %s", mastername[m].c_str(), args);
        else if(!strncmp(input, "succreg", cmdlen))
        {
            logline(ACLOG_INFO, "[%s] master server registration succeeded", mastername[m].c_str());
        } else {
            Lua::callHandler( LUA_ON_MASTER_SERVER_COMMAND, "s", input );
            processmasterinput(input, cmdlen, args);
            Lua::callHandler( LUA_ON_MASTER_SERVER_COMMAND_AFTER, "s", input );
        }

        masterinpos[m] = end - masterin[m].getbuf();
        input = end;
        end = (char *)memchr(input, '\n', masterin[m].length() - masterinpos[m]);
    }

    if(masterinpos[m] >= masterin[m].length())
    {
        masterin[m].setsize(0);
        masterinpos[m] = 0;
    }
}

void flushmasteroutput(int m)
{
    if(masterout[m].empty()) return;

    ENetBuffer buf;
    buf.data = &masterout[m][masteroutpos[m]];
    buf.dataLength = masterout[m].length() - masteroutpos[m];
    int sent = enet_socket_send(mastersock[m], NULL, &buf, 1);
    if(sent >= 0)
    {
        masteroutpos[m] += sent;
        if(masteroutpos[m] >= masterout[m].length())
        {
            masterout[m].setsize(0);
            masteroutpos[m] = 0;
        }
    }
    else disconnectmaster(m);
}

void flushmasterinput(int m)
{
    if(masterin[m].length() >= masterin[m].capacity())
        masterin[m].reserve(4096);

    ENetBuffer buf;
    buf.data = &masterin[m][masterin[m].length()];
    buf.dataLength = masterin[m].capacity() - masterin[m].length();
    int recv = enet_socket_receive(mastersock[m], NULL, &buf, 1);
    if(recv > 0)
    {
        masterin[m].advance(recv);
        processmasterinput(m);
    }
    else disconnectmaster(m);
}

extern char *global_name;
extern int interm;
extern int totalclients;

// send alive signal to masterserver after 40 minutes of uptime and if currently in intermission (so theoretically <= 1 hour)
// TODO?: implement a thread to drop this "only in intermission" business, we'll need it once AUTH gets active!
void updatemasterserver(int m, int millis, int port, bool fromLua = false)
{
    if(fromLua  || (!lastupdatemaster[m] || ((millis-lastupdatemaster[m])>40*60*1000 && (interm || !totalclients))))
    {
        char servername[30]; memset(servername,'\0',30); filtertext(servername,global_name,-1,20);
        if(mastername[m][0]) requestmasterf(m, "regserv %d %s %d\n", port, servername[0] ? servername : "noname", AC_VERSION);
        lastupdatemaster[m] = millis + 1;
        if ( !fromLua ) Lua::callHandler( LUA_ON_MASTER_SERVER_UPDATE, "s", mastername[m].c_str() );
    }
}

ENetSocket pongsock = ENET_SOCKET_NULL, lansock = ENET_SOCKET_NULL;
extern int getpongflags(enet_uint32 ip);

void serverms(int m, int mode, int numplayers, int minremain, char *smapname, int millis, const ENetAddress &localaddr, int *mnum, int *msend, int *mrec, int *cnum, int *csend, int *crec, int protocol_version)
{
    flushmasteroutput(m);
    updatemasterserver(m, millis, localaddr.port);

    static ENetSocketSet sockset;
    ENET_SOCKETSET_EMPTY(sockset);
    ENetSocket maxsock = pongsock;
    ENET_SOCKETSET_ADD(sockset, pongsock);
    if(mastersock[m] != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, mastersock[m]);
        ENET_SOCKETSET_ADD(sockset, mastersock[m]);
    }
    if(lansock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, lansock);
        ENET_SOCKETSET_ADD(sockset, lansock);
    }
    if(enet_socketset_select(maxsock, &sockset, NULL, 0) <= 0) return;

    // reply all server info requests
    static uchar data[MAXTRANS];
    ENetBuffer buf;
    ENetAddress addr;
    buf.data = data;
    int len;

    loopi(2)
    {
        ENetSocket sock = i ? lansock : pongsock;
        if(sock == ENET_SOCKET_NULL || !ENET_SOCKETSET_CHECK(sockset, sock)) continue;

        buf.dataLength = sizeof(data);
        len = enet_socket_receive(sock, &addr, &buf, 1);
        if(len < 0) continue;

        // ping & pong buf
        ucharbuf pi(data, len), po(&data[len], sizeof(data)-len);
        bool std = false;
        if(getint(pi) != 0) // std pong
        {
            extern struct servercommandline scl;
            extern string servdesc_current;
            (*mnum)++; *mrec += len; std = true;
            putint(po, protocol_version);
            putint(po, mode);
            numplayers = ( numplayers < MAXCL ) ? numplayers : ( MAXCL - 1 );
            double *var = ( double* ) Lua::getFakeVariable( LUA_YIELD_NUMBER_OF_PLAYERS, NULL, LUA_TNUMBER, "i", numplayers );
            if ( var != NULL )
            {
              numplayers = (int) var[0];
              delete[] var;
            }
            putint(po, numplayers);
            putint(po, minremain);
            sendstring(smapname, po);
            sendstring(servdesc_current, po);
            //putint(po, scl.maxclients);
            putint(po, ( scl.maxclients <= MAXCL ) ? scl.maxclients : MAXCL);
            putint(po, getpongflags(addr.host));
            if(pi.remaining())
            {
                int query = getint(pi);
                switch(query)
                {
                    case EXTPING_NAMELIST:
                    {
                        extern void extping_namelist(ucharbuf &p);
                        putint(po, query);
                        extping_namelist(po);
                        break;
                    }
                    case EXTPING_SERVERINFO:
                    {
                        extern void extping_serverinfo(ucharbuf &pi, ucharbuf &po);
                        putint(po, query);
                        extping_serverinfo(pi, po);
                        break;
                    }
                    case EXTPING_MAPROT:
                    {
                        extern void extping_maprot(ucharbuf &po);
                        putint(po, query);
                        extping_maprot(po);
                        break;
                    }
                    case EXTPING_UPLINKSTATS:
                    {
                        extern void extping_uplinkstats(ucharbuf &po);
                        putint(po, query);
                        extping_uplinkstats(po);
                        break;
                    }
                    case EXTPING_NOP:
                    default:
                        putint(po, EXTPING_NOP);
                        break;
                }
            }
        }
        else // ext pong - additional server infos
        {
            (*cnum)++; *crec += len;
            int extcmd = getint(pi);
            putint(po, EXT_ACK);
            putint(po, EXT_VERSION);

            switch(extcmd)
            {
                case EXT_UPTIME:        // uptime in seconds
                {
                    putint(po, uint(millis)/1000);
                    break;
                }

                case EXT_PLAYERSTATS:   // playerstats
                {
                    int cn = getint(pi);     // get requested player, -1 for all
                    if(!valid_client(cn) && cn != -1)
                    {
                        putint(po, EXT_ERROR);
                        break;
                    }
                    putint(po, EXT_ERROR_NONE);              // add no error flag

                    int bpos = po.length();                  // remember buffer position
                    putint(po, EXT_PLAYERSTATS_RESP_IDS);    // send player ids following
                    extinfo_cnbuf(po, cn);
                    *csend += int(buf.dataLength = len + po.length());
                    enet_socket_send(pongsock, &addr, &buf, 1); // send all available player ids
                    po.len = bpos;

                    extinfo_statsbuf(po, cn, bpos, pongsock, addr, buf, len, csend);
                    return;
                }

                case EXT_TEAMSCORE:
                    extinfo_teamscorebuf(po);
                    break;

                default:
                    putint(po,EXT_ERROR);
                    break;
            }
        }

        buf.dataLength = len + po.length();
        enet_socket_send(pongsock, &addr, &buf, 1);
        if(std) *msend += (int)buf.dataLength;
        else *csend += (int)buf.dataLength;
    }

    if(mastersock[m] != ENET_SOCKET_NULL && ENET_SOCKETSET_CHECK(sockset, mastersock[m])) flushmasterinput(m);
}

// this function should be made better, because it is used just ONCE (no need of so much parameters)
void servermsinit(int m, const char *master, const char *ip, int infoport, bool listen)
{
    mastername[m] = master;
    disconnectmaster(m);

    if(m != 0) return;

    if(listen)
    {
        ENetAddress address = { ENET_HOST_ANY, (enet_uint16)infoport };
        if(*ip)
        {
            if(enet_address_set_host(&address, ip)<0) logline(ACLOG_WARNING, "server ip not resolved");
            else serveraddress.host = address.host;
        }
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pongsock != ENET_SOCKET_NULL && enet_socket_bind(pongsock, &address) < 0)
        {
            enet_socket_destroy(pongsock);
            pongsock = ENET_SOCKET_NULL;
        }
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket");
        else enet_socket_set_option(pongsock, ENET_SOCKOPT_NONBLOCK, 1);
        address.port = CUBE_SERVINFO_PORT_LAN;
        lansock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(lansock != ENET_SOCKET_NULL && (enet_socket_set_option(lansock, ENET_SOCKOPT_REUSEADDR, 1) < 0 || enet_socket_bind(lansock, &address) < 0))
        {
            enet_socket_destroy(lansock);
            lansock = ENET_SOCKET_NULL;
        }
        if(lansock == ENET_SOCKET_NULL) logline(ACLOG_WARNING, "could not create LAN server info socket");
        else enet_socket_set_option(lansock, ENET_SOCKOPT_NONBLOCK, 1);
    }
}
