extern "C" {
  #include "lua/lua.h"
  #include "lua/lauxlib.h"
  #include "lua/lualib.h"
}

#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#define CFG_PATH "lua/config/"
#define CFG_SEP "[Lua Config File]"


typedef char cfg_F[256];

static void cfg_filename( char *dst, const char *src )
{
  strcpy( dst, CFG_PATH );
  strcat( dst, src );
  strcat( dst, ".cfg" );
}

static int cfg_createfile (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 1 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  std::ifstream in;
  in.open( filename );
  if ( in.is_open() ) in.close();
  else
  {
	std::ofstream out;
	out.open( filename, std::ios_base::out | std::ios_base::trunc );
	out << CFG_SEP << std::endl;
	out.close();
  }
  return 0;
}

static void replace_with(std::string & src, const std::string & what, const std::string & with)
{
    if (what != with) {
        std::string temp;
        std::string::size_type prev_pos = 0, pos = src.find(what, 0);
        while ( std::string::npos != pos ) {
            temp += std::string(src.begin() + prev_pos, src.begin() + pos) + with;
            prev_pos = pos + what.size();
            pos = src.find(what, prev_pos);
        }
        if ( !temp.empty() ) {
            src = temp + std::string(src.begin() + prev_pos, src.end());
            if (std::string::npos == with.find(what)) {
                replace_with(src, what, with);
            }
        }
    }
}
static int cfg_getvalue (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 2 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  const char *key = luaL_checkstring( L, 2 );
  std::ifstream in;
  in.open( filename );
  if ( in.is_open() )
  {
	bool executing = false, keyFound = false;
	std::string line;
	while ( getline( in, line ) )
	{
      if ( *(line.end() - 1) == '\r' ) line.erase( line.end() - 1 );
	  if ( !executing ) executing = ( line.compare( CFG_SEP ) == 0 );
	  else
	  {
		std::vector<std::string> tokens;
		std::stringstream ss( line ); std::string item;
		while ( getline( ss, item, '=' ) ) tokens.push_back( item );
		if ( tokens.size() == 0 ) continue;
        for (;;)
        {
          int slashes = 0;
          for ( std::string::reverse_iterator rit = tokens[0].rbegin(); rit < tokens[0].rend(); rit++ )
          {
            if ( (*rit) == '\\' ) ++slashes;
            else break;
          }
          if (slashes % 2 == 1) // один лишний слеш, относящийся к =
          {
            tokens[0].erase( tokens[0].end() - 1 );
            tokens[0] += '=';
            tokens[0] += tokens[1];
            tokens.erase( tokens.begin() + 1 );
          }
          else break;
        }
        replace_with( tokens[0], "\\\\", "\\" );
        if ( *(line.end() - 1) == '=' ) tokens.push_back( "" );
		if ( tokens[0].compare( key ) == 0 )
		{
		  std::string value = tokens[1];
		  for ( unsigned int i = 2; i < tokens.size(); i++ ) value.append( "=" ).append( tokens[i] );
		  lua_pushstring( L, value.c_str() );
		  keyFound = true; break;
		}
	  }
	}
	in.close();
	if ( !keyFound ) lua_pushnil( L );
  }
  else lua_pushnil( L );
  return 1;
}

