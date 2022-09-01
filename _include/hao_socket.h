#ifndef _HAO_SOCKET_H_
#define _HAO_SOCKET_H_

#include "hao_common.h"
#include "hao_timestamp.h"
#include "hao_internet_address.h"
#include "hao_timer.h"

#include <semaphore.h>

#include <sys/epoll.h>
#include <sys/socket.h>

#include <vector>
#include <queue>
#include <list>
#include <deque>
#include <map>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <tuple>

using std::unordered_map;
using std::atomic;
using std::map;
using std::list;
using std::queue;
using std::vector;
using std::multimap;
using std::unique_ptr;
using std::make_unique;
using std::mutex;
using std::condition_variable;
using std::thread;
using std::deque;
using std::tuple;
using std::chrono::seconds;

// epoll 中一次最多接受事件的个数
constexpr int MAX_EVENTS{512};

class Socket;
class Connection;
class Listening;

struct Listening
{
    int sockfd;
    InternetAddress listen_address;
    // 监听套接字也是需要连接池中的连接的，对该连接绑定EPOLL_CTL_ADD
    Connection *connection_ptr;
    Listening(int fd, const InternetAddress& address)
        :sockfd{fd}, listen_address{address}, connection_ptr{nullptr}
    {

    }
    ~Listening()
    {

    } 
};

using event_handler_ptr = void(Socket::*)(Connection*);

// 一个Connection表示一个Tcp连接
struct Connection
{
    private:
        static int32_t connection_count;
        const int32_t connection_id_;
    public:
        Connection();                      
        ~Connection(); 
        // 从连接池中取一个连接，并初始化             
        void GetOneToUse(); 
        // 归还一个连接到连接池中                
        void PutOneToFree();                 
        const int32_t Id() const;
        // 套接字fd
        int fd;    
        // 定时器id
        int32_t timer_id_;                         
        // 如果连接被分配给一个监听套接字，则用该指针指向该监听套接字
        Listening *listening_ptr;
        
        // nginx的失效标志位，以后可以考虑再次使用
        //unsigned instance:1;

        uint64_t sequence_num;
        InternetAddress client_addr;

        event_handler_ptr read_handler;
        event_handler_ptr write_handler;

        // epoll返回的事件
        uint32_t events;

        // 收包相关的变量
        // 当前收包状态
        PkgState cur_stat;
        // 保存收到的包头信息
        char head_info[DATA_BUFSIZE];

        // 接收数据的缓冲区的头指针，
        char        *precv_buf;
        // 要收多少数据
        uint32_t    recv_len;
        // new出来的用于收包的内存首地址，释放用的
        char        * precv_mem_pointer;

        // 业务逻辑处理的互斥量
        mutex     logic_proc_mutex;

        // 发包有关
        // 发送消息，如果发送缓冲区满了，则通过epoll来驱动消息继续发送，
        atomic<int>         throw_send_count;
        // 发送完成后释放用的，整个数据的头指针，= 消息头+包头+包体
        char                *send_mem_pointer;
        // 发送数据的缓冲区的头指针 = 包头+包体
        char                *send_buf;
        // 要发送多少数据
        uint32_t            send_len;

        // 回收有关
        // 到资源回收站里去的时间
        Timestamp           recycle_time;

        // 心跳包
        // 上次ping的时间（上次发送心跳包的时间）
        Timestamp           last_ping_time;

        // 网络安全有关
        // flood攻击上次收到的包的时间
        Timestamp           flood_kick_last_time;
        // flood攻击在该时间内收到包的次数统计
        int32_t             flood_attack_count;

        // 发送队列中有的数据条目数，若client只发不收，则可能造成此数过大，依据此数做踢出处理
        atomic<int>         send_count;
};

class Socket
{
    public:
        using MessageCallback = function<void(char*)>;
        using PingOutCallback = function<void(MsgHeader*, Timestamp)>;
        void set_message_callback(MessageCallback message_callback);
        void set_ping_out_callback(PingOutCallback ping_out_callback);
    private:
        MessageCallback  message_callback_;
        PingOutCallback ping_out_callback_;
    
