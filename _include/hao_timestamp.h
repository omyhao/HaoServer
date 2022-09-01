#ifndef _HAO_TIMESTAMP_H_
#define _HAO_TIMESTAMP_H_

#include <string>
#include <cstdint>
#include <chrono>

using std::string;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

class Timestamp
{
    public:
        Timestamp();
        Timestamp(int64_t microseconds);
        static const int InvalidTime = -1;
        
        friend Timestamp operator+(const Timestamp a, const std::chrono::microseconds& b);
        friend Timestamp operator+(const Timestamp a, const std::chrono::milliseconds& b);
        friend Timestamp operator+(const Timestamp a, const std::chrono::seconds& b);
        friend Timestamp operator+(const Timestamp a, const std::chrono::minutes& b);
        friend Timestamp operator+(const Timestamp a, const std::chrono::hours& b);
        friend Timestamp operator+(const Timestamp a, const int64_t micro);
        Timestamp operator-(const Timestamp b);
        Timestamp& operator+=(const Timestamp b);
        
        Timestamp& operator+=(const hours hours_);
        Timestamp& operator+=(const minutes minutes_);
        Timestamp& operator+=(const seconds sec);
        Timestamp& operator+=(const milliseconds mill);
        Timestamp& operator+=(const microseconds micro);

        int64_t Milliseconds() const;
        int64_t Microseconds() const;

        friend bool operator <(const Timestamp a, const Timestamp b);
        friend bool operator >(const Timestamp a, const Timestamp b);
        friend bool operator <=(const Timestamp a, const Timestamp b);
        friend bool operator >=(const Timestamp a, const Timestamp b);
        friend bool operator ==(const Timestamp a, const Timestamp b);
        friend bool operator !=(const Timestamp a, const Timestamp b);

        friend bool operator < (const Timestamp a, const milliseconds b);

        void swap(Timestamp& that);
        string ToString() const;
        string ToFormattedString(bool show_microseconds) const;
        static Timestamp now();
        static const int64_t kMicroSecondsPerSecond = 1000 * 1000;

    private:
        int64_t micro_seconds_since_epoch_;
};

#endif