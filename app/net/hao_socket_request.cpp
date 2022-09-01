#include "hao_socket.h"
#include "hao_log.h"
#include "hao_common.h"
#include "hao_memory.h"
#include "hao_global.h"
#include "hao_logic.h"

#include <string.h>

using namespace hao_log;

void Socket::ReadRequestHandler(Connection* conn)
{
    LOG_INFO << "进了ReadRequestHandler";
    bool is_flood {false};
    ssize_t reco = RecvProc(conn, conn->precv_buf, conn->recv_len);
    // 客户端关闭或出现其他问题
    if(reco <= 0)
    {
        return;
    }
    // 有数据了
    if(conn->cur_stat == PkgState::Head_Init)
    {
        LOG_INFO << "当前状态:Head_Init";
        // 如果收到了完整的包头
        if(reco == pkg_header_len_)
        {
            LOG_INFO << "收到了完整的包头";
            WaitRequestHandlerProcP1(conn, is_flood);
        }
        else
        {
            LOG_INFO << "收到的头不完整，要接着收";
            // 收到的包头不完整，内存和要收的数量要即时调整
            conn->cur_stat = PkgState::Haed_Recving;
            conn->precv_buf = conn->precv_buf + reco;
            conn->recv_len = conn->recv_len - reco;
        }
    }
    else if(conn->cur_stat == PkgState::Haed_Recving)
    {
        LOG_INFO << "当前状态:Head_Recving";
        if(conn->recv_len == reco)
        {
            // 包头收完整了
            LOG_INFO << "第二个elseif中收到了完整的包头";
            WaitRequestHandlerProcP1(conn, is_flood);
        }
        else
        {
            LOG_INFO << "接着收包头";
            conn->precv_buf = conn->precv_buf + reco;
            conn->recv_len = conn->recv_len - reco;
        }
    }
    else if(conn->cur_stat == PkgState::Body_Init)
    {
        LOG_INFO << "当前状态:Body_Init";
        // 包头刚好收完，开始收包体
        if(reco == conn->recv_len)
        {
            // 收到完整的包体了
            LOG_INFO << "正好收到了完整的包体";
            if(flood_ak_enable_)
            {
                is_flood = TestFlood(conn);
            }
            WaitRequestHandlerProcPlast(conn, is_flood);
        }
        else
        {
            conn->cur_stat = PkgState::Body_Recving;
            conn->precv_buf = conn->precv_buf + reco;
            conn->recv_len = conn->recv_len - reco;
        }
    }
    else if(conn->cur_stat == PkgState::Body_Recving)
    {
        LOG_INFO << "当前状态:Body_Recving";
        if(conn->recv_len == reco)
        {
            // 包体收完整了
            if(flood_ak_enable_)
            {
                is_flood = TestFlood(conn);
            }
            WaitRequestHandlerProcPlast(conn, is_flood);
        }
        else
        {
            conn->precv_buf = conn->precv_buf + reco;
            conn->recv_len = conn->recv_len - reco;
        }
    }
    LOG_INFO << "收完了";
    if(is_flood == true)
    {
        // 客户端flood，则直接关闭客户端
        zd_close_socket_proc(conn);
    }

}

ssize_t Socket::RecvProc(Connection* conn,  char *buf, ssize_t buf_len)
{
    LOG_INFO << "准备收数据了,要收的数据长度为:" << buf_len;
    ssize_t n{0};
    // 参数为0的时候，基本等同于read
    n = recv(conn->fd, buf, buf_len, 0);
    LOG_INFO << "收到的数据长度:" << n;
    if(n == 0)
    {
        // 客户端关闭
        LOG_INFO << "客户端关闭了连接";
        zd_close_socket_proc(conn);
        return 0;
    }
    if(n < 0)
    {
        // 没有收到数据，LT模式一般不会出现这个错误
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_ERROR << "RecvProc中errno == EAGAIN || errno == EWOULDBLOCK成立";
            return -1;
        }
        // 被信号打断了，直接返回
        if(errno == EINTR)
        {
            LOG_INFO << "RecvProc中errno == EINTR成立";
            return -1;
        }
        // 下面的错误都属于异常了，意味着要关闭客户端套接字到连接池中
        if(errno == ECONNRESET)
        {
            // 客户端没有4次挥手直接关闭了程序，则会给服务器发rst包
            // FIXME
            // do nothing
        }
        else
        {
            if(errno == EBADF)
            {
                // FIXME

            }
            else
            {
                LOG_ERROR << "RecvProc中发生错误";
            }
        }
        zd_close_socket_proc(conn);
    }
    return n;
}

