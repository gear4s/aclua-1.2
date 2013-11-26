#ifndef LUA_MODULE_MODULE_HPP
#define LUA_MODULE_MODULE_HPP

#include "luamod.h"

namespace lua {
  namespace module {
    void on_shutdown(lua_State * L, lua_CFunction callback) {
      lua_newuserdata(L, 1);
      lua_newtable(L);
      lua_pushliteral(L, "__gc");
      lua_pushcfunction(L, callback);
      lua_settable(L, -3);
      lua_setmetatable(L, -2);
      luaL_ref(L, LUA_REGISTRYINDEX);
    }
  }
} //namespace lua

#endif