    public:
        Socket();
        Socket(MessageCallback message_callback, PingOutCallback ping_out_callback);
        ~Socket();
        // 初始化函数，父进程中执行
        bool Initialize();

        // 初始化函数，子进程中执行
        void Start();

        // 关闭退出函数，子进程中执行
        void Shutdown();

        // 打印统计信息
        void PrintInfo();

        // 心跳包检测事件到，该去检测心跳包是否超时的事宜
        // 只是把内存释放，自雷应该重新实现该函数以实现具体的判断动作
        //void proc_ping_time_out_checking(MsgHeader* mgs, Timestamp cur_time);

        // 初始化epoll
        int Epoll_init();
        void Epoll_Process_Events();
        // 数据扔到发送队列中
        void MsgSend(char *send_buf);
        // 更新连接时间
        void UpdateTimer(Connection* conn, Timestamp when);
    private:
        int Epoll_Oper_Event(int fd, uint32_t event_type, uint32_t flag, int bcaction, Connection * conn);
        
        // 主动关闭一个连接
        void zd_close_socket_proc(Connection* conn);
    private:
        void ReadConfig();
        // 支持多端口监听
        bool OpenListeningSockets();
        // 关闭所有监听套接字
        void CloseListeningSockets();
        // 设置非阻塞套接字
        bool SetNonBlocking(int sock_fd);

        // 业务处理函数
        // 建立新连接
        void EventAccept(Connection *old);
        // 设置数据来时的读处理函数
        void ReadRequestHandler(Connection* conn);
        // 设置数据来时的写处理函数
        void WriteRequestHandler(Connection *conn);
        // 关闭连接函数
        void CloseConnection(Connection*);

        // 接收从客户端来的数据专用函数
        ssize_t RecvProc(Connection* conn, char *buf, ssize_t buf_len);
        
        // 包头收完整后的处理，称为包处理阶段1
        void WaitRequestHandlerProcP1(Connection* conn, bool& is_flood);

        // 收到一个完整包后的处理
        void WaitRequestHandlerProcPlast(Connection *conn, bool& is_flood);

        // 清空发送队列
        void ClearMsgSendQueue();

        // 将数据发送到客户端
        ssize_t SendProc(Connection *conn, char *buf, ssize_t size);

        // 获取对端信息，获取端口字符串，返回字符串的长度
        size_t SockNtop(struct sockaddr *sa, int port, u_char *text, size_t len);

        // 连接池或连接相关
        // 初始化连接池
        void InitConnectionPool();

        // 回收连接池
        void ClearConnection();

        // 从连接池中获取一个空闲连接
        Connection* GetConnection(int sock);

        // 归还参数conn代表的连接到连接池中
        void FreeConnection(Connection*);

        // 将回收的连接放到一个队列中来
        void InRecyConnectQueue(Connection*);

        // 和时间相关的函数
        void AddToTimerQueue(Connection *conn);
        // 从multimap中取得最早的时间返回去
        Timestamp GetEarliestTime();
        // 从time_queue_map移除最早的时间，并把最早这个时间所在的项的值所对应的指针返回
        // 调用者负责互斥，所以本函数不用互斥
        MsgHeader* RemoveFirstTimer();

        // 根据所给的当前时间，从time_queue_map中找到比这个事件更早的一个节点返回去，
        // 这些节点都是时间超过了，要处理的节点
        MsgHeader* GetOverTimeTimer(Timestamp cur_time);
        // 把指定用户tcp连接从timer表中移出
        void DeleteFromTimerQueue(int32_t timer_id);
        // 清理事件队列中的所有内容
        void ClearAllFromTimerQueue();
        

        // 和网络安全相关
        // 测试是否flood攻击成立，成立则返回
        bool TestFlood(Connection *conn);

