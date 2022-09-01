#include "hao_timestamp.h"
#include "sys/time.h"
#include <inttypes.h>

#include <cstring>
#include <iostream>

using std::chrono::duration_cast;


Timestamp::Timestamp()
    : micro_seconds_since_epoch_{0}
{

}
Timestamp::Timestamp(int64_t microseconds)
    : micro_seconds_since_epoch_{microseconds}
{

}

Timestamp operator+(Timestamp a, const microseconds& b)
{
    return a.micro_seconds_since_epoch_+b.count();
}

Timestamp operator+(const Timestamp a, const milliseconds& b)
{
    return a.micro_seconds_since_epoch_ + duration_cast<microseconds>(b).count();
}

Timestamp operator+(const Timestamp a, const std::chrono::seconds& b)
{
    return a.micro_seconds_since_epoch_ + duration_cast<microseconds>(b).count();
}

Timestamp operator+(const Timestamp a, const std::chrono::minutes& b)
{
    return a.micro_seconds_since_epoch_ + duration_cast<microseconds>(b).count();
}

Timestamp operator+(const Timestamp a, const std::chrono::hours& b)
{
    return a.micro_seconds_since_epoch_ + duration_cast<microseconds>(b).count();
}

Timestamp operator+(const Timestamp a, const int64_t b)
{
    return a.micro_seconds_since_epoch_ + b;
}

Timestamp& Timestamp::operator+=(const Timestamp b)
{
    // 这里错误的使用的+，而不是+=，导致加不成功
    // std::cerr << this->micro_seconds_since_epoch_ << std::endl;
    // std::cerr << b.micro_seconds_since_epoch_ << std::endl;
    this->micro_seconds_since_epoch_+= b.micro_seconds_since_epoch_;
    return *this;
}

Timestamp& Timestamp::operator+=(const hours hours_)
{
    micro_seconds_since_epoch_ += duration_cast<microseconds>(hours_).count();
    return *this;
}

Timestamp& Timestamp::operator+=(const minutes minutes_)
{
    micro_seconds_since_epoch_ += duration_cast<microseconds>(minutes_).count();
    return *this;
}

Timestamp& Timestamp::operator+=(const seconds sec)
{
    micro_seconds_since_epoch_ += duration_cast<microseconds>(sec).count();
    return *this;
}

Timestamp& Timestamp::operator+=(const milliseconds mill)
{
    micro_seconds_since_epoch_ += duration_cast<microseconds>(mill).count();
    return *this;
}

Timestamp& Timestamp::operator+=(const microseconds micro)
{
    micro_seconds_since_epoch_ += duration_cast<microseconds>(micro).count();
    return *this;
}

bool operator <(const Timestamp a, const Timestamp b)
{
    return a.micro_seconds_since_epoch_ < b.micro_seconds_since_epoch_;
}
bool operator >(const Timestamp a, const Timestamp b)
{
    return b < a;
}
bool operator <=(const Timestamp a, const Timestamp b)
{
    return !(a > b);
}

bool operator >=(const Timestamp a, const Timestamp b)
{
    return !(a < b);
}

bool operator ==(const Timestamp a, const Timestamp b)
{
    return a.micro_seconds_since_epoch_ == b.micro_seconds_since_epoch_;
}
bool operator !=(const Timestamp a, const Timestamp b)
{
    return !(a==b);
}

bool operator < (const Timestamp a, const milliseconds b)
{
    return a.micro_seconds_since_epoch_ < duration_cast<microseconds>(b).count();
}

Timestamp Timestamp::operator-(const Timestamp b)
{
    return micro_seconds_since_epoch_-b.micro_seconds_since_epoch_;
}

int64_t Timestamp::Milliseconds() const
{
    return micro_seconds_since_epoch_ / 1000;
}

int64_t Timestamp::Microseconds() const
{
    return micro_seconds_since_epoch_;
}

void Timestamp::swap(Timestamp& that)
{
    std::swap(micro_seconds_since_epoch_, that.micro_seconds_since_epoch_);
}

string Timestamp::ToString() const
{
    char buf[32] = {0};
    int64_t seconds = micro_seconds_since_epoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = micro_seconds_since_epoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
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

string Timestamp::ToFormattedString(bool show_microseconds) const
{
    char buf[64]{0};
    time_t seconds = static_cast<time_t>(micro_seconds_since_epoch_ / kMicroSecondsPerSecond);
    struct tm tm_time;
    localtime_r(&seconds, &tm_time);
    if(show_microseconds)
    {
        int microseconds = static_cast<int>(micro_seconds_since_epoch_ % kMicroSecondsPerSecond);
        std::memcpy(buf , digits2(static_cast<size_t>((1900+tm_time.tm_year)/100)), 2);
        format_time(buf + 2,
                        static_cast<unsigned>(tm_time.tm_year%100), 
                        static_cast<unsigned>(tm_time.tm_mon+1), 
                        static_cast<unsigned>(tm_time.tm_mday), '-');
        buf[10] = ' ';
        format_time(buf + 11, 
                        static_cast<unsigned>(tm_time.tm_hour), 
                        static_cast<unsigned>(tm_time.tm_min), 
                        static_cast<unsigned>(tm_time.tm_sec), ':');
        buf[19] = '.';
        char* usec_end = &buf[26];
        for(int i{0}; i < 3; ++i)
        {
            usec_end -= 2;
            std::memcpy(usec_end, digits2(static_cast<size_t>(microseconds%100)), 2);
            microseconds /= 100;
        }
    }
    else
    {
        int microseconds = static_cast<int>(micro_seconds_since_epoch_ % kMicroSecondsPerSecond);
        std::memcpy(buf , digits2(static_cast<size_t>((1900+tm_time.tm_year)/100)), 2);
        format_time(buf + 2,
                        static_cast<unsigned>(tm_time.tm_year%100), 
                        static_cast<unsigned>(tm_time.tm_mon+1), 
                        static_cast<unsigned>(tm_time.tm_mday), '-');
        buf[10] = ' ';
        format_time(buf + 11, 
                        static_cast<unsigned>(tm_time.tm_hour), 
                        static_cast<unsigned>(tm_time.tm_min), 
                        static_cast<unsigned>(tm_time.tm_sec), ':');
    }
    return buf;
}

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
