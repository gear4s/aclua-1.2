local loadconffile = function(filename) dofile("lua/config/"..filename..".lua") end

servername = "Another gearMod Server"
maxclients = 10
servermotd = "blue<Running> green<gearMod> blue<-> yellow<http://graphox.us/gearmod/>"
adminpass = ""

--[[

  enabled_commands - List of enabled commands

  Format:
  - enabled_commands = {
      "command name",
    }

]]
enabled_commands = {
  "help",
  "pm",
  "stats",

  -- admin commands
  "ban",
  "kick",
  "mute",
  "unmute",
  "say"
}

--[[

  disabled_commands - List of disabled commands

  Format:
  - disabled_commands = {
      "command name",
    }

]]
disabled_commands = {
  "whois"
}

--[[

  mute_words - Words to mute a player when said (mute triggers)

  Format:
  - mute_words = {
      "word",
    }

]]
mute_words = {
  "nigger",
  "nigga",
  "negro",
  "kike",
  "faggot",
  "motherfucker",
  "jude",
  "wichser",
  "kanake",
  "polake",
  "kinderficker",
  "scheiss auslaender",
  "bitch",
  "fuck",
  "asshole",
  "poes",
  "dick",
  "cock",
}

--[[

  setmasterserver - Set master server to connect to

]]
setmasterserver = "ms.cubers.net"

--[[

  sendmotd - send message of the day

]]
sendmotd = true

--[[

  mutespecs - Mute spectators by default (not implemented yet)

]]
mutespecs = false

-- load configuration file
loadconffile("server")

-- set values
setservname(servername)
setmaxcl(maxclients)
setadminpwd(adminpass)

-- unset
maxclients, adminpass, servername = nil, nil, nil

