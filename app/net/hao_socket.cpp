#include "hao_socket.h"
#include "hao_config.h"
#include "hao_log.h"
#include "hao_algorithm.h"
#include "hao_memory.h"
#include "hao_global.h"

#include <unistd.h>
#include <sys/ioctl.h>

#include <mutex>
using std::lock_guard;
using std::unique_lock;

using namespace hao_log;
Socket::Socket():
    running_{false},                                        // 默认进程没有运行  
    worker_connections_             {1024},                 // 单进程最大连接数
    listen_port_count_              {1},                    // 监听端口数
    recycle_connection_wait_time_   {60},                   // 回收连接等待的秒数
    epoll_handle_                   {-1},                   // epoll_fd
    pkg_header_len_                 {kPkgHeaderSize},    // 包头的大小
    msg_header_len_                 {kMsgHeaderSize},    // 消息头的大小
    discard_send_pkg_count_         {0},                    // 丢弃的数据包量
    online_user_count_              {0},                    // 在线用户数量
    last_print_time_                {0}                     // 上次打印统计信息的时间
{
    
}
Socket::Socket(MessageCallback message_callback, PingOutCallback ping_out_callback):
    message_callback_{move(message_callback)},
    ping_out_callback_{move(ping_out_callback)},
    running_{false},                                        // 默认进程没有运行  
    worker_connections_             {1024},                 // 单进程最大连接数
    listen_port_count_              {1},                    // 监听端口数
    recycle_connection_wait_time_   {60},                   // 回收连接等待的秒数
    epoll_handle_                   {-1},                   // epoll_fd
    pkg_header_len_                 {kPkgHeaderSize},    // 包头的大小
    msg_header_len_                 {kMsgHeaderSize},    // 消息头的大小
    discard_send_pkg_count_         {0},                    // 丢弃的数据包量
    online_user_count_              {0},                    // 在线用户数量
    last_print_time_                {0}                     // 上次打印统计信息的时间
{

}

Socket::~Socket()
{
    
}

// 初始化函数，fork之前调用
bool Socket::Initialize()
{
    // 读配置文件
    ReadConfig();
    
    return OpenListeningSockets();
}

void Socket::ReadConfig()
{
    Config& config                  = Config::GetInstance();
    worker_connections_             = static_cast<int>(config["Net"]["WorkerConnections"]);
    recycle_connection_wait_time_   = std::chrono::seconds((int)config["Net"]["RecycleConnectionWaitTime"]);
    ifkickTimeCount                 = static_cast<bool>(config["Net"]["WaitTimeEnable"]);
    wait_time_                      = seconds(std::max(5, (int)config["Net"]["MaxWaitTime"]));
    ifTimeOutKick                   = static_cast<bool>(config["Net"]["TimeOutKick"]);
    
    flood_ak_enable_                = static_cast<bool>(config["Security"]["FloodAttackKickEnable"]);
    flood_time_interval_            = std::chrono::milliseconds(static_cast<int>(config["Security"]["FloodTimeInterval"]));
    flood_kick_count_               = static_cast<int>(config["Security"]["FloodKickCounter"]);
    LOG_INFO << "配置项加载完毕";
}

bool Socket::OpenListeningSockets()
{
    struct sockaddr_in server_address;
    MemZero(&server_address, sizeof(server_address));
    
    Config& config = Config::GetInstance();
    LOG_INFO << "从配置文件中读取到的监听数:" << config["Net"]["Listen"].size();
    for(int i = 0; i < config["Net"]["Listen"].size(); i++)
    {
        AddressType  address_type= (bool)config["Net"]["Listen"][i]["Any"]?AddressType::Any : AddressType::Loopback;
        IpType ip_type = (bool)config["Net"]["Listen"][i]["ipv4"]?IpType::Ipv4:IpType::Ipv6;
        InternetAddress address {static_cast<uint16_t>((int)config["Net"]["Listen"][i]["ListenPort"]), address_type, ip_type};
        LOG_INFO << address.ToIPPort();
        int socket_fd = ::socket(address.Family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        if(-1 == socket_fd )
        {
            LOG_ERROR << "Epoll::OpenListeningSockets()::socket failed";
            return false;
        }
        int reuseaddr = 1;
        if(-1 == ::setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&reuseaddr, sizeof(reuseaddr)))
        {
            LOG_ERROR << "Epoll::OpenListeningSockets()::setsockopt reuseaddr failed";
            close(socket_fd);
            return false;
        }

        // 为处理惊群问题使用reuseport
        int reuseport{1};
        if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, (const void*)&reuseport, sizeof(reuseport)))
        {
            LOG_ERROR << "Epoll::OpenListeningSockets()::setsockopt reuseaddr failed";
        }

        if(-1 == ::bind(socket_fd, address.SockAddr(), address.Size()))
        {
            string info{strerror(errno)};
            LOG_INFO << info;
            LOG_ERROR << "Epoll::OpenListeningSockets()::bind() failed";
            close(socket_fd);
            return false;
        }
        if(-1 == ::listen(socket_fd, SOMAXCONN))
        {
            LOG_ERROR << "Epoll::OpenListeningSockets()::listen() failed";
            close(socket_fd);
            return false;
        }
        listen_socket_list_.emplace_back(socket_fd, address);
    }
    LOG_INFO << "监听成功";
    return true;
}

