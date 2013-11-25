return function(cn, tcn, ...)
  local pName, isKicked, err = getname(tcn), kb.kick(tonumber(tcn))

  if isKicked then
    msg = string.format("blue<Kicked player> red<%s>", pName)

    if select("#", ...) ~= 0 then
      msg = msg .. string.format(" blue<- Reason:> yellow<%s>", table.concat({...}, " "))
    end

    messages.notice(-1, msg)
  else
    return false, err
  end
end
