local uc = require "uppercut"

local filename = "server" -- server.log

local logfile = io.open("log\\" .. filename .. ".log","a+")

--[[

  server.log - Log message to file (default: server.log)

  Parameters:
  - msg
    - string
      - log message

]]
function server.log(msg)
  assert(msg ~= nil)
  logfile:write(os.date("[%a %d %b %X] ",os.time()))
  logfile:write(msg)
  logfile:write("\n")
  logfile:flush()
end

--[[

  server.log_destroy - Stop logfile writing

]]
function server.log_destroy()
  uc.curry(logfile.close, logfile))
end

--[[

  server.log_status - Echo status out to terminal

  Parameters:
  - msg
    - string
      - log message

]]
function server.log_status(msg)
  print(msg)
end

--[[

  server.log_error - Log an error to io screen

  Parameters:
  - msg
    - string
      - log message

]]
function server.log_error(error_message)
  if type(error_message) == "table" and type(error_message[1]) == "string" then
    error_message = error_message[1]
  end

  io.stderr:write(os.date("[%a %d %b %X] ",os.time()))
  io.stderr:write(error_message)
  io.stderr:write("\n")
  io.stderr:flush()
end
