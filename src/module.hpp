#ifndef LUA_MODULE_MODULE_HPP
#define LUA_MODULE_MODULE_HPP

namespace lua {
  namespace module {
    void open_geoip(lua_State * L);
    void open_filesystem(lua_State * L);
    void openAll(lua_State * L);
    void on_shutdown(lua_State * L, lua_CFunction callback);
  }
} //namespace lua

#endif