static int cfg_setvalue (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 3 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  const char *key = luaL_checkstring( L, 2 ), *value = luaL_checkstring( L, 3 );
  std::stringstream ss;
  std::ifstream in;
  in.open( filename );
  bool keyFound = false;
  if ( in.is_open() )
  {
	bool executing = false;
	std::string line;
	while ( getline( in, line ) )
	{
	  if ( !executing ) executing = ( line.compare( CFG_SEP ) == 0 );
	  else
	  {
		std::vector<std::string> tokens;
		std::stringstream ss( line ); std::string item;
		while ( getline( ss, item, '=' ) ) tokens.push_back( item );
		if ( tokens.size() == 0 ) continue;
        for (;;)
        {
          int slashes = 0;
          for ( std::string::reverse_iterator rit = tokens[0].rbegin(); rit < tokens[0].rend(); rit++ )
          {
            if ( (*rit) == '\\' ) ++slashes;
            else break;
          }
          if (slashes % 2 == 1) // один лишний слеш, относящийся к =
          {
            tokens[0].erase( tokens[0].end() - 1 );
            tokens[0] += '=';
            tokens[0] += tokens[1];
            tokens.erase( tokens.begin() + 1 );
          }
          else break;
        }
        replace_with( tokens[0], "\\\\", "\\" );
        //if ( *(line.end() - 1) == '=' ) tokens.push_back( "" );
		if ( tokens[0].compare( key ) == 0 )
		{
		  line = key;
          replace_with( line, "\\", "\\\\" );
          replace_with( line, "=", "\\=" );
		  line.append( "=" ).append( value );
		  keyFound = true;
		}
	  }
	  ss << line << std::endl;
	}
	in.close();
  }
  else ss << CFG_SEP << std::endl;
  if ( !keyFound )
  {
    std::string key2 = key;
    replace_with( key2, "\\", "\\\\" );
    replace_with( key2, "=", "\\=" );
    ss << key2 << "=" << value << std::endl;
  }
  std::ofstream out;
  out.open( filename, std::ios_base::out | std::ios_base::trunc );
  out << ss.str();
  out.close();
  return 0;
}

static int cfg_vars_iterator (lua_State *L)
{
  std::ifstream **in = ( std::ifstream** ) lua_touserdata( L, lua_upvalueindex( 1 ) );
  if ( *in )
  {
    if ( (*in)->is_open() )
    {
      std::string line;
      while ( getline( **in, line ) )
      {
        if ( *(line.end() - 1) == '\r' ) line.erase( line.end() - 1 );

        std::vector<std::string> tokens;
        std::stringstream ss( line ); std::string item;
        while ( getline( ss, item, '=' ) ) tokens.push_back( item );
        if ( tokens.size() == 0 ) continue;
        for (;;)
        {
          int slashes = 0;
          for ( std::string::reverse_iterator rit = tokens[0].rbegin(); rit < tokens[0].rend(); rit++ )
          {
            if ( (*rit) == '\\' ) ++slashes;
            else break;
          }
          if (slashes % 2 == 1) // один лишний слеш, относящийся к =
          {
            tokens[0].erase( tokens[0].end() - 1 );
            tokens[0] += '=';
            tokens[0] += tokens[1];
            tokens.erase( tokens.begin() + 1 );
          }
          else break;
        }
        replace_with( tokens[0], "\\\\", "\\" );
        //if ( *(line.end() - 1) == '=' ) tokens.push_back( "" );

        lua_pushstring( L, tokens[0].c_str() );
        return 1;
      }
      (*in)->close();
    }
    delete *in;
    *in = NULL;
  }
  return 0;
}
static int cfg_vars (lua_State *L)
{
  lua_checkstack( L, 1 );
  cfg_F filename; cfg_filename( filename, luaL_checkstring( L, 1 ) );
  std::ifstream **in = ( std::ifstream** ) lua_newuserdata( L, sizeof( std::ifstream* ) );
  *in = new std::ifstream;
  (*in)->open( filename );
  if ( (*in)->is_open() )
  {
    bool executing = false;
    std::string line;
    while ( !executing && getline( **in, line ) )
    {
      if ( *(line.end() - 1) == '\r' ) line.erase( line.end() - 1 );
      if ( !executing ) executing = ( line.compare( CFG_SEP ) == 0 );
    }
    if ( !executing )
    {
      (*in)->close();
      delete *in;
      *in = NULL;
    }
  }
  else
  {
    delete *in;
    *in = NULL;
  }
  luaL_getmetatable( L, "std::ifstream*" );
  lua_setmetatable( L, -2 );
  lua_pushcclosure( L, cfg_vars_iterator, 1 );
  return 1;
}
static int cfg_gc (lua_State *L)
{
  std::ifstream **in = ( std::ifstream** ) luaL_checkudata( L, 1, "std::ifstream*" );
  if ( *in )
  {
    if ( (*in)->is_open() ) (*in)->close();
    delete *in;
    *in = NULL;
  }
  return 0;
}

