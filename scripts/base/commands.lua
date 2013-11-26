-- command table
command = {}

--[[

  LOCAL PLAYER_COMMAND_SCRIPT_DIRECTORIES - Script directories to search for commands

  Format:
  - local PLAYER_COMMAND_SCRIPT_DIRECTORIES = {
      {["path"] = "path/to/command, ["admin"] = need_admin},
    }

]]
local PLAYER_COMMAND_SCRIPT_DIRECTORIES = {
  {["path"] = "lua/scripts/commands/", ["admin"] = false},
  {["path"] = "lua/scripts/commands/admin/", ["admin"] = true},
}

--[[

  LOCAL PLAYER_VALID_COMMANDS - List of valid commands

]]
local PLAYER_VALID_COMMANDS = {}

--[[

  command.load_player_command_script - Load a player command and execute it

  Parameters:
  - dir
    - string
      - valid filesystem directory
  - filename
    - string
      - command file name
  - args
    - table
      - list of arguments
  - cn
    - integer
      - player client number

]]
function command.load_player_command_script(dir, filename, args, cn)
  local callback, error = assert(loadfile(PLAYER_COMMAND_SCRIPT_DIRECTORIES[dir]["path"]..filename..".lua"))()
  if not callback then
    messages.notice(cn, "red<No function loaded for player command "..command..">", true)
  else
    local ret, err = callback(cn, unpack(args))
    if ret == false then
      messages.warning(cn, "red<" .. err .. ">", true)
    end
  end
end

--[[

  command.is_valid_command - Check if command is valid and exists

  Parameters:
  - table id
    - integer
      - command list id
        - accepted:
          - 1 - normal player
          - 2 - admins
  - cmd
    - string
      - command to check for

]]
function command.is_valid_command(table_id, cmd)
  return table.contains(PLAYER_VALID_COMMANDS[table_id], cmd)
end

--[[

  command.cmdlist_format - Formatted command list

  Parameters:
  - listnum
    - integer
      - command list id
        - accepted:
          - 1 - normal player
          - 2 - admins
  - delim
    - string
      - formatted id

]]
function command.cmdlist_format(listnum, delim)
  local arg = {}
  for _, command in pairs(PLAYER_VALID_COMMANDS[listnum]) do
    if table.contains(enabled_commands, command) and not table.contains(disabled_commands, command) then
      arg[#arg + 1] = command
    end
  end
  return table.concat(arg, delim)
end

--[[

  command.cmdlist - Unformatted command list

  Parameters:
  - listnum
    - integer
      - command list id
        - accepted:
          - 1 - normal player
          - 2 - admins

]]
function command.cmdlist(listnum)
  local arg = {}
  for _, command in pairs(PLAYER_VALID_COMMANDS[listnum]) do
    if table.contains(enabled_commands, command) and not table.contains(disabled_commands, command) then
      arg[#arg + 1] = command
    end
  end

  return arg
end

--[[

  command.cmdlist_format - Inserts commands into PLAYER_VALID_COMMANDS according to directories

]]
function command.load_player_command_scripts()
  for _, load_dir in pairs(PLAYER_COMMAND_SCRIPT_DIRECTORIES) do
    local dir_filename = load_dir["path"]
    local need_admin = _if(not load_dir["admin"], 1, 2)
    PLAYER_VALID_COMMANDS[need_admin] = {}

    for file_type, filename in filesystem.dir(dir_filename) do
     if file_type == filesystem.FILE and string.match(filename, ".lua$") then
        local command_name = string.sub(filename, 1, #filename - 4)
        filename = dir_filename .. "/" .. filename
        table.insert(PLAYER_VALID_COMMANDS[need_admin], command_name)
      end
    end
  end
end