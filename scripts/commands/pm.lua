return function (cn, tcn, ...)
  n = tonumber(tcn)

  if getip(n) == nil then
    return false, "Invalid CN"
  end

  messages.notice(tcn, string.format("blue<PM from> yellow<name<%s> (%d)> yellow<:> accol:M<%s>", cn, cn, table.concat({...}, " ")), true)
  messages.notice(cn, string.format("blue<PM sent to> yellow<name<%s>>", tcn), true)
end
