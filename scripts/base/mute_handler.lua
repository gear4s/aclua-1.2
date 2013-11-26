-- Muted players' table
local muted = {}

--[[

  server.mute - Mute a player

  Parameters:
  - cn
    - integer
      - player client number
  - tcn
    - integer
      - player client number
  - reason
    - string
      - reason for mute

]]
function server.mute(cn, tcn, reason)
  table.insert(muted, getip(tcn))

  local msg = "blue<You have been> red<muted> blue<by> orange<name<%s>>"

  if reason ~= "" then
     msg = msg .. " blue<- Reason:> yellow<%s>"
  end

  messages.notice(msg):format(cn, reason):send(tcn)
end

--[[

  server.unmute - Unmute a player

  Parameters:
  - cn
    - integer
      - player client number

]]
function server.unmute(cn)
  table.itemremove(muted, getip(cn))

  messages.notice("blue<You have been> green<unmuted>blue<.>"):send(cn)
end

--[[

  server.is_muted - Check if a player is muted

  Parameters:
  - cn
    - integer
      - player client number

]]
function server.is_muted(cn)
  return table.contains(muted, getip(cn))
end

--[[

  LOCAL translate_number_letters - Replace numbers (4 => a, 3 => e, etc)

  Parameters:
  - text
    - string
      - text to escape

]]
local function translate_number_letters(text)
  text = text:gsub("4", "a")
  text = text:gsub("3", "e")
  text = text:gsub("1", "i")
  text = text:gsub("0", "o")
  return text
end

--[[

  block_text - Block a player's text from being sent (handled with "onPlayerSayText" event)

  Parameters:
  - cn
    - integer
      - player client number
  - text
    - string
      - mute trigger

]]
function block_text(cn, text)
  if server.is_muted(cn) then
    messages.warning("red<Your chat messages are being blocked.>"
    return true
  else -- Check for mute triggers in their message
    text = string.lower(translate_number_letters(text))

    for _, trigger in pairs(mute_words) do
      if text:match(trigger) then
        messages.warning("red<Your chat messages are being blocked due to offensive language.>"):send(cn)
        table.insert(muted, getip(cn))
        return true
      end
    end
  end
end
