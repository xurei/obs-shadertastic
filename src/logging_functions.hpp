#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

#ifdef DEV_MODE
    #define debug(format, ...) info("(debug) " #format, ##__VA_ARGS__)
#else
    #define debug(format, ...)
#endif