#ifndef _HAO_LOGIC_H_
#define _HAO_LOGIC_H_

#include "hao_socket.h"

class LogicSocket
{
    public:
        LogicSocket();
        ~LogicSocket();
    public:
        void SendBodyPkgToClient(MsgHeader* p_msg_header, unsigned short msg_code);
        bool HandleRegister(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length);
        bool HandleLogin(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length);
        bool HandlePing(Connection* p_conn, MsgHeader* p_msg_header, char* p_pkg_body, uint16_t body_length);
    
        void HandlePingOut(MsgHeader* p_mgs_header, Timestamp cur_time);
        void HandleMessage(char *p_msg_buf);
        void AfterMessage(char *p_msg_buf);
};

#endif
