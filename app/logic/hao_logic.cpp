#include "hao_logic.h"
#include "hao_global.h"
#include "hao_common.h"
#include "hao_memory.h"
#include "hao_algorithm.h"
#include "hao_log.h"
#include "hao_logic_common.h" 

#include <mutex>
#include <functional>
using std::lock_guard;
using std::mutex;
using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

using namespace hao_log;

using handler = bool(LogicSocket::*)(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length);

static const handler status_handler[]
{
    &LogicSocket::HandlePing,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    // 业务逻辑
    &LogicSocket::HandleRegister,
    &LogicSocket::HandleLogin,
};

constexpr int kTotalCommands{sizeof(status_handler)/ sizeof(handler)};

// FIXME
// 线程池里也进行了二次bind，考虑效率问题
LogicSocket::LogicSocket()
{

}
LogicSocket::~LogicSocket()
{

}

// 由线程池来调用
// 线程池来不断的对消息队列中的消息进行处理
void LogicSocket::HandleMessage(char *p_msg_buf)
{
    LOG_INFO << "到了HandleMessage里边了";
    LOG_INFO << "要处理的消息内存地址:" << (void*)p_msg_buf;
    Memory& memory = Memory::GetInstance();
    MsgHeader* p_msg_header = (MsgHeader*)p_msg_buf;
    PkgHeader* p_pkg_header = (PkgHeader*)(p_msg_buf + sizeof(MsgHeader));
    void *p_pkg_body{nullptr};
    uint16_t pkg_len = ntohs(p_pkg_header->pkg_len);
    LOG_INFO << "得到的pkg_len为" << pkg_len;
    if(pkg_len == kPkgHeaderSize)
    {
        // 只有包头
        if(p_pkg_header->crc32 != 0)
        {
            // 只有包头的数据包crc会给0
            memory.FreeMemory(p_msg_buf);
            return;
        }
    }
    else
    {
        // 有包体
        p_pkg_header->crc32 = ntohl(p_pkg_header->crc32);
        LOG_INFO << "crc32值为:" << p_pkg_header->crc32;
        // 跳过消息头，包头
        p_pkg_body = (void*)(p_msg_buf + kMsgHeaderSize + kPkgHeaderSize);
        uint32_t calc_crc = GetCRC((unsigned char*)p_pkg_body, pkg_len-kPkgHeaderSize);
        LOG_INFO << "计算得到的crc值为:" << calc_crc;
        if(calc_crc != p_pkg_header->crc32)
        {
            LOG_INFO << "数据包CRC验证错误";
            memory.FreeMemory(p_msg_buf);
            LOG_INFO << "这里其实应该主动关闭connection";
            return;
        }
    }
    uint16_t msg_code = ntohs(p_pkg_header->msg_code);
    LOG_INFO << "msg_code:" << msg_code;
    Connection* p_conn = p_msg_header->conn;
    if(p_conn->sequence_num != p_msg_header->cur_sequence_num)
    {
        memory.FreeMemory(p_msg_buf);
        return;
    }
    if(msg_code >= kTotalCommands)
    {
        LOG_INFO << "msg_code can't find:" << msg_code;

    }
    LOG_INFO << "数据全都正确了,开始具体的处理方法了";
    (this->*status_handler[msg_code])(p_conn, p_msg_header, (char*)p_pkg_body, pkg_len-kPkgHeaderSize);
    Memory& mem_instance = Memory::GetInstance();
    mem_instance.FreeMemory(p_msg_buf); 
    LOG_INFO << "内存:" << (void*)p_msg_buf << "被释放了,没有泄漏";
}

