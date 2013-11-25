--[[

  kickban.lua - Kickban scripts

]]

kb = {}

--[[

  kb.kick - Kick a player from the server

  Parameters:
  - tcn
    - integer
      - player client number

]]
function kb.kick(tcn)
  n = tonumber(tcn)

  if getip(n) ~= nil then
    disconnect(n, DISC_MKICK)
    return true
  else
    return false, "Invalid CN"
  end
end

--[[

  kb.ban - Kick a player from the server

  - tcn
    - integer
      - player client number

]]
function kb.ban(tcn, btime)
  n = tonumber(tcn)

  if getip(n) ~= nil then
    ban(n, btime)
    return true
  else
    return false, "Invalid CN"
  end
end
