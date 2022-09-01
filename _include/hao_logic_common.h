#ifndef _HAO_LOGIC_COMMON_H_
#define _HAO_LOGIC_COMMON_H_

const       int CMD_START{0};
constexpr   int CMD_PING{CMD_START};
constexpr   int CMD_REGISTER{CMD_START + 5};
constexpr   int CMD_LOGIN{CMD_START + 6};

// 结构定义
#pragma pack(push, 1)

struct Register
{
    uint32_t    type;
    char        username[56];
    char        password[40];
};

struct Login
{
    char    username[56];
    char    password[40];
};

#pragma pack(pop)

#endif