        // 线程相关函数
        // 专门用来发送数据的线程
        void SendQueueThread();

        // 专门用来回收连接的线程
        void RecycleConnectionThread();

        // 时间队列监视线程，处理到期不发心跳包的用户踢出的线程
        //void TimerQueueMonitorThread();

        // 返回值是下一次epoll_wait timeout的值
        int TimerHeartBeatCheck();

    protected:
        // 网络通讯有关的成员变量
        size_t              pkg_header_len_;     // sizeof(Pkg_Header) 
        size_t              msg_header_len_;     // sizeof(Msg_Header)

        // 时间相关
        // 当时间到达 sock_max_wait_time  指定的时间时，把客户端踢出去，
        // 只有ifkickTimeCount = true时，本项才有用
        bool                 ifTimeOutKick;     
        // 多少秒检测一次是否 心跳超时，
        // 只有 ifkickTimeCount = true 时，本项才有用
        seconds                 wait_time_;      

    private:
        // epoll 连接的最大项数
        int worker_connections_;
        // 监听的端口数量
        int listen_port_count_;
        // epoll_create返回的句柄
        int epoll_handle_;

        // Epoll进程是否运行
        atomic<bool> running_;

        // 连接池
        queue<Connection*>   connection_pool_;
        // 空闲连接列表
        queue<Connection*>   free_connection_pool_;
        // 连接池总链接数
        atomic<int>         total_connection_n_;
        // 空闲连接总数
        atomic<int>         free_connection_n_;
        // 连接相关互斥量，互斥free_connection_list_,connection_list_
        mutex     connection_pool_mutex_;

        //监听套接字队列
        vector<Listening>   listen_socket_list_;
        // epoll_wait返回的活跃事件
        struct epoll_event  events_[MAX_EVENTS];

        
        // --------------------数据发送线程------------------------
        // 数据发送线程相关
        thread              send_message_queue_thread_;
        // 发送数据消息队列
        list<char*>         send_message_queue_;
        // 发送消息队列互斥量
        mutex               send_message_queue_mutex_;
        condition_variable  send_message_queue_cond_;
        // -------------------------------------------

        // -----------------连接回收线程------------------------
        thread recycle_connection_thread_;
        // 连接回收队列相关的互斥量
        mutex               recycle_connection_pool_mutex_;
        condition_variable  recycle_connection_pool_cond_;
        // 将要释放的连接放这里
        unordered_map<int, Connection*>   recycle_connection_pool_;
        // 待释放连接队列大小
        //int        total_recycle_connection_n_;
        // 等待多少秒后回收连接
        std::chrono::seconds        recycle_connection_wait_time_;
        // --------------------------------------------------------

        // -----------------------定时器线程----------------------------
        thread              timer_queue_monitor_thread_;  
        // 定时器队列互斥量
        mutex               timer_queue_mutex_;
        condition_variable  timer_queue_cond_;
        // 时间队列
        //multimap<Timestamp, MsgHeader*> timer_queue_map_;
        Timer               timer_;
        // ------------------------------------------------------------
        
        // 发送消息线程相关的信号量
        //sem_t               sem_event_send_queue_;

        // 时间相关
        // 是否开启踢人时钟，true 开启，false关闭
        bool                 ifkickTimeCount;
        // 和时间队列相关的互斥量
        

        // 在线用户相关
        // 当前在线用户数统计
        atomic<int>         online_user_count_;

        // 网络安全相关
        // flood攻击检测是否开启，1开启，0关闭
        bool                 flood_ak_enable_;
        // 表示每次收到数据包的时间间隔是100毫秒
        milliseconds        flood_time_interval_;
        // 累计多少次踢出此人
        int                 flood_kick_count_;

        // 统计用途
        // 上册打印统计信息的时间
        Timestamp           last_print_time_;
        // 丢弃的发送数据包数量
        int                 discard_send_pkg_count_;


};
#endif