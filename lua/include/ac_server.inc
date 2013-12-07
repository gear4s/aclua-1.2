function split(p,d)
  local t, ll
  t={}
  ll=0
  if(#p == 1) then return {p} end
    while true do
      l=string.find(p,d,ll,true)
      if l~=nil then
        table.insert(t, string.sub(p,ll,l-1))
        ll=l+1
      else
        table.insert(t, string.sub(p,ll))
        break
      end
    end
  return t
end

function slice(array, S, E)
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

function isTeammode(mode) -- gamemode
  if mode == 0 or mode == 4 or mode == 5 or mode == 7 or mode == 11 or mode == 13 or mode == 14 then
	return true
  else
	return false
  end
end

function team_isspect(team)
  return team > 1 and team <= 4
end
function team_isactive(team)
  return team == 0 or team == 1
end

local IDBASE = 10000

local fsflags = {}
local errmsg = ""

function fs_regflag(flag)
	if flag == nil then
		errmsg = "fs_regflag: expected flag name, got nil"
		return false
	end
	for i = 1, #fsflags do
		if flag == fsflags[i] then
			errmsg = "fs_regflag: flag "..flag.." already exists!"
			return false
		end
	end
	table.insert(fsflags,flag)
	errmsg = ""
	return true
end

function fs_savecflags(pid,flags)
	local sfile = io.open("lua/config/flagsystem.cfg","r")
	if sfile == nil then
		cfg.createfile("flagsystem")
	end
	io.close(sfile)

	if flags[1] == nil then
		errmsg = "fs_savecflags: table == nil"
		return false
	end

	cfg.setvalue("flagsystem",getip(pid),table.concat(fsflags,","))
	return true
end

function fs_loadcflags(pid)
	local flg = cfg.getvalue("flagsystem",getip(pid))

	if flg == "(fail)" then
		errmsg = "fs_loadcflags: couldn't load flags for ip "..getip(pid)
		return false
	else
		local tmp = split(flg,"[,]")
		if tmp[1] == nil then
			errmsg = "fs_loadcflags: no flags for ip "..getip(pid)
			return false
		end
		return tmp
	end
end

function fs_delflag(flag)
	if flag == nil then
		errmsg = "fs_regflag: expected flag name, got nil"
		return false
	end
	local notfound = 0
	for i = 1, #fsflags do
		if flag == fsflags[i] then
			table.remove(fsflags,i)
			return true
		else
			notfound = 1
		end
	end
	if notfound == 1 then
		errmsg = "fs_delflag: flag "..flag.." not exists"
		return false
	end
end

function fs_geterror()
	return errmsg
end

function fs_getflagtable()
	if fsflags[1] == nil then
		errmsg = "fs_getflagtable: flag table is nil (fix: add some flags)"
		return 0
	end
	return fsflags
end
