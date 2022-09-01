#include "hao_socket.h"
#include "hao_algorithm.h"
#include "hao_log.h"
#include "hao_global.h"

#include <unistd.h>

using namespace hao_log;

void Socket::EventAccept(Connection *old_connection)
{
    static int          use_accept4_{1};
    struct sockaddr_in6 client_addr;
    Connection  *new_conn {nullptr};
    MemZero(&client_addr, sizeof(client_addr));
    socklen_t addr_len = static_cast<socklen_t>(sizeof(client_addr));
    int client_sock_fd{-1};
    int err_code{0};
    LogLevel level;
    do
    {
        if(use_accept4_)
        {
            client_sock_fd = accept4(old_connection->fd, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        }
        else
        {
            client_sock_fd = accept(old_connection->fd, (sockaddr*)&client_addr, &addr_len);
        }
        
        LOG_DEBUG << "惊群测试, 进程id:" << pid;
        if(client_sock_fd == -1)
        {
            err_code = errno;
            // accept未准备好
            if(err_code == EAGAIN)
            {
                return;
            }
            level = LogLevel::ALERT;
            // 对端意外关闭套接字
            if(err_code == ECONNABORTED)
            {
                level = LogLevel::ERROR;
            }
            // 进程的fd用尽了
            else if (err_code == EMFILE || err_code == ENFILE)
            {
                level = LogLevel::CRIT;
            }
            LOG(level) << "HEpoll::EventAccept() accept4() failed";
            // accept4没实现
            if(use_accept4_ && err_code == ENOSYS)
            {
                use_accept4_ = 0;
                continue;
            }
            if(err_code == ECONNABORTED)
            {
                // 对方关闭套接字
            }
            if(err_code = EMFILE || err_code == ENFILE)
            {
                // FIXME
                // 使用一个idle_fd来接收
            }
            return;
            
        }
        // 走到这里，表明accetpt成功了
        // 超过了最大连接数了
        if(online_user_count_ >= worker_connections_)
        {
            close(client_sock_fd);
            return;
        }
        // FIXME 再想一下这里的进一步判断是否有必要
        // 恶意用户连上来发了1条数据就断开，不断连接，就会导致频繁调用GetConnection使得
        // 短时间内产生大量连接，危及服务器
        if(connection_pool_.size() > (worker_connections_ * 5))
        {
            //  比如你允许同时最大2048个连接，但连接池却有了 2048*5这么大的容量，
            // 这肯定是表示短时间内 产生大量连接/断开，
            // 因为我们的延迟回收机制，这里连接还在垃圾池里没有被回收
            if(free_connection_pool_.size() < worker_connections_)
            {
                close(client_sock_fd);
                return;
            }
        }
        new_conn = GetConnection(client_sock_fd);
        if(new_conn == nullptr)
        {
            // 连接池中连接不够用了
            if(close(client_sock_fd) == -1)
            {
                LOG_ALERT << "HEpoll::EventAccept() close(" << client_sock_fd << ") failed";
                return;
            }
        }
        // 成功拿到了连接池中的连接
        new_conn->client_addr.set_sockaddr(client_addr);
        LOG_INFO << "客户端fd:"<< client_sock_fd << " ip:" << new_conn->client_addr.ToIPPort() << "连接成功";
        if(!use_accept4_)
        {
            if(SetNonBlocking(client_sock_fd) == false)
            {
                // 设置非阻塞失败，归还连接
                CloseConnection(new_conn);
                return;
            }
        }
        // 设置连接绑定的监听端口
        new_conn->listening_ptr = old_connection->listening_ptr;
        new_conn->read_handler = &Socket::ReadRequestHandler;
        new_conn->write_handler = &Socket::WriteRequestHandler;
        if(Epoll_Oper_Event(
                    client_sock_fd,             // 客户端socket
                    EPOLL_CTL_ADD,              // 添加
                    EPOLLIN | EPOLLRDHUP,        // EPOLLIN可读，EPOLLRDHUP远端关闭
                    0,                          // 额外参数
                    new_conn                    // 连接池中的连接
                    ) == -1)
        {
            CloseConnection(new_conn);
            return;
        }
        LOG_INFO << "要开启踢人功能么:" << ifkickTimeCount;
        if(ifkickTimeCount)
        {
            LOG_INFO << "开始加入timer_queue";
            AddToTimerQueue(new_conn);
            LOG_INFO << "加入timer_queue结束";
        }

        ++online_user_count_;                   // 在线用户+1
        break;
    } while (1);
}