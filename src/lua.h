#ifndef __LUA_H__
#define __LUA_H__

extern "C" {
  #include "lua/lua.h"
  #include "lua/lauxlib.h" // required for luaL_Reg
}

#define LUA_SCRIPTS_PATH "lua/scripts/"
#define LUA_INCLUDES_PATH "lua/include/"

#include "lua_handlers.h"
#include "lua_generators.h"
#include "lua_tmr_library.h"
#include "lua_cfg_library.h"

#define LUA_FUNCTION_PREFIX(prefix, function) int prefix##function (lua_State *L)
#define LUA_FUNCTION(function) LUA_FUNCTION_PREFIX(__, function)

struct LuaArg;

class Lua {
  public:
    static const double version; // = Server Release Version

    static void initialize( const char *scriptsPath );
    static void destroy();

    // Full group of methods, implemented in Lua-script API Lua fashion {
    static void registerGlobals( lua_State *L );
    static void openExtraLibs( lua_State *L );
    // }

    static int callHandler( const char *handler, const char *arguments, ... );
    static int callHandler( const char *handler, int argc, const LuaArg *args );
    static void* getFakeVariable( const char *generator, int *numberOfReturns, int luaTypeReturned, const char* arguments, ... );
    static void* getFakeVariable( const char *generator, int *numberOfReturns, int luaTypeReturned, int argc, const LuaArg *args );

    static const int PLUGIN_BLOCK = 4;

    static int numberOfScripts;
    static lua_State **scripts;
};

struct LuaArg
{
  char type;
  int count, origin; // only used if (type == 'o')

  union
  {
    int i;
    const char *ccp;
    bool b;
    double d;
    lua_State *lsp;
    bool *bp;
    int luatbl;
  };
};

#endif
