#ifndef _HAO_SERVER_H_
#define _HAO_SERVER_H_

namespace hao_server
{
    void ServerInit(int argc, char *argv[]);
    // 切换为守护进程
    int MakeDaemon();
    void MasterProcessCycle();
}
#endif