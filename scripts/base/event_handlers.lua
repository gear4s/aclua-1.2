local innerEvents = {}

events = {}

local evMethods = {
  new = function(id, func)
    innerEvents[id] = {}

    events[id] = function(innerFunc)
      table.insert(innerEvents[id], innerFunc)
    end

    _G[func] = function(...)
      for i,f in ipairs(innerEvents[id]) do
        f(...)
      end
    end
  end
}

evMethods.new("playertext", "onPlayerSayText")
evMethods.new("playerconnect", "onPlayerConnect")
evMethods.new("playerteamkill", "onPlayerTeamKill")
evMethods.new("playernamechange", "onPlayerNameChange")
evMethods.new("startup", "onInit")