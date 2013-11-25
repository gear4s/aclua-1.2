function _if(cond, ifTrue, ifFalse)
  if cond then
    return ifTrue
  else
    return ifFalse
  end
end

messages = {}

messages.colors = {
  notice = "green",
  info = "blue",
  pm = "yellow",
  mute = "red",
  warning = "red",
  teamkiller = "red",
  error = "red"
}

--function to color strings and get the name of players from cn
function messages.preprocess(msg, to)

  msg = string.gsub(msg, "name<(.-)> |(.-)|(.-)|", function(cn, you_text, name_text)
    if tonumber(cn) == tonumber(to) then
      return "you "..you_text
    else
      return "name<"..cn.."> "..name_text
    end
  end)

  msg = string.gsub(msg, "name<(.-)>", function(cn)
    name = getname(cn)
    return name:gsub("[<>]", {["<"]="&ls111;";[">"]="&ls121;"})
  end)
  
  msg = string.gsub(msg, "accol:P<(.-)>", function (string) return "\fs\fP"..string.."\fr" end)
  msg = string.gsub(msg, "accol:M<(.-)>", function (string) return "\fs\fM"..string.."\fr" end)
  msg = string.gsub(msg, "red<(.-)>", function (string) return "\fs\f3"..string.."\fr" end)
  msg = string.gsub(msg, "blue<(.-)>", function (string) return "\fs\f1"..string.."\fr" end)
  msg = string.gsub(msg, "yellow<(.-)>", function (string) return "\fs\f2"..string.."\fr" end)
  msg = string.gsub(msg, "magenta<(.-)>", function (string) return "\fs\f5"..string.."\fr" end)
  msg = string.gsub(msg, "green<(.-)>", function (string) return "\fs\f0"..string.."\fr" end)
  msg = string.gsub(msg, "orange<(.-)>", function (string) return "\fs\f6"..string.."\fr" end)
  msg = string.gsub(msg, "grey<(.-)>", function (string) return "\fs\f4"..string.."\fr" end)
  
  msg = string.gsub(msg, "&ls111;", "<")
  msg = string.gsub(msg, "&ls121;", ">")
        
  return msg
end

function messages.parse(msg_color, msg_type, private, msg)
  msg = "orange<--[> " .. msg_color .. "<[ " .. msg_type .. _if(private, "> red<(private)> " .. msg_color .. "<", " ") .. "] > " .. msg .. " orange<]-- >"
  return msg
end

function messages.new(name)
  messages[name] = function(cn, msg, private)
    local msg_type = name

    --check if all required fields are set (possible crashing?)
    if cn == nil or msg == nil then
      return
    end
    
    private = (type(private) == "boolean") and private or false
    
    msg = messages.parse(messages.colors[msg_type], msg_type, private, msg)
    
    if type(cn) ~= "table" then
      if cn == -1 then
        cn = server.players()
      elseif cn == -2 then
        cn = server.adminplayers()
      end
    end
    
    if type(cn) == "table" then
      local sent_to = {}
      for _, scn in pairs(cn) do
        if not sent_to[scn] then
          server.say(messages.preprocess(msg, scn), scn)
          sent_to[scn] = true
        end
      end
    else
      server.say(messages.preprocess(msg, cn), cn)
    end
  end
end

messages.new("notice")
messages.new("info")
messages.new("warning")
messages.new("teamkiller")
messages.new("error")
