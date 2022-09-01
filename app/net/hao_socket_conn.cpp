#include "hao_socket.h"
#include "hao_memory.h"
#include "hao_log.h"
#include "hao_global.h"

#include <unistd.h>

using namespace hao_log;
using std::unique_lock;
using std::lock_guard;

int32_t Connection::connection_count {0};

Connection::Connection():
        sequence_num{0},
        connection_id_{connection_count++}
{

}
Connection::~Connection()
{

}

const int32_t Connection::Id() const
{
    return connection_id_;
}

// 分配出去一个连接的时候，初始化一些内容
void Connection::GetOneToUse()
{
    ++sequence_num;
    fd = -1;
    cur_stat = PkgState::Head_Init;
    precv_buf = head_info;
    recv_len = sizeof(PkgHeader);
    // 初始化的时候，还没有new内存
    precv_mem_pointer = nullptr;
    // 发包长度为0
    throw_send_count = 0;
    // 发送数据头指针记录
    send_mem_pointer = nullptr;
    events = 0;
    last_ping_time = Timestamp::now();

    // flood攻击上次收到包的时间
    flood_kick_last_time = 0;
    // flood攻击在改时间内收到包的次数
    flood_attack_count = 0;
    // 发送队列中共有的数据条目数，若client只发不收，则造成此数过大，可作出踢出处理
}

// 祸首一个连接
void Connection::PutOneToFree()
{
    ++sequence_num;
    if(precv_mem_pointer != nullptr)
    {
        Memory::GetInstance().FreeMemory(precv_mem_pointer);
        precv_mem_pointer = nullptr;
    }
    if(send_mem_pointer != nullptr)
    {
        Memory::GetInstance().FreeMemory(send_mem_pointer);
        send_mem_pointer = nullptr;
    }
    throw_send_count = 0;
}

// 初始化连接池
void Socket::InitConnectionPool()
{
    Connection* p_conn{nullptr};
    Memory& memory = Memory::GetInstance();

    int conn_size = sizeof(Connection);

    for(int i = 0; i < worker_connections_; ++i)
    {
        p_conn = (Connection*)memory.AllocMemory(conn_size, true);
        p_conn = new(p_conn)Connection();
        p_conn->GetOneToUse();
        connection_pool_.push(p_conn);
        free_connection_pool_.push(p_conn);
    }
    free_connection_n_ = total_connection_n_ = connection_pool_.size();
}

// 最终回收连接池
void Socket::ClearConnection()
{
    Connection* p_conn{nullptr};
    Memory& memory = Memory::GetInstance();
    while(!connection_pool_.empty())
    {
        p_conn = connection_pool_.front();
        connection_pool_.pop();
        p_conn->~Connection();
        memory.FreeMemory(p_conn);
    }
}

Connection* Socket::GetConnection(int sock_fd)
{
    lock_guard<mutex> lock(connection_pool_mutex_);
    if(!free_connection_pool_.empty())
    {
        Connection* p_conn = free_connection_pool_.front();
        free_connection_pool_.pop();
        p_conn->GetOneToUse();
        --free_connection_n_;
        p_conn->fd = sock_fd;
        LOG_INFO << "从连接池中取一个连接时recv_len:" << p_conn->recv_len;
        return p_conn;
    }
    // 如果没空闲连接，则重新新建一个
    Memory& memory = Memory::GetInstance();
    Connection* p_conn = (Connection*)memory.AllocMemory(sizeof(Connection),true);
    p_conn = new(p_conn)Connection();
    p_conn->GetOneToUse();
    connection_pool_.push(p_conn);
    ++total_connection_n_;
    p_conn->fd = sock_fd;
    return p_conn;
}

void Socket::FreeConnection(Connection* p_conn)
{
    lock_guard<mutex> lock(connection_pool_mutex_);
    p_conn->PutOneToFree();
    free_connection_pool_.push(p_conn);
    ++free_connection_n_;

}

void Socket::InRecyConnectQueue(Connection* p_conn)
{
    // 回收队列里没有的话
    unique_lock<mutex> recycle_lock{recycle_connection_pool_mutex_};
    if(!recycle_connection_pool_.count(p_conn->Id()))
    {
        p_conn->recycle_time = Timestamp::now();
        ++p_conn->sequence_num;
        recycle_connection_pool_[p_conn->Id()] = p_conn;
        --online_user_count_;
        recycle_lock.unlock();
        recycle_connection_pool_cond_.notify_one();
    }
}

void Socket::RecycleConnectionThread()
{
    Connection* p_conn{nullptr};
    while(running_)
    {
        unique_lock<mutex> recycle_lock{recycle_connection_pool_mutex_};
        recycle_connection_pool_cond_.wait(recycle_lock,[&]{
            return !recycle_connection_pool_.empty() || !running_;
        });
        Timestamp curr_time = Timestamp::now();
        for(auto it = recycle_connection_pool_.begin(); it != recycle_connection_pool_.end();)
        {
            p_conn = it->second;
            if(((p_conn->recycle_time + recycle_connection_wait_time_) > curr_time) && running_)
            {
                // 没到释放时间
                ++it;
                continue;
            }
            // 凡是到释放时间的throw_send_count都应该为0
            if(p_conn->throw_send_count > 0)
            {
                LOG_ERROR << "throw_send_count 不为0";
            }
            it=recycle_connection_pool_.erase(it);
            FreeConnection(p_conn); 
        }        
    }
    // 退出循环了，表示要结束程序了，但是此时也要把连接释放了
    lock_guard<mutex> recycle_mutex{recycle_connection_pool_mutex_};
    for(auto it = recycle_connection_pool_.begin(); it != recycle_connection_pool_.end();)
    {
        p_conn = it->second;
        it = recycle_connection_pool_.erase(it);
        FreeConnection(p_conn);
    }
    
}
void Socket::CloseConnection(Connection* conn)
{
    FreeConnection(conn);
    if(conn->fd != -1)
    {
        close(conn->fd);
        conn->fd = -1;
    }
}