--[[

  Misc

]]
local pk = {}
local function sendMOTD(cn)
  if cn == nil then cn = -1 end
  messages.notice(cn, servermotd, true)
  messages.notice(cn, "blue<Type> yellow<!cmds> blue<to see available server commands> .", true)
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
    return 4
  end

  parts = split(text, " ")

  if string.sub(parts[1], 1, 1) == "!" then
    local cmd, args = string.gsub(parts[1], "!", "", 1), string.slice(parts, 2)

    if cmd == "cmds" then
      messages.notice(cn, "blue<List of commands:>", true)
      messages.notice(cn, "yellow<"..command.cmdlist_format(1, " - ")..">", true)
      if isadmin(cn) then
        messages.notice(cn, "red<"..command.cmdlist_format(2, " - ")..">", true)
      end
    elseif command.is_valid_command(1, cmd) then
      if table.contains(enabled_commands, cmd) then
        command.load_player_command_script(1, cmd, args, cn)
      else
        messages.notice(cn, "red<Command disabled>", true)
      end
    elseif command.is_valid_command(2, cmd) then
      if not isadmin(cn) then
        messages.notice(cn, "red<Permission denied>", true)
      else
        if table.contains(enabled_commands, cmd) then
          command.load_player_command_script(2, cmd, args, cn)
        else
          messages.notice(cn, "red<Command disabled>", true)
        end
      end
    else
      messages.warning(cn, "red<Invalid command>", true)
    end
    return 4
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

  messages.notice(-1,string.format("yellow<name<%s> |>blue<have|>blue<has| connected from> red<%s>", cn, server.player_country(cn)))
  messages.notice(-2, string.format("blue<IP:> green<%s>", getip(cn)), true)

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
    messages.warning({cn}, string.format("blue<This server has anti-teamkill policies! 5 maximum teamkills!>%s", (pk[cn] == 4) and " red<LAST WARNING!>" or ""))
  else
    msg = string.format("blue<Banned player> red<name<%s>> blue<for> green<10 minutes> blue<- Reason:> yellow<teamkill limit reached>", cn)
    messages.notice(-1, msg)

    kb.ban(cn, 10)
  end
end

