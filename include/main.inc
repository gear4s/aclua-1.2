server = {}

function server.fileload(scriptfile)
    dofile("lua/scripts/base/"..scriptfile..".lua")
end

function server.say(text, cn)
  if cn == nil then cn = -1 end -- to all
  clientprint(cn, text)
end

function server.isTeammode(mode)
  if mode == 0 or mode == 4 or mode == 5 or mode == 7 or mode == 11 or mode == 14 then
	return true
  else
	return false
  end
end

function server.smart_switch(cn, team)
  forcedeath(cn)
  setteam(cn, team)
end

function table.contains(table, element)
  for _, value in pairs(table) do
    if value == element then
      return true
    end
  end
  return false
end

function server.players()
  local cList = {}
  for k=0, maxclient() do
    if getip(k) ~= nil then
      table.insert(cList, k)
    end
  end
  return cList
end

function server.adminplayers()
  local acList, cList = {}, server.players()
  for _, cn in pairs(cList) do
    if isadmin(cn) then
      table.insert(acList, cn)
    end
  end
  return acList
end

function string.slice(array, S, E)
  local result = {}
  local length = #array
  S = S or 1
  E = E or length
  if E < 0 then
    E = length + E + 1
  elseif E > length then
    E = length
  end
  if S < 1 or S > length then
    return {}
  end
  local i = 1
  for j = S, E do
    result[i] = array[j]
    i = i + 1
  end
  return result
end

function table.itemremove(array, item)
  local pKey = ""
  for k, v in pairs(array) do
    if tostring(v) == tonumber(item) then
      pKey = k
    end
  end
  table.remove(array, tonumber(pKey))
end

function server.player_country(cn)
  return geoip.ip_to_country(getip(cn))
end
