return function (cn, tcn, ...)
  n = tonumber(tcn)

  if getip(n) == nil then
    return false, "blue<Invalid CN>"
  end

  if server.is_muted(tcn) then
    server.unmute(tcn)
  else
    return false, string.format("Player> blue<name<%s>> red<is not muted", n)
  end
end
