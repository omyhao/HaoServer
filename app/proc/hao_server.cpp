#include "hao_server.h"
#include "hao_log.h"
#include "hao_config.h"
#include "hao_internet_address.h"
#include "hao_signal.h"
#include "hao_threadpool.h"
#include "hao_logic.h"
#include "hao_global.h"
#include "hao_memory.h"

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include <vector>
#include <utility>
#include <iostream>
#include <string_view>

using std::cerr;
using std::endl;
using std::vector;
using std::string;
using std::pair;
using std::initializer_list;
using std::string_view;


using namespace hao_signal;
using namespace hao_log;
using namespace hao_server;

// 当前进程id
pid_t pid;

// 父进程id
pid_t ppid;

// 进程类型
ProcessType process_type;

// 子进程状态是否发生改变
sig_atomic_t worker_status_changed;

// 是否以守护进程运行
int daemonized{0};

// 进程退出标志量, 0:不退出，1:退出
int g_stop_event;

// 保存环境变量
size_t g_argv_space{0};
size_t g_env_space{0};
int g_argc{0};
vector<string> g_argv;
vector<char> g_environ;
// 系统进程环境变量地址
char **g_os_argv{nullptr};

// 保存所有服务器监听地址
//vector<ServerAddress> all_listen_address;

// 进程socket对象
Socket g_socket;
// 逻辑处理对象
LogicSocket g_logic_socket;
// 进程线程池对象
ThreadPool  g_threadpool;

namespace 
{
    void SetProcessName(string_view process_name);
    void StartWorkerProcess(int num_processes);
    void MakeChildProcess(int process_id, const char *proc_name);
    void WorkerProcessCycle(int process_id, const char* proc_name);
    void WorkerProcessInit(int process_id);
    void ProcessEventsAndTimer();
    void ExitLog();

    void SetProcessName(string_view process_name)
    {
        //LOG_INFO << "要设置进程名:" << process_name;
        size_t available_space = g_argv_space + g_env_space;
        size_t name_len = std::min(process_name.size(), available_space);
        g_os_argv[1] = NULL;
        strncpy(g_os_argv[0], process_name.data(), name_len);
        memset(g_os_argv[0]+name_len, 0, available_space - name_len);
    }

    void StartWorkerProcess(int num_processes)
    {
        for(int i = 0; i < num_processes; i++)
        {
            MakeChildProcess(i, "hao_server: worker process");
        }
        LOG_INFO <<  num_processes << "个进程创建成功";
    }
    void MakeChildProcess(int process_id, const char *proc_name)
    {
        pid_t cur_pid;
        cur_pid = fork();
        switch (cur_pid)
        {
            case -1:
                LOG_ALERT << "NostMakeChildProcess()中fork()子进程" 
                                    << process_id <<" proc_name:" << proc_name << " 失败";
                break;
            case 0:
                //这里是子进程
                ppid = pid;
                pid = getpid();
                //LOG_INFO << "ppid:" << ppid << " pid:" << pid;
                WorkerProcessCycle(process_id, proc_name);
                break;
            default:
                break;
        }
    }

    // worker子进程， 
    // process_num 从0开始编号
    void WorkerProcessCycle(int process_id, const char* proc_name)
    {
        if(!g_socket.Initialize())
        {
            LOG_ERROR << "Epoll初始化错误";
            exit(EXIT_FAILURE);
        }
        LOG_INFO << pid << "g_socket初始化成功";
        LOG_INFO << process_id << " " << proc_name << "开始设置进程名";
        // 设置子进程类型
        process_type = ProcessType::Worker;
        
        // 设置子进程进程名
        SetProcessName(proc_name);
        LOG_INFO << pid << " 设置进程名成功";
        LOG_INFO << "开始进行epoll初始化";
        WorkerProcessInit(process_id);
        
        LOG_NOTICE <<"PID:" << pid << ' ' << proc_name << " begin epoll";
        g_socket.Epoll_Process_Events();

    } 

    void WorkerProcessInit(int process_id)
    {
        //LOG_INFO << pid << "设置信号集";
        // 子进程信号集
        sigset_t sigset;
        // 清空信号集       
        sigemptyset(&sigset);
        // 释放原先屏蔽的信号，(fork时防止信号出现时屏蔽的)
        if(sigprocmask(SIG_SETMASK, &sigset, NULL) == -1)
        {
            LOG_ALERT << "NostWorkerProcessInit()中sigprocmask()失败";
        }
        //LOG_INFO << pid << "设置信号集结束";
        //LOG_INFO << pid << "开始执行epoll_socket.Init()";

        // 创建线程池
        Config& config_instance = Config::GetInstance();
        int thread_nums = config_instance["Process"]["ProcMsgRecvWorkerThreadCount"];
        LOG_INFO << "获得线程数参数:" << thread_nums << "开始创建线程池";
        
        g_threadpool.CreateThreads(thread_nums);
        LOG_INFO << pid << "线程池创建成功";
        LOG_INFO << pid << "开始进行g_socket初始化";
        // 初始化子进程
        //g_socket.Initialize();
        //LOG_INFO << pid << " g_socket初始化完成";
        g_socket.Start();
        LOG_INFO << pid << " 2个后台线程创建成功";
        g_socket.Epoll_init();

    }

