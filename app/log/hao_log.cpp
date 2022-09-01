#include "hao_log.h"
#include "hao_global.h"

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include <string>
#include <array>
#include <string_view>
#include <vector>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <iostream>
#include <limits>
#include <type_traits>

using namespace hao_log;

using std::array;
using std::vector;
using std::string;
using std::string_view;
using std::to_chars;


struct GlobalLog
{
    int log_fd_;
    LogLevel log_level_;
};

GlobalLog global_log_;

LogLevel hao_log::GetLevel()
{
    return global_log_.log_level_;
}

constexpr const char* h_level_string(int level)
{
    return &"[      ]"      // null string
            "[emerg ]"      // system is unusable
            "[alert ]"      // action must be taken immediately
            "[crit  ]"      // critical conditions
            "[error ]"      // error conditions
            "[warn  ]"      // warning conditions
            "[notice]"      // normal, but significant, condition
            "[info  ]"      // informational message
            "[debug ]"      // debug-level message
            [level * 8];
}

void hao_log::LOG_INIT(string_view filename, LogLevel init_level)
{
    
    global_log_.log_level_ =  init_level;
    // FIXME 
    // 权限应该是可读，0644
    global_log_.log_fd_ = open(filename.empty() ? "HaoServer.log" : filename.data(), O_RDWR | O_APPEND | O_CREAT, 0666);
    if(global_log_.log_fd_ == -1)
    {
        perror("open file failed.");
        global_log_.log_fd_ = STDERR_FILENO;
    }
    printf("log fd:%d\n", global_log_.log_fd_);
}

void hao_log::LOG_EXIT()
{
    if(global_log_.log_fd_ != -1)
    {
        close(global_log_.log_fd_);
        global_log_.log_fd_ = -1;
    }
}


constexpr const char* digits2(size_t value) {
  // GCC generates slightly better code when value is pointer-size.
  return &"0001020304050607080910111213141516171819"
         "2021222324252627282930313233343536373839"
         "4041424344454647484950515253545556575859"
         "6061626364656667686970717273747576777879"
         "8081828384858687888990919293949596979899"[value * 2];
}

inline void format_time(char* buf, unsigned a, unsigned b, unsigned c, char sep)
{
    uint64_t timer_buffer = a | (b << 24) | (static_cast<uint64_t>(c) << 48);
    timer_buffer += (((timer_buffer * 205) >> 11) & 0x000f00000f00000f) * 6;
    timer_buffer = ((timer_buffer & 0x00f00000f00000f0) >> 4) | ((timer_buffer & 0x000f00000f00000f) << 8);
    auto u_sep = static_cast<uint64_t>(sep);
    timer_buffer |= 0x3030003030003030 | (u_sep << 16) | (u_sep << 40);

    std::memcpy(buf, &timer_buffer, 8);
}

void hao_log::LOG_TO_STDERR(int err, const char * fmt, ...)
{
    char log_line_buffer_[kLogLineSize];
    std::memset(log_line_buffer_, 0, sizeof(log_line_buffer_));

    // format time
    timeval current_time;
    gettimeofday(&current_time, NULL);
    struct tm to_time;
    localtime_r(&current_time.tv_sec, &to_time);
    std::memcpy(log_line_buffer_ , digits2(static_cast<size_t>((1900+to_time.tm_year)/100)), 2);
    format_time(log_line_buffer_ + 2,
                    static_cast<unsigned>(to_time.tm_year%100), 
                    static_cast<unsigned>(to_time.tm_mon+1), 
                    static_cast<unsigned>(to_time.tm_mday), '-');
    log_line_buffer_[10] = ' ';
    format_time(log_line_buffer_ + 11, 
                    static_cast<unsigned>(to_time.tm_hour), 
                    static_cast<unsigned>(to_time.tm_min), 
                    static_cast<unsigned>(to_time.tm_sec), ':');
    log_line_buffer_[19] = '.';
    auto usec = current_time.tv_usec;
    char* usec_end = &log_line_buffer_[26];
    for(int i{0}; i < 3; ++i)
    {
        usec_end -= 2;
        std::memcpy(usec_end, digits2(static_cast<size_t>(usec%100)), 2);
        usec /= 100;
    }
    log_line_buffer_[26] = ' ';

    int i {27}, count{0};
    if(err)
    {
        char* error_info = strerror(err);
        size_t len = std::strlen(error_info);
        std::memcpy(log_line_buffer_+i, error_info, len);
        i+=len;
        ++i;
        log_line_buffer_[i] = ' ';
        ++i;
    }

    char *p{log_line_buffer_+i}, *last{log_line_buffer_ + kLogLineSize};

    
    va_list args;
    va_start(args, fmt);
    count = vsnprintf(p, kLogLineSize - i, fmt, args);
    va_end(args);
    
    write(STDERR_FILENO, log_line_buffer_, i+count);
}

