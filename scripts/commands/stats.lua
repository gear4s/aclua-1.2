return function(cn, tcn)
  if tcn ~= nil and tostring(tcn) ~= tostring(cn) then
    n = tonumber(tcn)
    if n ~= nil and getip(n) ~= nil then
      messages.notice("blue<Stats for player> red<name<%s>>blue<:> green<Frags:> orange<%s> blue<-> green<Deaths:> red<%s> blue<-> green<Score:> yellow<%s>"):format(n, getfrags(n), getdeaths(n), getscore(n)):send(cn)
      messages.notice(cn, string.format("blue<--> green<Teamkills:> orange<%s> blue<-> green<Accuracy:> red<%s>", getteamkill(n), getaccuracy(n)), true):send(cn)
    else
      return false, "Invalid CN"
    end
  else
    messages.notice(string.format("blue<Your stats:> green<Frags:> orange<%s> blue<-> green<Deaths:> red<%s> blue<-> green<Score:> yellow<%s>", getfrags(cn), getdeaths(cn), getscore(cn))):send(cn)
    messages.notice(string.format("blue<--> green<Teamkills:> orange<%s> blue<-> green<Accuracy:> red<%s>", getteamkill(cn), getaccuracy(cn)), true):send(cn)
  end
end