    void ExitLog()
    {
        LOG_EXIT();
    }
    void ExitServer()
    {
        // 进程退出
        g_threadpool.WaitForTasks();

    }
}


void hao_server::ServerInit(int argc, char *argv[])
{
    // 初始化配置类单例
    std::cout << "开始加载Config" << std:: endl;
    Config& config = Config::GetInstance();
    if(!config.Load("./hao.hjson"))
    {
        cerr << "加载配置文件失败" << endl;
        std::exit(EXIT_FAILURE);
    }
    // 初始化内存类单例
    Memory::GetInstance();

    // 注册日志退出函数
    if(std::atexit(ExitLog) != 0)
    {
        cerr << "注册日志退出函数失败" << endl;
    }
    if(std::atexit(ExitServer) != 0)
    {
        cerr << "注册服务器退出函数失败" << endl;
    }
    LOG_INIT(static_cast<string_view>(config["Log"]["Path"]),(LogLevel)((int)config["Log"]["Level"]));
    // 设置进程全局变量
    g_stop_event = 0;
    process_type = ProcessType::Master;
    pid = getpid();            // 获得进程pid
    ppid = getppid();        // 获得父进程pid
    worker_status_changed = 0; // 子进程状态未改变

    // 保存原始的环境变量
    g_argc = argc;
    g_os_argv = argv;
    // 计算argv所占内存, +1 是字符串末尾为\0
    for(int i = 0; i < argc; i++)
    {
        g_argv.emplace_back(argv[i]);
        // 将参数另存
        g_argv_space += g_argv[i].size() + 1;
        
    }
    // 计算环境变量所占内存, +1 是字符串末尾为\0
    for(char **ep = environ; *ep!= nullptr; ep++)
    {
        g_env_space += strlen(*ep) + 1;
    }
    
    // reserve 和 resize 要同时使用
    g_environ.reserve(g_env_space);
    // 没有resize的话，数据存进去了，但是size还会是0
    g_environ.resize(g_env_space);
    char *temp = &g_environ[0];
    
    for(int i{0}; environ[i]; i++)
    {
        size_t size = strlen(environ[i])+1;
        strcpy(temp, environ[i]);
        environ[i] = temp;
        temp += size;
    }
    // 初始化信号配置函数
    if(!init_signals())
    {
        LOG_ERROR << "信号处理函数初始化出错";
        exit(EXIT_FAILURE);
    }
    // 初始化socket类
    // if(!g_socket.Initialize())
    // {
    //     LOG_ERROR << "Epoll初始化错误";
    //     exit(EXIT_FAILURE);
    // }
    // LOG_INFO << pid << "g_socket初始化成功";
    // 创建守护进程
    if(static_cast<bool>(config["Process"]["Daemon"]))
    {
        if(MakeDaemon() == -1)
        {
            LOG_INFO << "Daemonized failed";
            std::exit(EXIT_FAILURE);
        }
    }
    
    // 开始主循环
    MasterProcessCycle();
}

void hao_server::MasterProcessCycle()
{
    LOG_INFO << "开始主循环了";
    sigset_t sigset;
    sigemptyset(&sigset);

    // 下面的信号在执行主处理程序的时候，不要收到
    // 为了保护不希望由信号中断的代码临界区
    sigaddset(&sigset, SIGCHLD);       // 子进程状态改变
    sigaddset(&sigset, SIGALRM);       // 定时器超时
    sigaddset(&sigset, SIGIO);         // 异步IO
    sigaddset(&sigset, SIGINT);        // 终端终端符
    sigaddset(&sigset, SIGHUP);        // 连接断开
    sigaddset(&sigset, SIGUSR1);       // 用户自定义信号1
    sigaddset(&sigset, SIGUSR2);       // 用户自定义信号2
    sigaddset(&sigset, SIGWINCH);      // 终端窗口大小改变
    sigaddset(&sigset, SIGTERM);       // 终止
    sigaddset(&sigset, SIGQUIT);       // 终端退出符
    // 也可以接着添加其他需要屏蔽的信号

    // 信号阻塞期间，无法接收到信号集中的信号，发过来的多个单信号，会被合并为1个
    // 等放开之后，就会重新收到了。
    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
    {
        LOG_ALERT << "NostMasterProcessCycle()中sigprocmask()失败";
    }

    // 设置主进程标题
    string master_name{"hao_server: master process "};
    for(auto& single_argv : g_argv)
    {
        master_name.append(single_argv);
    }
    LOG_NOTICE << "PID:" << pid << ' ' << master_name << " begin.";
    SetProcessName(master_name);
    Config& config = Config::GetInstance();
    // 读取要创建worker进程的数量
    int num_work_process = static_cast<int>(config["Process"]["WorkerProcesses"]);
    LOG_INFO << "进程数:" << num_work_process;
    StartWorkerProcess(num_work_process);
    
    // 取消屏蔽信号
    sigemptyset(&sigset);
    for(;;)
    {
        sigsuspend(&sigset);
        sleep(10);
    }
}
