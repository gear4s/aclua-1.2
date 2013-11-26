return function (cn, ...)
  if select("#", ...) == 0 then
    return false, "Need text to send"
  end

  messages.notice("red<%s>"):format(table.concat({...}, " ")):send()
end
