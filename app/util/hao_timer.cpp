#include "hao_timer.h"
#include "hao_log.h"

#include <stdlib.h>
#include <sys/time.h>

using namespace hao_log;

struct event
{
    int32_t    min_heap_idx;
    Timestamp  ev_timeout;
    void*       user_data_;
};

static inline void	     min_heap_ctor_(min_heap_t* s);
static inline void	     min_heap_dtor_(min_heap_t* s);
static inline void	     min_heap_elem_init_(struct event* e);
static inline int	     min_heap_elt_is_top_(const struct event *e);
static inline bool	     min_heap_empty_(min_heap_t* s);
static inline uint32_t	     min_heap_size_(min_heap_t* s);
static inline struct event*  min_heap_top_(min_heap_t* s);
static inline int	     min_heap_reserve_(min_heap_t* s, uint32_t n);
static inline int	     min_heap_push_(min_heap_t* s, struct event* e);
static inline struct event*  min_heap_pop_(min_heap_t* s);
static inline int	     min_heap_adjust_(min_heap_t *s, struct event* e);
static inline int	     min_heap_erase_(min_heap_t* s, struct event* e);
static inline void	     min_heap_shift_up_(min_heap_t* s, uint32_t hole_index, struct event* e);
static inline void	     min_heap_shift_up_unconditional_(min_heap_t* s, uint32_t hole_index, struct event* e);
static inline void	     min_heap_shift_down_(min_heap_t* s, uint32_t hole_index, struct event* e);


// #define min_heap_elem_greater(a, b) \
// 	(evutil_timercmp(&(a)->ev_timeout, &(b)->ev_timeout, >))
#define min_heap_elem_greater(a, b) \
	((a)->ev_timeout > (b)->ev_timeout)

void min_heap_ctor_(min_heap_t* s) 
{
    s->p = nullptr; 
    s->n = 0; 
    s->a = 0; 
}

void min_heap_dtor_(min_heap_t* s) 
{ 
    if (s->p)
    {
        s->n = 0;
        s->a = 0;
        free(s->p);
    }      
}

void min_heap_elem_init_(struct event* e) 
{ 
    e->min_heap_idx = -1; 
}

bool min_heap_empty_(const min_heap_t* s) 
{ 
    return 0u == s->n; 
}

uint32_t min_heap_size_(const min_heap_t* s)
{ 
    return s->n; 
}

// 返回堆顶元素
struct event* min_heap_top_(min_heap_t* s) 
{ 
    return s->n ? *s->p : nullptr; 
}

// 堆中插入一个新节点
int min_heap_push_(min_heap_t* s, struct event* e)
{
    // 为新节点分配位置，检查是否已经是最大容量或者内存分配失败
	if (s->n == UINT32_MAX || min_heap_reserve_(s, s->n + 1))
		return -1;
    // 从最后一个节点开始shift_up 
	min_heap_shift_up_(s, s->n++, e);
	return 0;
}

// 删除堆顶元素
struct event* min_heap_pop_(min_heap_t* s)
{
	if (s->n)
	{
        // 找到堆顶
		struct event* e = *s->p;
        // 互换堆顶和最后一个节点，然后shift_down
		min_heap_shift_down_(s, 0u, s->p[--s->n]);
        // 元素堆索引置-1
		e->min_heap_idx = -1;
		return e;
	}
	return nullptr;
}
// 判断一个节点是否是堆顶
int min_heap_elt_is_top_(const struct event *e)
{
	return e->min_heap_idx == 0;
}

