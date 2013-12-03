local function sendMOTD(cn)
  if cn == nil then cn = -1 end
  messages.notice(servermotd):send(cn)
  messages.notice("blue<Type> yellow<!cmds> blue<to see available server commands> ."):send(cn)
end

events.playerconnect(function(cn)
  messages.notice("yellow<name<%s>> blue<has connected from> red<%s>"):format(cn, server.player_country(cn)):send()

  sendMOTD(cn)
end)
