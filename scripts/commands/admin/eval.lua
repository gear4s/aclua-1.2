return function(cn, ...)
  if select("#", ...) == 0 then
    return false, "Need code to evaluate"
  end

  assert(loadstring(table.concat({...}, " ")))()
end
