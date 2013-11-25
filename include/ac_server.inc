include("main")
include("messages")

function switch (selector, t)
  assert (type (t) == "table", "Table not supplied to switch")

  local f = t [selector]

  if f then
    assert (type (f) == "function", "Not a function")
    f ()
  else
    print "default"
  end -- if
end -- switch

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