static int cfg_exists (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 1 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  std::ifstream in;
  in.open( filename );
  if ( in.is_open() )
  {
    lua_pushboolean( L, 1 );
    in.close();
  }
  else lua_pushboolean( L, 0 );
  return 1;
}

static int cfg_totable (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 1 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  std::ifstream in;
  in.open( filename );
  if ( in.is_open() )
  {
    lua_newtable( L );
    bool executing = false;
    std::string line;
    while ( getline( in, line ) )
    {
      if ( *(line.end() - 1) == '\r' ) line.erase( line.end() - 1 );
      if ( !executing ) executing = ( line.compare( CFG_SEP ) == 0 );
      else
      {
        std::vector<std::string> tokens;
        std::stringstream ss( line ); std::string item;
        while ( getline( ss, item, '=' ) ) tokens.push_back( item );
        if ( tokens.size() == 0 ) continue;
        for (;;)
        {
          int slashes = 0;
          for ( std::string::reverse_iterator rit = tokens[0].rbegin(); rit < tokens[0].rend(); rit++ )
          {
            if ( (*rit) == '\\' ) ++slashes;
            else break;
          }
          if (slashes % 2 == 1) // один лишний слеш, относящийся к =
          {
            tokens[0].erase( tokens[0].end() - 1 );
            tokens[0] += '=';
            tokens[0] += tokens[1];
            tokens.erase( tokens.begin() + 1 );
          }
          else break;
        }
        replace_with( tokens[0], "\\\\", "\\" );
        if ( *(line.end() - 1) == '=' ) tokens.push_back( "" );

        std::string value = tokens[1];
        for ( unsigned int i = 2; i < tokens.size(); i++ ) value.append( "=" ).append( tokens[i] );
        lua_pushstring( L, value.c_str() );
        lua_setfield( L, 2, tokens[0].c_str() );
      }
    }
    in.close();
  }
  else lua_pushnil( L );
  return 1;
}

static int cfg_fromtable (lua_State *L)
{
  cfg_F filename;
  lua_checkstack( L, 2 );
  cfg_filename( filename, luaL_checkstring( L, 1 ) );
  std::stringstream ss;
  std::ifstream in;

  in.open( filename );
  if ( in.is_open() )
  {
    bool executing = false;
    while ( !executing )
    {
      std::string line;
      getline( in, line );
      executing = ( line.compare( CFG_SEP ) == 0 );
      ss << line << std::endl;
    }
    in.close();
  }

  lua_pushnil( L );
  while ( lua_next( L, 2 ) != 0 )
  {
    std::string key = lua_tostring( L, -2 );
    replace_with( key, "\\", "\\\\" );
    replace_with( key, "=", "\\=" );
    ss << key << "=" << lua_tostring( L, -1 ) << std::endl;
    lua_pop( L, 1 );
  }

  std::ofstream out;
  out.open( filename, std::ios_base::out | std::ios_base::trunc );
  out << ss.str();
  out.close();
  return 0;
}


static const luaL_Reg cfg_library[] =
{
  { "createfile", cfg_createfile },
  { "getvalue", cfg_getvalue },
  { "setvalue", cfg_setvalue },
  { "vars", cfg_vars },
  { "exists", cfg_exists },
  { "totable", cfg_totable },
  { "fromtable", cfg_fromtable },
  { NULL, NULL }
};

static const luaL_Reg cfg_udata_methods[] =
{
  { "__gc", cfg_gc },
  { NULL, NULL }
};

int luaopen_cfg (lua_State *L)
{
  luaL_newmetatable( L, "std::ifstream*" );
  luaL_register( L, NULL, cfg_udata_methods );
  lua_pop( L, 1 );

  luaL_register( L, "cfg", cfg_library );
  return 1;
}