bool Socket::SetNonBlocking(int sock_fd)
{
    int non_block{1};
    if(ioctl(sock_fd, FIONBIO, &non_block) == -1)
    {
        return false;
    }
    return true;
}

void Socket::CloseListeningSockets()
{
    for(int i = 0; i < listen_socket_list_.size(); i++)
    {
        close(listen_socket_list_[i].sockfd);
        LOG_INFO << "关闭监听端口"  << listen_socket_list_[i].listen_address.Port();
    }
}

void Socket::set_message_callback(MessageCallback message_callback)
{
    message_callback_ = move(message_callback);
}

void Socket::set_ping_out_callback(PingOutCallback ping_out_callback)
{
    ping_out_callback_ = move(ping_out_callback);
}

int Socket::Epoll_init()
{
    epoll_handle_ = epoll_create1(EPOLL_CLOEXEC);
    if(epoll_handle_ == -1)
    {
        LOG_ERROR << "Socket::Epoll_init()::Epoll_init() failed";
        exit(EXIT_FAILURE);
    }
    LOG_INFO << pid << "创建的epoll_fd:" << epoll_handle_;
    InitConnectionPool();
    LOG_INFO << "要监听的端口数目:" << listen_socket_list_.size();
    for(auto it = listen_socket_list_.begin(); it != listen_socket_list_.end(); ++it)
    {
        Connection* p_conn = GetConnection((*it).sockfd);
        if(p_conn == nullptr)
        {
            LOG_ERROR << "Socket::Epoll_init()::GetConnection() failed";
            exit(EXIT_FAILURE);
        }
        p_conn->listening_ptr = &(*it);
        (*it).connection_ptr = p_conn;
        p_conn->read_handler = &Socket::EventAccept;
        LOG_INFO << "要把fd:" << (*it).sockfd << "添加到epoll中";
        if(Epoll_Oper_Event(
            (*it).sockfd,
            EPOLL_CTL_ADD,
            EPOLLIN | EPOLLRDHUP,
            0,
            p_conn
            ) == -1)
        {
            exit(EXIT_FAILURE);
        }
        LOG_INFO << "添加fd:" <<(*it).sockfd << "到epoll中成功";
    }
    return 1;
}

int Socket::Epoll_Oper_Event(int fd, uint32_t event_type, uint32_t flag, int bcaction, Connection *p_conn)
{
    struct epoll_event ev;
    MemZero(&ev, sizeof(ev));
    if(event_type == EPOLL_CTL_ADD)
    {
        ev.events = flag;
        p_conn->events = flag;
    }
    else if(event_type == EPOLL_CTL_MOD)
    {
        ev.events = p_conn->events;
        if(bcaction == 0)
        {
            ev.events |= flag;
        }
        else if( bcaction == 1)
        {
            ev.events &= ~flag;
        }
        else
        {
            ev.events = flag;
        }
        p_conn->events = ev.events;
    }
    else
    {
        return 1;
    }
    ev.data.ptr = static_cast<void*>(p_conn);
    if(epoll_ctl(epoll_handle_, event_type, fd, &ev) == -1)
    {
        LOG_ERROR << "epoll_ctl failed";
        return -1;
    }
    LOG_INFO << fd << "添加到了EPOLL红黑树中";
    return 1;
}

