#ifndef _HAO_TIMER_H_
#define _HAO_TIMER_H_

#include "hao_timestamp.h"

#include <cstdint>
#include <functional>
#include <atomic>
#include <unordered_map>

using std::function;
using std::atomic;
using std::unordered_map;

struct event;
typedef struct min_heap
{
    struct event    **p;        // 动态分配的数组
    uint32_t        n,a;        // n:数组中元素个数，a:数组大小
}min_heap_t;

class Timer
{
    public:
        Timer();
        ~Timer();
        int32_t    TimerAdd(Timestamp when, void* data);
        bool        TimerCancel(int32_t timer_id);
        void        Update(int32_t timer_id, Timestamp when);
        uint32_t    Size() const;
        bool        Empty() const;
        Timestamp   EarliestTime() const;
        void*       Pop();
        // FIXME
        // 这里应该把释放内存的工作给外面 
        void        Clear();
        

    private:
        min_heap_t                  min_heap_;
        //uint32_t                    timer_id_;
        unordered_map<int32_t, event*> ref_;
};


#endif