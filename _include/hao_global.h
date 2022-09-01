#ifndef _HAO_GLOBAL_H_
#define _HAO_GLOBAL_H_

#include "hao_logic.h"
#include "hao_threadpool.h"

#include <sys/types.h>
#include <signal.h>
#include <tuple>
#include <string>
#include <cstdint>
#include <vector>

using std::vector;
using std::tuple;
using std::string;

enum class ProcessType {Master, Worker};

// 当前进程id
extern pid_t pid;

// 父进程id
extern pid_t ppid;

//extern int stop_event;

// 进程类型
extern ProcessType process_type;

extern sig_atomic_t worker_status_changed;

// 保存环境变量
extern size_t g_argv_space;
extern size_t g_env_space;
extern int g_argc;
extern vector<string> g_argv;
extern vector<char> g_environ;
// 系统进程环境变量地址
extern char **g_os_argv;

// 保存所有服务器监听地址
extern vector<ServerAddress> all_listen_address;

// 每个进程的epoll
extern Socket g_socket;
extern LogicSocket g_logic_socket;

extern ThreadPool g_threadpool;


#endif