return function (cn, tcn, ...)
  n = tonumber(tcn)

  if getip(n) == nil then
    return false, "Invalid CN"
  end

  if n == tonumber(cn) then
    return false, "You cannot mute yourself"
  end

  server.mute(cn, tcn, table.concat({...}, " "))
end
