#include "hao_socket.h"
#include "hao_memory.h"
#include "hao_global.h"
#include "hao_log.h"

#include <mutex>
#include <list>

using std::mutex;
using std::lock_guard;
using std::unique_lock;
using std::scoped_lock;
using std::list;

using namespace hao_log;

void Socket::AddToTimerQueue(Connection *p_conn)
{
    Memory& memory = Memory::GetInstance();
    Timestamp futtime = Timestamp::now();
    LOG_INFO << "当前时间:" << futtime.Microseconds() << ' ' << futtime.ToFormattedString(true);
    //LOG_INFO << "心跳时间:" << wait_time_;
    futtime += wait_time_;
    LOG_INFO << "到期时间:" << futtime.Microseconds() << ' ' << futtime.ToFormattedString(true);
    MsgHeader* tmp_msg_header = (MsgHeader*)memory.AllocMemory(msg_header_len_, false);
    tmp_msg_header->conn = p_conn;
    tmp_msg_header->cur_sequence_num = p_conn->sequence_num;
    LOG_INFO << "准备进锁了:" << p_conn->sequence_num;
    {
        scoped_lock timer_lock{timer_queue_mutex_};
        uint32_t timer_id = timer_.TimerAdd(futtime, tmp_msg_header);
        p_conn->timer_id_ = timer_id;
        LOG_INFO << "fd:" << tmp_msg_header->conn->fd << " 添加到了定时器里了,定时器size():" << timer_.Size();
    }
    LOG_INFO << "最早到期时间:" << GetEarliestTime().ToFormattedString(true);
}

// 定时器中最早的时间，调用者负责互斥，且不为空
Timestamp Socket::GetEarliestTime()
{
    return timer_.EarliestTime();
}

// 定时器中移除最早的时间，调用者互斥
MsgHeader* Socket::RemoveFirstTimer()
{
    if(timer_.Empty())
    {
        return nullptr;
    }
    // 这里写日志的时候进行了pop，再return pop的话，就会返回nullptr
    //LOG_INFO << "原始数据:fd" << ((MsgHeader*)timer_.Pop())->conn->fd;
    return (MsgHeader*)timer_.Pop();
}

// 根据给定时间，从定时器中找到比这个时间更早的时间
MsgHeader* Socket::GetOverTimeTimer(Timestamp cur_time)
{
    if(timer_.Empty())
    {
        return nullptr;
    }
    LOG_INFO << "timer_不为空";
    LOG_INFO << "当前时间:" << cur_time.ToFormattedString(true);
    Timestamp earliest_time = GetEarliestTime();
    LOG_INFO << "最早时间:" << earliest_time.ToFormattedString(true);
    if(earliest_time <= cur_time)
    {
        // 有超时节点了
        LOG_INFO << "有超时节点";
        MsgHeader* temp = RemoveFirstTimer();
        temp->conn->timer_id_ = -1;
        // if(ifTimeOutKick != true)
        // {
        //     // 如果并没有要求超时就踢出
        //     // FIXME: 这里会出现的问题：
               // FIXED: 其实这里也不会出现下面的问题
        //     // 不同的定时器可能会保存到同一个conn的指针，如果conn删除，那么定时器要怎么做
        //     Timestamp new_time = cur_time + wait_time_;
        //     Memory& memory = Memory::GetInstance();
        //     MsgHeader* temp_msg_header = (MsgHeader*)memory.AllocMemory(sizeof(MsgHeader), false);
        //     temp_msg_header->conn = temp->conn;
        //     temp_msg_header->cur_sequence_num = temp->cur_sequence_num;
        //     timer_.TimerAdd(new_time, temp_msg_header);
        // }
        return temp;
    }
    return nullptr;
}

// 把给定的tcp链接从定时器中删除
void Socket::DeleteFromTimerQueue(int32_t timer_id)
{
    if(timer_id == -1)
    {
        LOG_INFO << "该定时器已经被删除了";
    }
    else
    {
        LOG_INFO << "要删除的定时器id:" << timer_id;
        timer_.TimerCancel(timer_id);
    }
    
}

// 清空定时器
void Socket::ClearAllFromTimerQueue()
{
    // FIXME
    timer_.Clear();
}

// 更新定时器
void Socket::UpdateTimer(Connection* conn, Timestamp when)
{
    timer_.Update(conn->timer_id_, when+wait_time_);
}

int Socket::TimerHeartBeatCheck()
{
    Timestamp earliest_time{timer_.EarliestTime()};
    Timestamp cur_time = Timestamp::now();
    // 当前时间
    if(earliest_time < cur_time)
    {
        LOG_INFO << "要开始处理超时事件了";
        // 时间到了，可以处理了
        // 保存要处理的内容
        list<MsgHeader*> is_idle_list;
        MsgHeader* result{nullptr};
        // 一次性把所有超时事件取出来
        while((result = GetOverTimeTimer(cur_time)) != nullptr)
        {
            LOG_INFO << "取出来的fd:" << result->conn->fd;
            is_idle_list.push_back(result);
        }
        MsgHeader* temp_msg = nullptr;
        Memory& mem_instance = Memory::GetInstance();
        while(!is_idle_list.empty())
        {
            temp_msg = is_idle_list.front();
            is_idle_list.pop_front();
            // FIXME
            // 这里应该通过子类进行function进行绑定
            //proc_ping_time_out_checking(temp_msg, cur_time);
            LOG_INFO << "要主动关闭链接了fd:" << temp_msg->conn->fd;
            zd_close_socket_proc(temp_msg->conn);
            LOG_INFO << "要释放内存了";
            mem_instance.FreeMemory(temp_msg);
        }  
        LOG_INFO << "定时事件处理完了，开始下一波";
        return (timer_.EarliestTime() - Timestamp::now()).Milliseconds();   
    }
    else
    {
        LOG_INFO << "当前还没有到时事件";
        return (earliest_time - cur_time).Milliseconds();
    }
    
}