void Socket::WaitRequestHandlerProcP1(Connection* p_conn, bool& is_flood)
{
    Memory& memory = Memory::GetInstance();

    PkgHeader* header = (PkgHeader*)p_conn->head_info;
    uint16_t pkg_len = ntohs(header->pkg_len);
    LOG_INFO << "包长度:" << pkg_len;
    // 恶意包或错包的判断
    if(pkg_len < pkg_header_len_ || pkg_len > (PKG_MAX_LENGTH - pkg_header_len_))
    {
        p_conn->cur_stat = PkgState::Head_Init;
        p_conn->precv_buf =  p_conn->head_info;
        p_conn->recv_len = pkg_header_len_;
    }
    else
    {
        LOG_INFO << "这是一个合法的包头";
        // 合法的包头
        // 分配的内存大小为:消息头+包总大小(包头+包体)
        LOG_INFO << "要申请的大小:msg_header_len" << msg_header_len_ << " pkg_len:" << pkg_len;
        char *p_temp_buffer = (char*)memory.AllocMemory(msg_header_len_ + pkg_len, false);
        LOG_INFO << "申请到的内存地址为:" << (void*)p_temp_buffer;
        p_conn->precv_mem_pointer = p_temp_buffer;

        // 写消息头部
        MsgHeader* p_temp_msg_header = (MsgHeader*)p_temp_buffer;
        p_temp_msg_header->conn = p_conn;
        p_temp_msg_header->cur_sequence_num = p_conn->sequence_num;

        // 写包头内容
        p_temp_buffer += msg_header_len_;
        // 把收到的包头拷贝过来
        memcpy(p_temp_buffer, header, pkg_header_len_);
        if(pkg_len ==  pkg_header_len_)
        {
            // 只有包头的报文
            if(flood_ak_enable_)
            {
                is_flood = TestFlood(p_conn);
            }
            // 处理该条消息
            WaitRequestHandlerProcPlast(p_conn, is_flood);
        }
        else
        {
            LOG_INFO << "开始收包体";
            // 开始收包体
            p_conn->cur_stat = PkgState::Body_Init;
            p_conn->precv_buf = p_temp_buffer + pkg_header_len_;
            p_conn->recv_len = pkg_len - pkg_header_len_;
        }
    }
}

void Socket::WaitRequestHandlerProcPlast(Connection* p_conn, bool &is_flood)
{
    if(is_flood == false)
    {
        LOG_INFO << "is_flood:" << is_flood;
        LOG_INFO << "要把p_conn中的内存块发送到线程池中了";
        g_threadpool.PushTask(&LogicSocket::HandleMessage, &g_logic_socket, p_conn->precv_mem_pointer);
    }
    else
    {
        // FIXME 这里是flood攻击，是否需要主动关闭socket
        // FIXED,flood攻击的时候，直接把fd关闭了
        LOG_INFO << "检测到了flood攻击, 要关闭该连接了" << p_conn->client_addr.ToIPPort();
        Memory& memory = Memory::GetInstance();
        memory.FreeMemory(p_conn->precv_mem_pointer);
        zd_close_socket_proc(p_conn);   
        LOG_INFO << "连接关闭成功";
    }
    p_conn->precv_mem_pointer = nullptr;
    p_conn->cur_stat = PkgState::Head_Init;
    p_conn->precv_buf = p_conn->head_info;
    p_conn->recv_len = pkg_header_len_;
}

ssize_t Socket::SendProc(Connection* p_conn, char* buff, ssize_t size)
{
    ssize_t n{0};
    for(;;)
    {
        n = send(p_conn->fd, buff, size, 0);
        if(n > 0)
        {
            return n;
        }
        if(n == 0)
        {
            // send 0表示对端关闭了连接
            LOG_INFO << "对段关闭了!!!";
            return 0;
        }
        if(errno == EAGAIN)
        {
            // 内核缓冲区满了
            return -1;
        }
        if(errno == EINTR)
        {
            // 被信号打断了
            LOG_INFO << "Socket::SendProc()->send() failed";
        }
        else
        {
            return -2;
        }
    }
}

void Socket::WriteRequestHandler(Connection* p_conn)
{
    Memory& memory = Memory::GetInstance();

    ssize_t send_size = SendProc(p_conn, p_conn->send_buf, p_conn->send_len);
    if(send_size > 0 && send_size != p_conn->send_len)
    {
        // 没有全发完
        p_conn->send_buf += send_size;
        p_conn->send_len -= send_size;
        return;
    }
    else if(send_size == -1)
    {
        LOG_ERROR << "Socket::WriteRequestHandler()->send_size == -1";
        return;
    }
    if(send_size > 0 && send_size == p_conn->send_len)
    {
        // 数据全发完了，则移除可写事件
        if(Epoll_Oper_Event(
                p_conn->fd,
                EPOLL_CTL_MOD,
                EPOLLOUT,
                1,              // 0:增加，1:去掉，2:完全覆盖
                p_conn
            ) == -1)
        {
            LOG_ERROR << "Socket::WriteRequestHandler()->Epoll_Oper_Event() failed";
        }
    }
    memory.FreeMemory(p_conn->send_mem_pointer);
    p_conn->send_mem_pointer = nullptr;
    --p_conn->throw_send_count;
}