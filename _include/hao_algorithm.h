#ifndef _HAO_ALGORITHM_H_
#define _HAO_ALGORITHM_H_

#include <algorithm>
#include <locale>
#include <cctype>
#include <cstring>

static inline void LeftTrim(std::string &content)
{
    content.erase(content.begin(), std::find_if(content.begin(), content.end(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }));
}

static inline void RightTrim(std::string &content)
{
    content.erase(std::find_if(content.rbegin(), content.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }).base(), content.end());
}

static inline void Trim(std::string &content)
{
    LeftTrim(content);
    RightTrim(content);
}

inline void MemZero(void* ptr, size_t size)
{
    memset(ptr, 0, size);
}

uint32_t GetCRC(const unsigned char *data, size_t len);

#endif