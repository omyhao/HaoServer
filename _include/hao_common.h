#ifndef _HAO_COMMON_H_
#define _HAO_COMMON_H_

#include <cstdint>

// 包最大长度 = 包头 + 包体
constexpr int PKG_MAX_LENGTH {30000};
// 包头的最大长度，最大长度要大于 Pkg_Header
constexpr int DATA_BUFSIZE {20};

enum class PkgState
{
    Head_Init,
    Haed_Recving,
    Body_Init,
    Body_Recving
};
class Connection;

struct MsgHeader
{
    Connection* conn;
    uint64_t    cur_sequence_num;
};

#pragma pack(push, 1)
struct PkgHeader
{
    // value (0 - 2^16-1) = (0 - 65535)
    uint16_t pkg_len;
    uint16_t msg_code;
    uint32_t crc32;
};
#pragma pack(pop)

constexpr int kMsgHeaderSize{sizeof(MsgHeader)};
constexpr int kPkgHeaderSize{sizeof(PkgHeader)};

#endif