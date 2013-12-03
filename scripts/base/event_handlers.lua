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

listener = {
  add = evMethods.new -- external usage
}

evMethods.new("playerconnect", "onPlayerConnect")
evMethods.new("playerdisconnect", "onPlayerDisconnect")

evMethods.new("playervoice", "onPlayerSayVoice")
evMethods.new("playertext", "onPlayerSayText")

evMethods.new("playerspawn", "onPlayerSpawn")
evMethods.new("playerspawnafter", "onPlayerSpawnAfter")

evMethods.new("playersetweapon", "onPlayerWeaponChange")
evMethods.new("playerreload", "onPlayerWeaponReload")
evMethods.new("playerteamkill", "onPlayerTeamKill")
evMethods.new("playersuicide", "onPlayerSuicide")
evMethods.new("playerdamage", "onPlayerDamage")
evMethods.new("playerdamageafter", "onPlayerDamageAfter")
evMethods.new("playerdeath", "onPlayerDeath")
evMethods.new("playerdeathafter", "onPlayerDeathAfter")
evMethods.new("playershoot", "onPlayerShoot")

evMethods.new("playerteamchange", "onPlayerTeamChange")
evMethods.new("playernamechange", "onPlayerNameChange")

evMethods.new("playersetadmintry", "onPlayerRoleChangeTry")
evMethods.new("playersetadmin", "onPlayerRoleChange")

evMethods.new("playerpickup", "onPlayerItemPickup")

evMethods.new("votecall", "onPlayerCallVote")
evMethods.new("voteend", "onVoteEnd")
evMethods.new("vote", "onPlayerVote")

evMethods.new("matchend", "onArenaWin")
evMethods.new("mapchange", "onMapChange")
evMethods.new("intermission", "onMapEnd")

evMethods.new("flagaction", "onFlagAction")
evMethods.new("ontick", "LuaLoop")
evMethods.new("onnotice", "onLuaNotice")

evMethods.new("itemspawn", "onItemSpawn")
evMethods.new("itemrespawn", "onItemRespawn")

evMethods.new("rotationread", "onMaprotRead")
evMethods.new("passwordread", "onPasswordsRead")
evMethods.new("blacklistread", "onBlRead")

evMethods.new("logline", "onLogLine")

evMethods.new("sendmap", "onPlayerSendMap")


evMethods.new("startup", "onInit")
evMethods.new("shutdown", "onDestroy")
