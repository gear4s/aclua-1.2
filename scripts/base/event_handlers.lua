--[[

  Misc

]]
local pk = {}
local function sendMOTD(cn)
  if cn == nil then cn = -1 end
  messages.notice(servermotd):send(cn)
  messages.notice("blue<Type> yellow<!cmds> blue<to see available server commands> ."):send(cn)
end

--[[

  onPlayerSayText - Player text event

  Parameters:
  - cn
    - integer
      - client number
  - text
    - string
      - player text

]]
function onPlayerSayText(cn, text)
  pk[cn] = 0
  local m = block_text(cn, text)
  if m then
    return PLUGIN_BLOCK
  end

  parts = split(text, " ")

  if string.sub(parts[1], 1, 1) == "!" then
    local cmd, args = string.gsub(parts[1], "!", "", 1), string.slice(parts, 2)

    if cmd == "cmds" then
      messages.notice(cn, "blue<List of commands:>", true)
      messages.notice("yellow<%s>"):format(command.cmdlist_format(1, " - ")):send(cn)
      if isadmin(cn) then
        messages.notice("red<%s>"):format(command.cmdlist_format(2, " - ")):send(cn)
      end
    elseif command.is_valid_command(1, cmd) then
      if table.contains(enabled_commands, cmd) then
        command.load_player_command_script(1, cmd, args, cn)
      else
        messages.notice("red<Command disabled>"):send(cn)
      end
    elseif command.is_valid_command(2, cmd) then
      if not isadmin(cn) then
        messages.notice("red<Permission denied>"):send(cn)
      else
        if table.contains(enabled_commands, cmd) then
          command.load_player_command_script(2, cmd, args, cn)
        else
          messages.notice("red<Command disabled>"):send(cn)
        end
      end
    else
      messages.warning("red<Invalid command>"):send(cn)
    end
    return PLUGIN_BLOCK
  end
end

--[[

  onInit - Script initialization

]]
function onInit()
  command.load_player_command_scripts()
end

--[[

  onPlayerConnect - Player connection succeeded

  Parameters:
  - cn
    - integer
      - client number

]]
function onPlayerConnect(cn)
  pk[cn] = 0

  messages.notice("yellow<name<%s> |>blue<have|>blue<has| connected from> red<%s>"):format(cn, server.player_country(cn)):send(cn)

  sendMOTD(cn)
end

--[[

  onPlayerConnect - Player teamkill event

  Parameters:
  - cn
    - integer
      - actor client number
  - cn
    - integer
      - target client number

]]
function onPlayerTeamKill(cn, tcn)
  pk[cn] = pk[cn] + 1

  if pk[cn] < 5 then
    messages.warning("blue<This server has anti-teamkill policies! 5 maximum teamkills!>%s"):format((pk[cn] == 4) and " red<LAST WARNING!>" or ""):send(cn)
  else
    messages.notice("blue<Banned player> red<name<%s>> blue<for> green<10 minutes> blue<- Reason:> yellow<teamkill limit reached>"):send(cn)

    kb.ban(cn, 10)
  end
end

