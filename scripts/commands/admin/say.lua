return function (cn, ...)
  if select("#", ...) == 0 then
    return false, "Need text to send"
  end

  messages.notice(-1, "red<" .. table.concat({...}, " ") .. ">")
end