int min_heap_erase_(min_heap_t* s, struct event* e)
{
    LOG_INFO << "到了min_heap_erase_中了";
	if (-1 != e->min_heap_idx)
	{
        LOG_INFO << "id不为-1";
        // 获取堆中的最后一个元素
        LOG_INFO << "s->n:" << s->n;
		struct event *last = s->p[--s->n];
        LOG_INFO << "这里？";
        // 找到要删除节点的父节点
		uint32_t parent = (e->min_heap_idx - 1) / 2;
        LOG_INFO << "parent:" << parent;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
        // 要删除的节点不是堆顶且最后一个节点的超时值小于父节点的超时值
		if (e->min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
        {
            // 把最后一个节点与要删除的节点互换为止
            LOG_INFO << "开始shift_up";
			min_heap_shift_up_unconditional_(s, e->min_heap_idx, last);
            LOG_INFO << "shift_up结束";
        }
            
		else
        {
            // 如果该节点本身就是堆顶，或者最后一个节点的超时值不小于父节点的超时值
            // 将最后一个结点的超时值换到要删除的结点位置，然后下沉
            LOG_INFO << "开始shift_down";
			min_heap_shift_down_(s, e->min_heap_idx, last);
            LOG_INFO << "shift_down结束";
        }

            
        // 将删除节点的堆索引值设置为-1
		e->min_heap_idx = -1;
		return 0;
	}
	return -1;
}

// 调整内存
int min_heap_reserve_(min_heap_t* s, uint32_t n)
{
    // 如果堆大小<堆元素个数
	if (s->a < n)
	{
		struct event** p;
        // 如果不是第一次调整则扩大两倍，否则设置为8
		uint32_t a = s->a ? s->a * 2 : 8;
        // 如果扩一次之后还不够，则直接设置为目标值
		if (a < n)
			a = n;
        // 重新调整内存
		if (!(p = (struct event**)realloc(s->p, a * sizeof *p)))
			return -1;
        // 设置首地址
		s->p = p;
        // 设置堆容量
		s->a = a;
	}
	return 0;
}

void min_heap_shift_up_unconditional_(min_heap_t* s, uint32_t hole_index, struct event* e)
{
    uint32_t parent = (hole_index - 1) / 2;
    do
    {
        (s->p[hole_index] = s->p[parent])->min_heap_idx = hole_index;
        hole_index = parent;
        parent = (hole_index - 1) / 2;
    } while (hole_index && min_heap_elem_greater(s->p[parent], e));
    (s->p[hole_index] = e)->min_heap_idx = hole_index;
}

// 上浮调整
void min_heap_shift_up_(min_heap_t* s, uint32_t hole_index, struct event* e)
{
    // 找父节点
    uint32_t parent = (hole_index - 1) / 2;
    // 如果如果不是堆顶，且父节点大于该节点
    while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
        // 父节点大，则将父节点放在当前的hole_index的位置
        (s->p[hole_index] = s->p[parent])->min_heap_idx = hole_index;
        // 设置hole_index为父节点索引
        hole_index = parent;
        // 重新设置父节点
        parent = (hole_index - 1) / 2;
    }
    // 找到了插入的位置
    (s->p[hole_index] = e)->min_heap_idx = hole_index;
}

void min_heap_shift_down_(min_heap_t* s, uint32_t hole_index, struct event* e)
{
    uint32_t min_child = 2 * (hole_index + 1);
    while (min_child <= s->n)
	{
        min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
        if (!(min_heap_elem_greater(e, s->p[min_child])))
            break;
        (s->p[hole_index] = s->p[min_child])->min_heap_idx = hole_index;
        hole_index = min_child;
        min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->min_heap_idx = hole_index;
}

int min_heap_adjust_(min_heap_t *s, struct event *e)
{
	if (-1 == e->min_heap_idx) {
		return min_heap_push_(s, e);
	} else {
		uint32_t parent = (e->min_heap_idx - 1) / 2;
		/* The position of e has changed; we shift it up or down
		 * as needed.  We can't need to do both. */
		if (e->min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], e))
			min_heap_shift_up_unconditional_(s, e->min_heap_idx, e);
		else
			min_heap_shift_down_(s, e->min_heap_idx, e);
		return 0;
	}
}

Timer::Timer()
{
    min_heap_ctor_(&min_heap_);
}

