#ifndef __LUA_GENERATORS_H__
#define __LUA_GENERATORS_H__

#define LUA_YIELD_IS_SPAM_DETECTED "yieldIsSpamDetected" // = bool (spam detected); (int player_cn, string message, bool spam_detected)
#define LUA_YIELD_SERVER_OPERATOR "yieldServerOperator" // = int (operator's cn); (int operator_cn)
#define LUA_YIELD_CAN_SPEECH "yieldCanSpeech" // = bool (can speech); (int player_cn, string message, bool can_speech)
#define LUA_YIELD_IS_BANNED "yieldIsBanned" // = bool (is banned); (string ip, bool is_banned)
#define LUA_YIELD_IS_BLACKLISTED "yieldIsBlacklisted" // = bool (is blacklisted); (string ip, bool is_blacklisted)
#define LUA_YIELD_NUMBER_OF_PLAYERS "yieldNumberOfPlayers" // = int (number of online players); (int players)

#endif