// 1:非正常返回，0:正常返回
void Socket::Epoll_Process_Events()
{
    int timeout{-1}, events{0};
    for(;;)
    {
        if(timer_.Empty())
        {
            LOG_INFO << "定时器为空";
            events = epoll_wait(epoll_handle_, events_, MAX_EVENTS, -1);
        }
        else
        {
            LOG_INFO << "定时器不为空:size()" << timer_.Size() << "开始处理定时器事件";
            timeout = TimerHeartBeatCheck();
            events = epoll_wait(epoll_handle_, events_, MAX_EVENTS, timeout);
        }
        LOG_INFO << "epoll被激活了:" << events << "个事件";
        if(events == -1)
        {
            std::cerr << epoll_handle_ << " " << strerror(errno) << std::endl;
            // 被信号打断
            if(errno == EINTR)
            {
                LOG_INFO << "epoll_wait returned EINTR";
                continue;
            }
            else
            {
                LOG_ALERT << "epoll_wait failed";
                break;
            }
        }
        else if(events == 0)    
        {
            // 超时，但是没事件 
            if(timeout == -1)
            {
                LOG_ALERT << "epoll_wait timeout & no events returned";
                break;
            }
            
        }
        else
        {
            Connection* p_conn {nullptr};
            uint32_t revents{0};
            for(int i{0}; i < events; ++i)
            {
                p_conn = (Connection*)(events_[i].data.ptr);
                revents = events_[i].events;
                if(revents & EPOLLRDHUP)
                {
                    LOG_INFO << "客户端关闭了";
                }
                if(revents &(EPOLLERR | EPOLLHUP))
                {
                    revents |= EPOLLIN | EPOLLOUT;
                }
                if(revents & (EPOLLIN|EPOLLPRI|EPOLLRDHUP))
                {
                    // 如果是新连接，则调用的是Socket::EventAccept
                    // 如果是已有连接，则调用的是Socket::ReadRequestHandler
                    LOG_INFO << "触发了EPOLLIN";
                    (this->*(p_conn->read_handler))(p_conn);
                }
                if(revents & EPOLLOUT)
                {
                    LOG_INFO << "触发了EPOLLOUT";
                    if(revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                    {
                        --p_conn->throw_send_count;
                    }
                    else
                    {
                        (this->*(p_conn->write_handler))(p_conn);
                    }
                }
            }
             LOG_INFO << "io事件处理完了，开始下一波";
        }
       
    }
    
}

void Socket::MsgSend(char *p_send_buf)
{
    LOG_INFO << "要发送的消息块地址:" << (void*)p_send_buf;
    Memory& memory = Memory::GetInstance();
    
    // 发送消息队列中消息太多了
    if(send_message_queue_.size() > 50000)
    {
        ++discard_send_pkg_count_;
        memory.FreeMemory(p_send_buf);
        return;
    }
    MsgHeader* p_msg_header = (MsgHeader*)p_send_buf;
    Connection* p_conn = p_msg_header->conn;
    if(p_conn->send_count > 400)
    {
        // 用户收消息太慢，或者根本不收消息，发送队列中数据条目过大，
        // 为恶意用户，直接踢出
        LOG_INFO << "用户" << p_conn->client_addr.ToIPPort() << "积压了大量数据包";
        ++discard_send_pkg_count_;
        memory.FreeMemory(p_send_buf);
        zd_close_socket_proc(p_conn);
        return;
    }
    LOG_INFO << "发送的数量还没有超过400";
    ++p_conn->send_count;
    LOG_INFO << "此时send_count:" << p_conn->send_count;
    unique_lock<mutex> send_queue_lock{send_message_queue_mutex_};
    LOG_INFO << "将内存块添加到了send_message_queue中了";
    send_message_queue_.push_back(p_send_buf);
    send_queue_lock.unlock();
    send_message_queue_cond_.notify_one();
}

void Socket::zd_close_socket_proc(Connection* p_conn)
{
    // 如果要之前加到了定时器中
    if(ifkickTimeCount)
    {
        DeleteFromTimerQueue(p_conn->timer_id_);
    }
    if(p_conn->fd != -1)
    {
        LOG_INFO << "fd:" << p_conn->fd << "超时了, 要进行关闭了";
        close(p_conn->fd);
        LOG_INFO << "关闭成功";
        p_conn->fd = -1;
    }
    if(p_conn->throw_send_count > 0)
    {
        --p_conn->throw_send_count;
    }
    InRecyConnectQueue(p_conn);
}

bool Socket::TestFlood(Connection *p_conn)
{
    Timestamp cur_time = Timestamp::now();
    if((cur_time-p_conn->flood_kick_last_time) < flood_time_interval_)
    {
        // 发包太频繁了
        LOG_INFO << "包间隔时间小于100ms";
        p_conn->flood_attack_count++;
        p_conn->flood_kick_last_time = cur_time;
    }
    else
    {
        p_conn->flood_attack_count = 0;
        p_conn->flood_kick_last_time = cur_time;
    }
    if(p_conn->flood_attack_count >= flood_kick_count_)
    {
        return true;
    }
    return false;
}

void Socket::SendQueueThread()
{
    int err{0};
    list<char*>::iterator pos, posend;

    char *p_msg_buf{nullptr};
    MsgHeader   *p_msg_header{nullptr};
    PkgHeader   *p_pkg_header{nullptr};
    Connection  *p_conn{nullptr};

    uint16_t itmp{0};
    ssize_t send_size{0};
    Memory& memory = Memory::GetInstance();

    // 进程不退出
    while(running_)
    {
        unique_lock<mutex> message_queue_lock{send_message_queue_mutex_};
        send_message_queue_cond_.wait(message_queue_lock,[&]{
            return !send_message_queue_.empty() || !running_;
        });
        if(!running_)
        {
            break;
        }
        LOG_INFO << "send_message_queue.Size()" << send_message_queue_.size();
        pos = send_message_queue_.begin();
        posend = send_message_queue_.end();
        while(pos != posend)
        {
            p_msg_buf = *pos;
            LOG_INFO << "要发送消息的内存块:" << (void*)p_msg_buf;
            p_msg_header = (MsgHeader*)p_msg_buf;
            p_pkg_header = (PkgHeader*)(p_msg_buf+msg_header_len_);
            p_conn = p_msg_header->conn;
            // 包过期
            if(p_conn->sequence_num != p_msg_header->cur_sequence_num)
            {
                pos = send_message_queue_.erase(pos);
                memory.FreeMemory(p_msg_buf);
                continue;
            }
            LOG_INFO << "包还没有过期呢";
            if(p_conn->throw_send_count > 0)
            {
                // 靠系统驱动来发送消息，这里不能再发送
                ++pos;
                continue;
            }
            // 发送队列数减一
            --p_conn->send_count;

            // 这里可以开始发送消息
            // 发送后释放用的，因为这段内存是new出来的
            p_conn->send_mem_pointer = p_msg_buf;
            pos = send_message_queue_.erase(pos);
            // 要发送的数据的缓冲区指针
            p_conn->send_buf = (char*)p_pkg_header;
            // 包头+包体长度，
            itmp = ntohs(p_pkg_header->pkg_len);
            // 要发送多少数据
            p_conn->send_len = itmp;

            send_size = SendProc(p_conn, p_conn->send_buf, p_conn->send_len);
            LOG_INFO << "发出去的数据长度:" << send_size;
            if(send_size > 0)
            {
                // 数据全都发送出去了
                if(send_size == p_conn->send_len)
                {
                    LOG_INFO << "数据全都发完了";
                    memory.FreeMemory(p_conn->send_mem_pointer);
                    p_conn->send_mem_pointer = nullptr;
                    p_conn->throw_send_count = 0;
                }
                else
                {
                    LOG_INFO << "数据只发送了一部分";
                    // 只发送了部分数据
                    // 更新发送缓冲区
                    p_conn->send_buf += send_size;
                    p_conn->send_len -= send_size;
                    // 标记发送缓冲区满了
                    ++p_conn->throw_send_count;
                    if(Epoll_Oper_Event(
                        p_conn->fd,
                        EPOLL_CTL_MOD,
                        EPOLLOUT,
                        0,                  // 0增加，1，去掉，2，完全覆盖
                        p_conn
                    ) == -1)
                    {
                        LOG_ERROR << "Socket::SendQueueThread()->Epoll_Oper_Event() failed";
                    }
                }
                continue;
            }
            else if(send_size == 0)
            {
                LOG_INFO << "对端关闭了";
                // 对端关闭了，
                // 释放内存
                memory.FreeMemory(p_conn->send_mem_pointer);
                p_conn->send_mem_pointer = nullptr;
                p_conn->throw_send_count = 0;
                continue;
            }
            else if(send_size == -1)
            {
                LOG_INFO << "内核缓冲区满了";
                ++p_conn->throw_send_count;
                if(Epoll_Oper_Event(
                        p_conn->fd,
                        EPOLL_CTL_MOD,
                        EPOLLOUT,
                        0,
                        p_conn
                    ) == -1)
                {
                    LOG_ERROR << "Socket::SendQueueThread()->Epoll_Oper_Event() failed";
                }
                continue;
            }
            else
            {
                LOG_INFO << "各种尝试都作了，还是不成功，则直接释放内存";
                memory.FreeMemory(p_conn->send_mem_pointer);
                p_conn->send_mem_pointer = nullptr;
                p_conn->throw_send_count = 0;
                continue;
            }
        }
    }
}

void Socket::Start()
{
    LOG_INFO << "Socket::Start()开始执行";
    running_ = true;
    send_message_queue_thread_ = thread(&Socket::SendQueueThread, this);
    recycle_connection_thread_ = thread(&Socket::RecycleConnectionThread, this);
    // if(ifkickTimeCount)
    // {
    //     timer_queue_monitor_thread_ = thread(&Socket::TimerQueueMonitorThread, this);
    // }
    LOG_INFO << "两个后台线程创建完成";
    
}

void Socket::Shutdown()
{
    running_ = false;
    if(send_message_queue_thread_.joinable())
    {
        send_message_queue_thread_.join();
    }
    if(recycle_connection_thread_.joinable())
    {
        recycle_connection_thread_.join();
    }
    if(timer_queue_monitor_thread_.joinable())
    {
        timer_queue_monitor_thread_.join();
    }
}