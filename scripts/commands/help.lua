--[[

  "help" command

  Author: Aaron Lee "gear" Marais

]]

--[[

  Help definitions

  Format:
  - helpdef = {
    ["command"] = {need_admin, "Definition", {{"required", "arguments"}, {"non-required", "arguments"}}
  }

]]
local helpdef = {
  -- normal commands below
  ["stats"] = {false, "View your current stats (e.g. frags)", {false, {"cn"}}},
  ["whois"] = {false, "Get country and (if admin) IP of a certain player", {false, {"cn"}}},
  ["help"] = {false, "Get help on specific commands", {false, {"command"}}},
  -- admin commands below
  ["kick"] = {true, "Kick a player from the server", {{"cn"}, {"text"}}},
  ["ban"] = {true, "Ban a player from the server", {{"cn"}, {"text"}}},
  ["mute"] = {true, "Mute a player on the server", {{"cn"}, {"text"}}},
  ["mute"] = {true, "Unmute a player on the server", {{"cn"}, false}},
  ["say"] = {true, "Print text out to the whole server", {{"text"}, false}},
}

return function(cn, command)
  if command == nil then
    messages.notice("blue<Provides information on commands. For a list of commands type> yellow<\"!cmds\">."):send(cn)
    messages.notice("blue<Usage:> yellow<!help {command}>"):send(cn)
  else
    if helpdef[command] ~= nil then
      local cmdpat = nil
      local needadmin, def, usage = helpdef[command][1], helpdef[command][2], helpdef[command][3]

      if (isadmin(cn) or not needadmin) then
        if usage[1] ~= false or usage[2] ~= false then
          cmdpat = string.format(" blue<- usage:> %s ", command)
          if usage[1] ~= false then
            cmdpat = cmdpat .. string.format("blue<(%s)>", table.concat(usage[1], ") ("))
          end

          if usage[2] ~= false then
            cmdpat = cmdpat .. string.format(" grey<[%s]>", table.concat(usage[2], "] ["))
          end
        end

        messages.notice("blue<Help on the \">yellow<%s>blue<\" command:> green<%s>%s"):format(command, def, cmdpat ~= nil and cmdpat or ""):send(cn)
      else
        return false, string.format("Administrator privileges is> blue<required> red<to retrieve help on the %s command.", command)
      end
    else
      return false, string.format("No help information for command> blue<%s", command)
    end
  end
end

