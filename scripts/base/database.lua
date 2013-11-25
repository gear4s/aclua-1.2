--[[

  DATABASE CONNECTION FRAMEWORK OBJECT

  NOTE: still in development

]]

--include the core database connection
require "luasql_mysql"

local RECONNECT = 3000
--create the db object
db = {}
db.env = assert(luasql.mysql())

--create a function to make it simple to (re)connect
function db.connect()
  db.con = assert(
    db.env:connect(
      --sourcename[,username[,password[,hostname[,port]]]]
      tostring(config.get("mysql_database") or "alphaserv"),
      tostring(config.get("mysql_username") or "alphaserv"),
      tostring(config.get("mysql_password") or "password"),
      tostring(config.get("mysql_hostname") or "localhost"),
      tonumber(config.get("mysql_port") or 3306)
    )
  )
end

--disconnect function
function db.disconnect (reconnect)
  db.con:close()
  db.env:close()
  if reconnect == true then
    server.sleep(RECONNECT, function()
      db.connect()
    end)
  end
end

--templated for basic db operations
db.templates = {}
db.templates.select = "SELECT %s FROM %s"
db.templates.select_where = "SELECT %s FROM %s WHERE %s"
db.templates.select_order = "SELECT %s FROM %s ORDER BY %s"
db.templates.select_where_order = "SELECT %s FROM %s WHERE %s ORDER BY %s"
db.templates.insert = "INSERT INTO %s ( %s ) VALUES ( %s )"
db.templates.update = "UPDATE %s SET %s WHERE ( %s )"

--///Functions///--
function db.escape(string)
  if string == nil then return nil end
  string = tostring(string)
  local escaped = string.format('%q',string)
  debug.write(-1, "mysql escaped: '"..string.."'; to: '"..tostring(escaped).."';")
  return escaped
end

function db.createresult ()
  return {
    ['rows'] = {},
    ['addrow'] = function (this, row, i)
      local count = #this.rows+1
      if i then count = i end
      this.rows[count] = {}
      for a, b in pairs(row) do
        this.rows[count][a] = b
      end
    end
  }
end

function db.query(sql)
  local cur = assert (db.con:execute(sql))
  local data = {}
  if not cur then
    db.disconnect (true)
    return false
  end
  if cur == true or cur == 1 then return true end
  if (cur == 0) or (cur == false) then
    return false
  end
  local i = 0
  local numrows = cur:numrows()
  if numrows > 0 then
    local this = db.createresult()
    row = cur:fetch ({}, "a")
    while row do
      i = i + 1 --add 1
      this:addrow(row, i)

      row = cur:fetch (row, "a")
    end
    data = this.rows
  end
  cur:close()
  return data
end
