#ifndef HOPMOD_LUA_MODULE_MODULE_HPP
#define HOPMOD_LUA_MODULE_MODULE_HPP

namespace lua {
  namespace module {
    void open_geoip(lua_State * L);
    void open_filesystem(lua_State * L);

    void openAll(lua_State * L) {
      open_geoip(L);
      open_filesystem(L);
    }
  }
} //namespace lua

#endif

