PLUGIN_NAME = "GraphOX"
PLUGIN_AUTHOR = "gear4" -- aaron@graphox.us
PLUGIN_VERSION = "1.2.1" -- 15 August 2012

-- common

include("ac_server")  -- load base2 for framework

geoip.load_geoip_database("lua/extra/geoip.dat")

-- load framework
server.fileload("config")
server.fileload("event_handlers")
server.fileload("kickban")
server.fileload("mute_handler")
server.fileload("commands")
server.fileload("tklimit")
server.fileload("banner")
