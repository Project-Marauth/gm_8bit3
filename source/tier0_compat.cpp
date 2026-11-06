/// Stand-ins for the tier0 logging API on 32-bit Linux. The SDK's tier1 (strtools asserts) calls LoggingSystem_IsChannelEnabled, LoggingSystem_Log and LOG_GENERAL, but the libtier0_srv.so that 32-bit Garry's Mod dedicated servers ship predates the logging system and exports none of them, so the module failed to load with "undefined symbol: LOG_GENERAL". These only ever back an assert message, so reporting every channel as disabled loses nothing. Other platforms ship a tier0 with the real API and do not compile this.
#if defined(__linux__) && defined(__i386__)

#include <tier0/dbg.h>

LoggingChannelID_t LOG_GENERAL = 0;

bool LoggingSystem_IsChannelEnabled(LoggingChannelID_t, LoggingSeverity_t) {
	return false;
}

LoggingResponse_t LoggingSystem_Log(LoggingChannelID_t, LoggingSeverity_t, const char*, ...) {
	return LR_CONTINUE;
}

#endif
