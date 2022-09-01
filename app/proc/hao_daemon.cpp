#include "hao_server.h"
#include "hao_log.h"

#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <stdlib.h>

const int MAX_CLOSE = 1024;

using namespace hao_log;
using namespace hao_server;

// 0 on success,  -1 on error
// 要保留stderr的输出
int hao_server::MakeDaemon()
{
    // 设置为0，不要让它来限制文件权限，以免引起混乱
	umask(0);
    
    struct rlimit rl;
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0)
    {
        // 输出错误信息 can't get file limit
        LOG_ERROR << "can't get file limit";
        return -1;
    }
	// 成为后台进程
	switch (fork())
	{
		case -1:
			LOG_ERROR << "nost_daemon()中fork()失败";
			// 创建子进程失败，可以写日志
			return -1;
		case 0:
			// 子进程，走到这里直接break
			break;

		default:
			// 父进程，直接退出
			_exit(EXIT_SUCCESS);
	}

	// 只有子进程可以走到这里
	// 成为session leaser 
	if(setsid() == -1)
	{
		// 脱离终端，终端关闭，将与此子进程无关
        LOG_ERROR << "nost_daemon()中setsid()失败";
		return -1;
	}

    // 确保不再申请控制终端
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0)
        LOG_ERROR << "Can't ignore SIGHUP";
        // 输出错误信息 can't ignore SIGHUP 
    switch(fork())
    {
        case -1:
            LOG_ERROR << "nost_daemon()中fork()失败";
            return -1;
        case 0:
            // 子进程
            break;
        default:
            // 父进程
            _exit(EXIT_SUCCESS);
    }
    
    
    // 修改工作路径
    if(chdir("/") < 0)
    {
        LOG_ERROR << "can't change directory to /";
        return -1;
    }

    // 关闭所有打开的文件
    // if(rl.rlim_max == RLIM_INFINITY)
    //     rl.rlim_max = MAX_CLOSE;
    // for(int i = 0; i < rl.rlim_max; i++)
    //     close(i);

	// int fd = open("/dev/null", O_RDWR);
	// if(fd != STDIN_FILENO)
	// {
	// 	// 记录错误日志 can't open file /dev/null
	// 	LOG_ERROR << "NostMakeDaemon()中打开/dev/null失败";
	// 	return -1;
	// }
	// // 先关闭STDIN_FILENO(这是规矩，已经打开的描述符，改动前先关闭)
	// // 类似于指针指向null, 让/dev/null 成为标准输入
	// if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDERR_FILENO)
	// {
	// 	// 记录错误日志
	// 	LOG_ERROR << "NostMakeDaemon()中dup2(STDOUT)失败";
	// 	return -1;
	// }
	// if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
	// {
	// 	// 记录错误日志
	// 	LOG_ERROR << "NostMakeDaemon()中dup2(STDERR)失败";
	// 	return -1;
	// }

	// 这里其实保留了global_log_.log_fd_
	int fd = open("/dev/null", O_RDWR);
	if(fd == -1)
	{
		LOG_EMERG << "open /dev/null failed";
		return -1;
	}
	if(dup2(fd, STDIN_FILENO) == -1)
	{
		LOG_EMERG << "dup2(STDIN) failed";
		return -1;
	}
	if(dup2(fd, STDOUT_FILENO) == -1)
	{
		LOG_EMERG << "dup2(STDOUT) failed";
		return -1;
	}
	if(fd > STDERR_FILENO)
	{
		if(close(fd) == -1)
		{
			LOG_EMERG << "close " << fd << " fdiled.";
			return -1;
		}
	}
	return 0;
}