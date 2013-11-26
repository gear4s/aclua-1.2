local innerEvents = {}

events = {}

local evMethods = {
  new = function(id)
    innerEvents[id] = {}

    events[id] = function(innerFunc)
      table.insert(innerEvents[id], innerFunc)
    end
  end,

  call = function(id, ...)
    for i,f in ipairs(innerEvents[id]) do
      f(...)
    end
  end
}

evMethods.new("playertext")
evMethods.new("playerconnect")
evMethods.new("playerteamkill")
evMethods.new("playernamechange")
evMethods.new("startup")

function onPlayerSayText(...)
  evMethods.call("playertext", ...)
end
function onPlayerConnect(...)
  evMethods.call("playerconnect", ...)
end
function onPlayerTeamKill(...)
  evMethods.call("playerteamkill", ...)
end
function onInit(...)
  evMethods.call("startup", ...)
end
function onPlayerNameChange(...)
  evMethods.call("playernamechange", ...)
end