Log::Log(LogLevel level, string_view filename, unsigned int line, int err)
    :free_space_{kLogLineSize}, cur_{log_line_buffer_}, end_{log_line_buffer_+kLogLineSize}
{
    // format time
    timeval current_time;
    gettimeofday(&current_time, NULL);
    struct tm to_time;
    localtime_r(&current_time.tv_sec, &to_time);
    std::memcpy(log_line_buffer_ , digits2(static_cast<size_t>((1900+to_time.tm_year)/100)), 2);
    format_time(log_line_buffer_ + 2,
                    static_cast<unsigned>(to_time.tm_year%100), 
                    static_cast<unsigned>(to_time.tm_mon+1), 
                    static_cast<unsigned>(to_time.tm_mday), '-');
    log_line_buffer_[10] = ' ';
    format_time(log_line_buffer_ + 11, 
                    static_cast<unsigned>(to_time.tm_hour), 
                    static_cast<unsigned>(to_time.tm_min), 
                    static_cast<unsigned>(to_time.tm_sec), ':');
    log_line_buffer_[19] = '.';
    auto usec = current_time.tv_usec;
    char* usec_end = &log_line_buffer_[26];
    for(int i{0}; i < 3; ++i)
    {
        usec_end -= 2;
        std::memcpy(usec_end, digits2(static_cast<size_t>(usec%100)), 2);
        usec /= 100;
    }
    log_line_buffer_[26] = ' ';
    cur_ = log_line_buffer_ + 27;
    free_space_ -= 27;
    
    // append pid
    *this << pid << ' ';

    // append [log_level]
    Append(h_level_string(level), 8);

    // append [filename line]
    *this << '[' << filename << ' ' << line << ']';
    
    // append errno
    if(err)
    {
        // Append("error occured", 13);
        // *this << " err:" << err << ' ';
        // char buf[512];
        // strerror_r(err, buf, 512);
        // *this << buf;
        *this << "err:" << err <<' ' <<  strerror(err) << ' ';
    }

}

void Log::Append(const char* src, size_t len)
{
    if(free_space_)
    {
        auto avail = std::min(free_space_, len);
        std::memcpy(cur_, src, avail);
        cur_+= avail;
        free_space_ -= avail;
    }
}

Log& Log::operator<<(bool num)
{
    if(free_space_)
    {
        *cur_ = num ? '1' : '0';
        ++cur_;
        --free_space_;
    }

    return *this;
}

template <typename INT>
constexpr int digits10() noexcept
{
    return std::numeric_limits<INT>::digits10;
}

struct result
{
    char* begin;
    char* end;
};
template<typename UINT>
result format_decimal(char* out, UINT value, int size)
{
    out += size;
    char* end = out;
    while(value >= 100)
    {
        out -= 2;
        // memcpy(dst, src, 2);
        memcpy(out, digits2(static_cast<size_t>(value%100)), 2);
        value /= 100;
    }
    if(value < 10)
    {
        *--out = static_cast<char>('0'+value);
        return {out, end};
    }
    out -= 2;
    memcpy(out, digits2(static_cast<size_t>(value)), 2);
    return {out,end};
}

