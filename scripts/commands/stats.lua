return function(cn, tcn)
  if tcn ~= nil and tostring(tcn) ~= tostring(cn) then
    n = tonumber(tcn)
    if n ~= nil and getip(n) ~= nil then
      messages.notice(cn, string.format("blue<Stats for player> red<name<%s>>blue<:> green<Frags:> orange<%s> blue<-> green<Deaths:> red<%s> blue<-> green<Score:> yellow<%s>", n, getfrags(n), getdeaths(n), getscore(n)), true)
      messages.notice(cn, string.format("blue<--> green<Teamkills:> orange<%s> blue<-> green<Accuracy:> red<%s>", getteamkill(n), getaccuracy(n)), true)
    else
      return false, "Invalid CN"
    end
  else
    messages.notice(cn, string.format("blue<Your stats:> green<Frags:> orange<%s> blue<-> green<Deaths:> red<%s> blue<-> green<Score:> yellow<%s>", getfrags(cn), getdeaths(cn), getscore(cn)), true)
    messages.notice(cn, string.format("blue<--> green<Teamkills:> orange<%s> blue<-> green<Accuracy:> red<%s>", getteamkill(cn), getaccuracy(cn)), true)
  end
end
