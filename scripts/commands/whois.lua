return function (cn, tcn)
  local n = ""
  if tcn == nil then
    n = cn
  else
    n = tonumber(tcn)
    if getip(n) == nil then
      return false, "Invalid CN"
    end
  end

  msg = string.format("blue<Country:> yellow<%s>", server.player_country(n))

  if isadmin(cn) then
    msg = msg .. string.format(" blue<- IP:> green<%s>", getip(n))
  end

  messages.notice(msg):send(cn)
end