// +1 是为了存负号-
constexpr int buffer_size = std::numeric_limits<unsigned long long>::digits10+1;
template<typename T>
constexpr int num_bits()
{
    return std::numeric_limits<T>::digits;
}
template<bool B, typename T, typename F>
using conditional_t = typename std::conditional<B, T, F>::type;
template <typename T>
using uint32_or_64_t =
    conditional_t<num_bits<T>() <=32, 
                    uint32_t, 
                    conditional_t<num_bits<T>() < 64, uint64_t, uintmax_t>>;
template<typename T>
void Log::FormatInteger(T num)
{
    auto abs_value = static_cast<uint32_or_64_t<T>>(num);
    bool negative {num < 0};
    if(negative)
    {
        abs_value = 0 - abs_value;
    }
    char buffer[buffer_size];
    char* begin = format_decimal(buffer, num, buffer_size-1).begin;
    if(negative)
    {
        *--begin = '-';
    }
    if(free_space_ >= kMaxNumericSize)
    {
        Append(begin, buffer - begin + buffer_size - 1);
    }
}

Log& Log::operator<<(short num)
{
    *this << static_cast<int>(num);
    return *this;
}

Log& Log::operator<<(unsigned short num)
{
    *this << static_cast<unsigned int>(num);
    return *this;
}

Log& Log::operator<<(int num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(unsigned int num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(long num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(unsigned long num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(long long num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(unsigned long long num)
{
    FormatInteger(num);
    return *this;
}

Log& Log::operator<<(const char* src)
{
    if(src)
    {
        auto size = std::strlen(src);
        auto real = std::min(size, free_space_);
        std::memcpy(cur_, src, real);
        cur_ += real;
        free_space_ -= real;
    }
    else
    {
        if(free_space_ >= 6)
        memcpy(cur_, "(null)", 6);
        cur_ += 6;
        free_space_ -= 6;
    }
    

    return *this;
}

Log& Log::operator<<(const void * src)
{
    std::uintptr_t address = reinterpret_cast<uintptr_t>(src);
    if(free_space_ >= kMaxNumericSize)
    {
        int n = std::snprintf(cur_, free_space_, "0x%" PRIXPTR, address);
        cur_ += n;
        free_space_ -= n;
    }

    return *this;
}

Log& Log::operator<<(float num)
{
    *this << static_cast<double>(num);
    return *this;
}

Log& Log::operator<<(double num)
{
    if(free_space_ >= kMaxNumericSize)
    {
        int len = snprintf(cur_, kMaxNumericSize, "%.12g", num);
        cur_ += len;
        free_space_ -= len;
    }
    return *this;
}


Log& Log::operator<<(char c)
{
    if(free_space_)
    {
        *cur_ = c;
        ++cur_;
        --free_space_;
    }
    return *this;
}

Log& Log::operator<<(string_view sv)
{
    auto real = std::min(sv.size(), free_space_);
    std::memcpy(cur_, sv.data(), real);
    cur_ += real;
    free_space_ -= real;
    return *this;
}

Log& Log::operator<<(const std::string& s)
{
    auto real = std::min(s.size(), free_space_);
    std::memcpy(cur_, s.c_str(), real);
    cur_ += real;
    free_space_ -= real;
    return *this;
}

Log::~Log()
{
    
    if(cur_ != end_)
    {
        *cur_ = '\n';
        cur_++;
    }
    size_t size = cur_ - log_line_buffer_;
    size_t written {0};
    ssize_t result {0};
    while(written < size)
    {
        result = write(global_log_.log_fd_, &log_line_buffer_[written], size - written);
        if(result == -1)
        {
            if(errno == ENOSPC)
            {
                // 磁盘空间不够了
                break;
            }
            else
            {
                continue;
            }
        }
        written += result;
    }
}