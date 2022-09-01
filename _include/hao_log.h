#ifndef _HAO_LOG_H_
#define _HAO_LOG_H_

#include <sys/types.h>
#include <errno.h>
#include <string>
#include <string_view>
using std::string_view;


namespace hao_log
{
    const int kLogLineSize { 4096 };
    const int kMaxNumericSize { 48 };
    enum LogLevel
    {
        STDERR = 0, EMERG, ALERT, CRIT, ERROR, WARN, NOTICE, INFO, DEBUG
    };

    void LOG_INIT(string_view filename, LogLevel init_level);
    void LOG_EXIT();
    LogLevel GetLevel();
    void LOG_TO_STDERR(int err, const char * fmt, ...);
    
    #define LOG_SET_LEVEL(level) global_log_.log_level_ = level;

    #define _LOG_LEVEL_(level, cur_errno) \
        (level > GetLevel()) ? (void)0: \
            Voidify() & Log(level, __FILE__, __LINE__, cur_errno)
    
    #define LOG_EMERG  _LOG_LEVEL_(LogLevel::EMERG, errno)
    #define LOG_ALERT  _LOG_LEVEL_(LogLevel::ALERT, errno)
    #define LOG_CRIT   _LOG_LEVEL_(LogLevel::CRIT, errno)
    #define LOG_ERROR  _LOG_LEVEL_(LogLevel::ERROR, errno)
    #define LOG_WARN   _LOG_LEVEL_(LogLevel::WARN, errno)
    #define LOG_NOTICE _LOG_LEVEL_(LogLevel::NOTICE, errno)
    #define LOG_INFO   _LOG_LEVEL_(LogLevel::INFO, errno)
    #define LOG_DEBUG  _LOG_LEVEL_(LogLevel::DEBUG, errno)

    #define LOG(level) _LOG_LEVEL_(level, errno)

    class Log
    {
        public:
            Log(LogLevel level, string_view filename, unsigned int line, int err);
            ~Log();
            
            Log(const Log&) = delete;
            Log(Log&&) = delete;
            const Log& operator=(const Log&) = delete;
            Log& operator=(Log&&) = delete;
            
            Log& operator<<(bool);
            Log& operator<<(short);
            Log& operator<<(unsigned short);
            Log& operator<<(int);
            Log& operator<<(unsigned int);
            Log& operator<<(long);
            Log& operator<<(unsigned long);
            Log& operator<<(long long);
            Log& operator<<(unsigned long long);
            Log& operator<<(const void *);
            Log& operator<<(const char*);
            Log& operator<<(float);
            Log& operator<<(double);
            Log& operator<<(char);
            Log& operator<<(string_view);
            Log& operator<<(const std::string&);

        private:
            template<typename T>
            void FormatInteger(T);

            void Append(const char* src, size_t len);
            
            char log_line_buffer_[kLogLineSize];
            size_t free_space_;
            char* cur_;
            char* end_;
    };

    namespace
    {
        class Voidify
        {
            public:
                void operator&(const Log&){}
        };
    }   
    
}

#endif