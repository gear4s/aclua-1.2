extern "C" {
  #include "lua/lua.h"
  #include "lua/lauxlib.h"
  #include "lua/lualib.h"
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


extern int servmillis;

enum CallMethod
{
  CM_VIA_FUNCTION,
  CM_VIA_FUNCTION_REFERENCE
};
struct tmr_Timer
{
  CallMethod cm;
  bool once;

  lua_State *L;
  int id, endpoint, period;
  char function[256];
  int functionReference;
};

static tmr_Timer *timers = NULL;
static int numberOfTimers = 0;

struct TimerToCreate;
static TimerToCreate *timersToCreate = NULL;

struct TimerToCreate
{
  tmr_Timer timer;
  TimerToCreate *next;

  static void append( tmr_Timer timer )
  {
    TimerToCreate *e = new TimerToCreate;
    e->timer = timer;
    e->next = timersToCreate;
    timersToCreate = e;
  }
};

struct TimerToRemove;
static TimerToRemove *timersToRemove = NULL;

struct TimerToRemove
{
  int id;
  TimerToRemove *next;

  static void append( int id )
  {
    TimerToRemove *e = new TimerToRemove;
    e->id = id;
    e->next = timersToRemove;
    timersToRemove = e;
  }
};


volatile static bool tickLoopIsRunning = false;

void tmr_tick()
{
  tickLoopIsRunning = true;
  for ( int i = 0; i < numberOfTimers; i++ )
  {
	tmr_Timer &timer = timers[i];
	if ( servmillis >= timer.endpoint )
	{
      if ( timer.once ) TimerToRemove::append( timer.id );
	  else timer.endpoint = servmillis + timer.period;

      if (timer.cm == CM_VIA_FUNCTION)
        lua_getglobal( timer.L, timer.function );
      else // if (timers[i].cm == CM_VIA_FUNCTION_REFERENCE)
        lua_rawgeti( timer.L, LUA_REGISTRYINDEX, timer.functionReference );
      lua_pushnumber( timer.L, timer.id );
	  if ( lua_pcall( timer.L, 1, 0, 0 ) )
      {
        printf( "<Lua> Error when calling %s timer[id = %d] handler:\n%s\n", timer.function, timer.id, lua_tostring( timer.L, lua_gettop( timer.L ) ) );
        lua_pop( timer.L, 1 );
      }
	}
  }
  tickLoopIsRunning = false;

  while ( timersToRemove )
  {
    int id = timersToRemove->id;
    for ( int i = 0; i < numberOfTimers; i++ )
    {
      tmr_Timer &timer = timers[i];
      if ( timer.id == id )
      {
        if ( timer.cm == CM_VIA_FUNCTION_REFERENCE )
          luaL_unref( timer.L, LUA_REGISTRYINDEX, timer.functionReference );
        if ( i != numberOfTimers-1 )
        {
          timers[i] = timers[numberOfTimers-1];
          timers[numberOfTimers-1] = timer;
        }
        timers = ( tmr_Timer* ) realloc( timers, ( --numberOfTimers ) * sizeof( tmr_Timer ) );
      }
    }

    TimerToRemove *empty = timersToRemove;
    timersToRemove = timersToRemove->next;
    delete empty;
  }

  while ( timersToCreate )
  {
    timers = ( tmr_Timer* ) realloc( timers, ( ++numberOfTimers ) * sizeof( tmr_Timer ) );
    timers[numberOfTimers-1] = timersToCreate->timer;

    TimerToCreate *empty = timersToCreate;
    timersToCreate = timersToCreate->next;
    delete empty;
  }
}

void tmr_free()
{
  if ( numberOfTimers )
  {
	numberOfTimers = 0;
	free( timers );
	timers = NULL;
  }
}

static int tmr_create (lua_State *L)
{
  tmr_Timer timer;
  timer.once = false;
  timer.L = L;
  timer.id = luaL_checkint( L, 1 );
  timer.period = luaL_checkint( L, 2 );
  timer.endpoint = servmillis + timer.period;
  if ( lua_isstring( L, 3 ) )
  {
    strcpy( timer.function, lua_tostring( L, 3 ) );
    timer.cm = CM_VIA_FUNCTION;
  }
  else if ( lua_isfunction( L, 3 ) )
  {
    timer.functionReference = luaL_ref( timer.L, LUA_REGISTRYINDEX );
    timer.cm = CM_VIA_FUNCTION_REFERENCE;
  }

  if ( tickLoopIsRunning ) TimerToCreate::append( timer );
  else
  {
    timers = ( tmr_Timer* ) realloc( timers, ( ++numberOfTimers ) * sizeof( tmr_Timer ) );
    timers[numberOfTimers-1] = timer;
  }
  return 0;
}

static int tmr_after (lua_State *L)
{
  tmr_Timer timer;
  timer.once = true;
  timer.L = L;
  timer.id = luaL_checkint( L, 1 );
  timer.period = luaL_checkint( L, 2 );
  timer.endpoint = servmillis + timer.period;
  if ( lua_isstring( L, 3 ) )
  {
    strcpy( timer.function, lua_tostring( L, 3 ) );
    timer.cm = CM_VIA_FUNCTION;
  }
  else if ( lua_isfunction( L, 3 ) )
  {
    timer.functionReference = luaL_ref( timer.L, LUA_REGISTRYINDEX );
    timer.cm = CM_VIA_FUNCTION_REFERENCE;
  }

  if ( tickLoopIsRunning ) TimerToCreate::append( timer );
  else
  {
    timers = ( tmr_Timer* ) realloc( timers, ( ++numberOfTimers ) * sizeof( tmr_Timer ) );
    timers[numberOfTimers-1] = timer;
  }
  return 0;
}

static int tmr_remove (lua_State *L)
{
  int id = luaL_checkint( L, 1 );
  if ( tickLoopIsRunning ) TimerToRemove::append( id );
  else
  {
    for ( int i = 0; i < numberOfTimers; i++ )
    {
      tmr_Timer timer = timers[i];
      if ( timer.id == id )
      {
        if ( timer.cm == CM_VIA_FUNCTION_REFERENCE )
          luaL_unref( timer.L, LUA_REGISTRYINDEX, timer.functionReference );
        if ( i != numberOfTimers-1 )
        {
          timers[i] = timers[numberOfTimers-1];
          timers[numberOfTimers-1] = timer;
        }
        timers = ( tmr_Timer* ) realloc( timers, ( --numberOfTimers ) * sizeof( tmr_Timer ) );
      }
    }
  }
  return 0;
}

static int tmr_vacantid (lua_State *L)
{
  for ( int i = 0; i < numberOfTimers; i++ )
  {
    int p = timers[i].id - 1, n = p + 2;
    int pm = 0, nm = 0;
    for ( int j = i + 1; j < numberOfTimers; j++ )
    {
      if ( p == timers[j].id ) pm++;
      if ( n == timers[j].id ) nm++;
    }
    for ( TimerToCreate *tmrs = timersToCreate; tmrs != NULL; tmrs = tmrs->next )
    {
      if ( p == tmrs->timer.id ) pm++;
      if ( n == tmrs->timer.id ) nm++;
    }
    if ( nm == 0 ) {
      lua_pushnumber( L, n );
      return 1;
    }
    if ( pm == 0 ) {
      lua_pushnumber( L, p );
      return 1;
    }
  }

  for ( TimerToCreate *tmrs = timersToCreate; tmrs != NULL; tmrs = tmrs->next )
  {
    int p = tmrs->timer.id - 1, n = p + 2;
    int pm = 0, nm = 0;
    for ( TimerToCreate *tmrs2 = tmrs; tmrs2 != NULL; tmrs2 = tmrs2->next )
    {
      if ( p == tmrs2->timer.id ) pm++;
      if ( n == tmrs2->timer.id ) nm++;
    }
    if ( nm == 0 ) {
      lua_pushnumber( L, n );
      return 1;
    }
    if ( pm == 0 ) {
      lua_pushnumber( L, p );
      return 1;
    }
  }

  lua_pushnumber( L, 0 );
  return 1;
}


static const luaL_Reg tmr_library[] =
{
  { "create", tmr_create },
  { "remove", tmr_remove },
  { "after", tmr_after },
  { "vacantid", tmr_vacantid },
  { NULL, NULL }
};

int luaopen_tmr (lua_State *L)
{
  luaL_register( L, "tmr", tmr_library );
  return 1;
}
