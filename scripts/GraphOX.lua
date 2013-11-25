PLUGIN_NAME = "GraphOX"
PLUGIN_AUTHOR = "gear4" -- aaron@graphox.us
PLUGIN_VERSION = "1.2.1" -- 15 August 2012

-- common

include("ac_server")  -- load base2 for framework

-- load framework
server.fileload("kickban")
server.fileload("config")
server.fileload("mute_handler")
server.fileload("commands")
server.fileload("event_handlers")
