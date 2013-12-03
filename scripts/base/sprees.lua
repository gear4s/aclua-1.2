-- Keep these messages at the top of the file so users can find them easily
local killingspree_message = {
  ["5"]  = "yellow<name<%s>> is on a orange<KILLING SPREE>!!",
  ["10"] = "yellow<name<%s>> is on a orange<RAMPAGE>!!",
  ["15"] = "yellow<name<%s>> is orange<DOMINATING>!!",
  ["20"] = "yellow<name<%s>> is orange<UNSTOPPABLE>!!",
  ["30"] = "yellow<name<%s>> is orange<GODLIKE>!!"
}

local broadcast_minimum = 10 -- minimum amount of gibs to message publically
local long_killingspree = 15
local first_frag = true
local players = {}

local function send_first_frag_message(target, actor)
    if not first_frag or target == actor then return end
    messages.info("yellow<name<%s>> made the orange<FIRST KILL>!!"):format(actor):send()
    first_frag = false
end

local function send_killingspree_message(target_cn, actor_cn)
    local actor_killingspree = players[actor_cn]
    local target_killingspree = players[target_cn]

    if actor_cn ~= target_cn then
        actor_killingspree = actor_killingspree + 1
        players[actor_cn] = actor_killingspree
    else
        actor_killingspree = 0
    end

    if killingspree_message[actor_killingspree] then
        local message = killingspree_message[actor_killingspree]

        if actor_killingspree < (broadcast_minimum - 1) --[[ guaranteed ]] then
            messages.info(message):format(actor_cn):send(actor_cn)
        else
            messages.info(message):format(actor_cn):send()
        end
    end

    if target_killingspree >= long_killingspree then
        messages.info("yellow<name<%s>> was stopped by orange<name<%s>>!!"):format(target_cn, actor_cn):send()
    end

    players[target_cn] = 0
end

events.playerdeath(function(ct, ca)
    send_first_frag_message(ct, ca)
    send_killingspree_message(ct, ca)
end)

events.intermission(function()
    first_frag = true

    for player in server.players() do
        players[player] = 0
    end
end)

events.playersuicide(function(cn)
    players[cn] = 0
end)

events.playerdisconnect(function(cn)
    players[cn] = nil
end)

events.playerconnect(function(cn)
    players[cn] = 0
end)

