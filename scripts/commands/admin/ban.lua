return function(cn, tcn, btime, ...)
  local pName, isKicked, err = getname(tcn), kb.ban(tonumber(tcn), tonumber(btime) or 1)

  if isKicked ~= false then
    msg = string.format("blue<Banned player> red<%s> blue<for> green<%s minute%s>", tcn, (btime ~= nil) and tostring(btime) or "1", (btime = 1) and "" or "s"))

    if select("#", ...) ~= 0 then
      msg = msg .. string.format(" blue<- Reason:> yellow<%s>", table.concat({...}, " "))
    end

    messages.notice(-1, msg)
  else
    return false, err
  end
end
