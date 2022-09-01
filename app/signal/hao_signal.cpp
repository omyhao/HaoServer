#include "hao_server.h"
#include "hao_log.h"
#include "hao_global.h"
#include "hao_signal.h"

#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <vector>

using std::vector;

using namespace hao_signal;
using namespace hao_log;

struct Signal
{
    int             signo;
    const char      *signame;
    // 信号处理函数
    void (*handler)(int signo, siginfo_t *siginfo, void *ucontext);
};

static void signal_handler(int signo, siginfo_t *siginfo, void *ucontext);
static void process_get_status(void);

vector<Signal> signals{
        // signo      signame             handler
    { SIGHUP,    "SIGHUP",           signal_handler },        //终端断开信号，对于守护进程常用于reload重载配置文件通知--标识1
    { SIGINT,    "SIGINT",           signal_handler },        //标识2   
	{ SIGTERM,   "SIGTERM",          signal_handler },        //标识15
    { SIGCHLD,   "SIGCHLD",          signal_handler },        //子进程退出时，父进程会收到这个信号--标识17
    { SIGQUIT,   "SIGQUIT",          signal_handler },        //标识3
    { SIGIO,     "SIGIO",            signal_handler },        //指示一个异步I/O事件【通用异步I/O信号】
    { SIGSYS,    "SIGSYS, SIG_IGN",  nullptr             },        //我们想忽略这个信号，SIGSYS表示收到了一个无效系统调用，如果我们不忽略，进程会被操作系统杀死，--标识31
                                                                   //所以我们把handler设置为NULL，代表 我要求忽略这个信号，请求操作系统不要执行缺省的该信号处理动作（杀掉我）
    //...日后根据需要再继续增加
    { 0,         nullptr,            nullptr             }         //信号对应的数字至少是1，所以可以用0作为一个特殊标记
};

// 信号初始化
bool hao_signal::init_signals()
{
    struct sigaction sa;
    for(auto& sig : signals)
    {
        if(sig.signo != 0)
        {
            memset(&sa, 0, sizeof(struct sigaction));
            sigemptyset(&sa.sa_mask);
            if(sig.handler)
            {
                sa.sa_sigaction = sig.handler;
                sa.sa_flags = SA_SIGINFO;
            }
            else
            {
                sa.sa_handler = SIG_IGN;
            }
            if(sigaction(sig.signo, &sa, NULL) == -1)
            {
                LOG_EMERG << "sigaction(" << sig.signame << ") failed";
                return false;
            }
        }
        
    }
    return true;
}

static void signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{
    auto cur_signal = signals.begin();
    for(; cur_signal->signo != 0; cur_signal++)
    {
        if(cur_signal->signo == signo)
        {
            break;
        }
    }
    if(process_type == ProcessType::Master)
    {
        // 主进程
        switch (signo)
        {
        case SIGCHLD:
            // 子进程状态变化了。
            worker_status_changed = 1;
            break;
        
        default:
            break;
        }
    }
    else if(process_type == ProcessType::Worker)
    {
        // 子进程信号处理
    }
    else
    {
        // 一般进程
    }
    if(siginfo && siginfo->si_pid)  // si_pid == 发送信号的进程id
    {
        LOG_NOTICE << "signal " << signo  << " (" << cur_signal->signame 
                << ") received from " << siginfo->si_pid;
    }
    else
    {
        LOG_NOTICE << "signal " << signo << " (" << cur_signal->signame << ") received";
    }
    // 子进程状态有变化，通常是意外退出
    if(signo == SIGCHLD)
    {
        process_get_status();
    }
}

static void process_get_status(void)
{
    pid_t   pid;
    int     status;
    int     err;
    int     one{0};     // 标记信号正常处理过一次
    for( ; ; )
    {
        // -1      : 表示等待任何子进程
        // status  : 保存子进程的状态信息
        // WNOHANG : 表示不要阻塞，让waitpid()立即返回  
        pid = waitpid(-1, &status, WNOHANG);
        if(pid == 0)
        {
            // pid 为 0 表示子进程没结束，会立即返回0
            break;
        }
        if(pid == -1)
        {
            // 返回-1 表示waitpid调用出错了
            err = errno;
            if(err = EINTR) // 被某个信号中断了
            {
                continue;
            }
            if(err == ECHILD && one)    // 表示没有子进程
            {
                return;
            }
            if(err == ECHILD)
            {
                LOG_INFO << "waitpid() failed";
                return;
            }
            LOG_ALERT << "waitpid() failed";
            return;
        }
        one = 1;        // 标记waitpid()正常返回
        if(WTERMSIG(status))
        {
            LOG_ALERT << "pid = " << pid << " exited on signal " << WTERMSIG(status) << "!";
        }
        else
        {
            LOG_NOTICE << "pid = " << pid << " exited with code " << WEXITSTATUS(status) << "!";
        }
    }
}