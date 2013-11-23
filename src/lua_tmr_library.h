extern "C" {
  #include "lua/lua.h"
}

LUALIB_API int luaopen_tmr (lua_State *L);
LUALIB_API void tmr_tick();
LUALIB_API void tmr_free();