bool LogicSocket::HandleRegister(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length)
{
    LOG_INFO << "到了HandleRegister中";
    if(p_pkg_body == nullptr)
    {
        return false;
    }
    int recv_len = sizeof(Register);
    if(recv_len != body_length)
    {
        return false;
    }
    lock_guard<mutex> logic_mutex{p_conn->logic_proc_mutex};
    Register *p_recv_info = (Register*)p_pkg_body;
    p_recv_info->type = ntohl(p_recv_info->type);
    LOG_INFO << "username size  :" << sizeof(p_recv_info->username);
    LOG_INFO << "username strlen:" << strlen(p_recv_info->username);
    LOG_INFO << "password size  :" << sizeof(p_recv_info->password);
    LOG_INFO << "password strlen:" << strlen(p_recv_info->password);
    // LOG_INFO << "type:" << p_recv_info->type;
    // LOG_INFO << "username:" << p_recv_info->username;
    // LOG_INFO << "password:" << p_recv_info->password;
    // 保证数据的绝对安全，如果名字超过了数组长度，要手动把数组最后一位设置为0
    // 这样也会改变发送到客户端的crc的值
    p_recv_info->username[sizeof(p_recv_info->username)-1]= 0;
    p_recv_info->password[sizeof(p_recv_info->password)-1]= 0;
    LOG_INFO << "type:" << p_recv_info->type;
    LOG_INFO << "username:" << p_recv_info->username;
    LOG_INFO << "password:" << p_recv_info->password;

    Memory& memory = Memory::GetInstance();
    int send_len = sizeof(Register);
    char *p_send_buf = (char*)memory.AllocMemory(kMsgHeaderSize+kPkgHeaderSize+send_len,true);
    LOG_INFO << "发送消息的内存块地址:" << (void*)p_send_buf;
    // 填充消息头
    memcpy(p_send_buf, p_msg_header, kMsgHeaderSize);
    PkgHeader* p_pkg_header = (PkgHeader*)(p_send_buf+kMsgHeaderSize);
    p_pkg_header->msg_code = htons(CMD_REGISTER);
    p_pkg_header->pkg_len = htons(kPkgHeaderSize+send_len);

    // 填充消息体
    Register* p_send_info = (Register*)(p_send_buf+kMsgHeaderSize+kPkgHeaderSize);
    // 把消息原封不动的发送回去
    p_recv_info->type = htonl(p_recv_info->type);
    memcpy(p_send_info, p_recv_info, sizeof(Register));
    p_pkg_header->crc32 = htonl(GetCRC((unsigned char*)p_send_info, send_len));
    LOG_INFO << "要发送包的crc32:" << ntohl(p_pkg_header->crc32);
    
    // 把数据包发送出去
    g_socket.MsgSend(p_send_buf);
    return true;
}
bool LogicSocket::HandleLogin(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length)
{
    LOG_INFO << "到了HandleLogin中";
    if(p_pkg_body == nullptr)
    {
        return false;
    }
    int recv_len = sizeof(Login);
    if(recv_len != body_length)
    {
        return false;
    }
    lock_guard<mutex> logic_mutex{p_conn->logic_proc_mutex};
    Login *p_recv_info = (Login*)p_pkg_body;
    LOG_INFO << "登陆的信息:";
    LOG_INFO << "username size  :" << sizeof(p_recv_info->username);
    LOG_INFO << "username strlen:" << strlen(p_recv_info->username);
    LOG_INFO << "password size  :" << sizeof(p_recv_info->password);
    LOG_INFO << "password strlen:" << strlen(p_recv_info->password);
    // LOG_INFO << "type:" << p_recv_info->type;
    // LOG_INFO << "username:" << p_recv_info->username;
    // LOG_INFO << "password:" << p_recv_info->password;
    // 保证数据的绝对安全，如果名字超过了数组长度，要手动把数组最后一位设置为0
    // 这样也会改变发送到客户端的crc的值
    p_recv_info->username[sizeof(p_recv_info->username)-1]= 0;
    p_recv_info->password[sizeof(p_recv_info->password)-1]= 0;
    LOG_INFO << "username:" << p_recv_info->username;
    LOG_INFO << "password:" << p_recv_info->password;

    Memory& memory = Memory::GetInstance();
    int send_len = sizeof(Login);
    char *p_send_buf = (char*)memory.AllocMemory(kMsgHeaderSize+kPkgHeaderSize+send_len,true);
    LOG_INFO << "发送消息的内存块地址:" << (void*)p_send_buf;
    // 填充消息头
    memcpy(p_send_buf, p_msg_header, kMsgHeaderSize);
    PkgHeader* p_pkg_header = (PkgHeader*)(p_send_buf+kMsgHeaderSize);
    p_pkg_header->msg_code = htons(CMD_LOGIN);
    p_pkg_header->pkg_len = htons(kPkgHeaderSize+send_len);

    // 填充消息体
    Login* p_send_info = (Login*)(p_send_buf+kMsgHeaderSize+kPkgHeaderSize);
    // 把消息原封不动的发送回去
    memcpy(p_send_info, p_recv_info, sizeof(Login));
    p_pkg_header->crc32 = htonl(GetCRC((unsigned char*)p_send_info, send_len));
    LOG_INFO << "要发送包的crc32:" << ntohl(p_pkg_header->crc32);
    
    // 把数据包发送出去
    g_socket.MsgSend(p_send_buf);
    LOG_INFO << "登陆成功";
    return true;
}
bool LogicSocket::HandlePing(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length)
{
    // 心跳包不可以有包体
    if(body_length != 0)
    {
        return false;
    }
    g_socket.UpdateTimer(p_conn, Timestamp::now());
    return true;
}

void SendBodyPkgToClient(MsgHeader* p_msg_header, unsigned short msg_code)
{

}