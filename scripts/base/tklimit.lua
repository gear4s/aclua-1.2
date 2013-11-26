local pk = {}

events.playerteamkill(function(cn, tcn)
  pk[cn] = pk[cn] + 1

  if pk[cn] < 5 then
    messages.warning("blue<This server has anti-teamkill policies! 5 maximum teamkills!>%s"):format((pk[cn] == 4) and " red<LAST WARNING!>" or ""):send(cn)
  else
    messages.notice("blue<Banned player> red<name<%s>> blue<for> green<10 minutes> blue<- Reason:> yellow<teamkill limit reached>"):send(cn)

    kb.ban(cn, 10)
  end
end)

events.playerconnect(function(cn)
  pk[cn] = 0
end)
