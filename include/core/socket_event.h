#ifndef __DC_SOCKET_EVENT_H__
#define __DC_SOCKET_EVENT_H__

#include "epoll_event.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LISTENQUEUE 20

namespace dc {

enum SOCKET_EVENTS {
    SOCKET_READ = 0x001,
    SOCKET_WRITE = 0x004,
    SOCKET_ERROR = 0x008 | 0x10
};

enum TCP_UDP {
    SOCKET_TCP = 0,
    SOCKET_UDP
};

class SocketFdHandler {
public:
    virtual void OnRecv(std::string& recveBuf) = 0;
    virtual void OnError(int fd, int err, std::string& error) = 0;

    void Send(std::string& sendBuf) {
        sendBuf_.append(sendBuf, 0, sendBuf.size());
    };

    std::string& sendBuf() { return sendBuf_; };        // for SocketEvent use when OnWrite

protected:
    std::string sendBuf_;
};

class SocketEvent : public EventHandler {
public:
    SocketEvent(TCP_UDP type = SOCKET_TCP, bool isEPOLLET = true);
    virtual ~SocketEvent();

    int Initialize();

    virtual void OnRead(int fd);
    virtual void OnWrite(int fd);
    virtual void OnError(int fd, int err, std::string& error);
    virtual void OnAccept(int fd, sockaddr_in& addr, socklen_t len) = 0;

    int AddSocket(int fd, SocketFdHandler* sfd, uint32_t events = SOCKET_READ|SOCKET_WRITE|SOCKET_ERROR);
    int AddListenSocket(std::string& ip_s, int port, SocketFdHandler* sfd, uint32_t events = SOCKET_READ|SOCKET_ERROR);
    //int ModSocketEvent(int fd, uint32_t events);
    int DelSocket(int fd);

    int Wait(int timeout);

    SocketFdHandler* GetHandler(int fd);

    struct SocketInfo {
        int fd;
        bool listen;
        SocketFdHandler* handler; 
    };

private:
    int SetNonBlocking(int fd);

    TCP_UDP socket_type_;

    EpollEvent epoll_event_;

    //<fd, SocketInfo>
    std::map<int, SocketInfo> fd_si_;

    std::string recvBuf_;
};

}   // namespace dc

#endif  //  __DC_SOCKET_EVENT_H__