Timer::~Timer()
{
    for(uint32_t i = 0; i < min_heap_.n; ++i)
    {
        free(min_heap_.p[i]);
    }
    min_heap_dtor_(&min_heap_);
}

bool Timer::Empty() const
{
    return min_heap_empty_(&min_heap_);
}

uint32_t Timer::Size() const
{
    return min_heap_size_(&min_heap_);
} 

Timestamp Timer::EarliestTime() const
{
    return Empty()? Timestamp::InvalidTime : (**min_heap_.p).ev_timeout;
}

int32_t Timer::TimerAdd(Timestamp when, void* data)
{
    struct event *ev = (struct event*)malloc(sizeof(struct event));
    if(nullptr == ev)
    {
        return 0;
    }
    min_heap_elem_init_(ev);
    ev->ev_timeout = when;
    ev->user_data_ = data;
    min_heap_push_(&min_heap_, ev);
    ref_[ev->min_heap_idx] = ev;
    LOG_INFO <<"定时器id:" << ev->min_heap_idx << "地址:" << (void*)ev  <<"添加成功，到期时间:" << ev->ev_timeout.ToFormattedString(true);
    return ev->min_heap_idx;
}

void Timer::Update(int32_t timer_id, Timestamp when)
{
    auto iterator = ref_.find(timer_id);
    if(iterator != ref_.end())
    {
        ref_[timer_id]->ev_timeout = when;
        min_heap_adjust_(&min_heap_, ref_[timer_id]);
        LOG_INFO <<"定时器id:" << ref_[timer_id]->min_heap_idx <<"更新成功，到期时间:" << ref_[timer_id]->ev_timeout.ToFormattedString(true);
    }
}

bool Timer::TimerCancel(int32_t timer_id)
{
    // FIXME
    // FIXED
    // 这里可以使用hashmap来删除
    auto iterator = ref_.find(timer_id);
    if(iterator != ref_.end())
    {
        LOG_INFO << "hashmap中有这个定时器:" << (void*)ref_[timer_id];
        min_heap_erase_(&min_heap_, ref_[timer_id]);
        LOG_INFO << "应该是这里卡住了";
        free(ref_[timer_id]);
        ref_.erase(iterator);
        return true;
    }
    LOG_INFO << "hashmap中没有这个定时器";
    return false;
    // for(uint32_t i{0}; i < min_heap_.n; ++i)
    // {
    //     if(timer_id == min_heap_.p[i]->timer_id)
    //     {
    //         struct event *e = min_heap_.p[i];
    //         min_heap_erase_(&min_heap_, min_heap_.p[i]);
    //         free(e);
    //         return true;
    //     }
    // }
    // return false;
}

void* Timer::Pop()
{
    if(min_heap_.n)
    {
        // FIXME 再次思考这里会不会出问题
        // 或者整个定时器是否需要加锁
        // FIXED
        // 其实不会，因为只有epoll线程会操作定时器
        struct event* e = *min_heap_.p;
        min_heap_shift_down_(&min_heap_, 0u, min_heap_.p[--min_heap_.n]);
        e->min_heap_idx = -1;
        void* data = e->user_data_;
        e->user_data_ = nullptr;
        std::free(e);
        return data;
    }
    return nullptr;
}

void Timer::Clear()
{
    for(uint32_t i = 0; i < min_heap_.n; ++i)
    {
        free(min_heap_.p[i]);
    }
    min_heap_dtor_(&min_heap_);
}

// void Timer::Run() const
// {
//     struct event    *event;
//     struct timeval  now;

//     while((event = min_heap_top_(&min_heap_)) != nullptr)
//     {
//         gettimeofday(&now, nullptr);
//         if(evutil_timercmp(&now, &(event->ev_timeout), <))
//             break;
//         min_heap_pop_(&min_heap_);
//         event->callback_();
//         if(event->ev)
//     }
// }