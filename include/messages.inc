messages = {}

msgd = {
  text = "",
  name = "",
  to = "",

  colors = {
    notice = "green",
    info = "blue",
    pm = "yellow",
    mute = "red",
    warning = "red",
    teamkiller = "red",
    error = "red"
  },

  new = function(self, name)
    if self.colors == nil then
      print("INVALID MESSAGE NAME ! COLOR REQUIRED ! MESSAGE NOT CREATED")
      return
    end
    messages[name] = function(text)
      msgd.text = text or ""
      msgd.name = name

      return msgd
    end
  end,

  text_vars = function(self, msg)
    msg = string.gsub(msg, "(%w+)(%b<>)", function(color, text)
      text = text:sub(2, -2)
      local color = ({
        green = '0',
        blue = '1',
        yellow = '2',
        red = '3',
        grey = '4',
        magenta = '5',
        orange = '6',
      })[color]

      if color then
        return "\fs\f"..color..text.."\fr"
      end
    end)

    msg = string.gsub(msg, "name<(.-)>", function(cn)
      return getname(tonumber(cn))
    end)

    return msg
  end,

  prefix = function(self, private)
    private = private and "> red<(private)>" .. self.colors[self.name] .. "<" or ""
    local title = self.colors[self.name] .. "<[ " .. self.name .. private .. " ]> "

    self.text = title .. self.text
  end,

  format = function(self, ...)
    self.text = string.format(self.text, ...)
    return self
  end,

  send = function(self, cn, private)
    self:prefix((private == nil) and (cn and true or false) or (private and true or false))

    if not cn then
      server.say(self:text_vars(self.text))
    else
      server.say(self:text_vars(self.text), cn)
    end
  end
}

msgd:new("notice")
msgd:new("info")
msgd:new("warning")
msgd:new("error")
