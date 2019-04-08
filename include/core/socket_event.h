#ifndef __DC_SOCKET_EVENT_H__
#define __DC_SOCKET_EVENT_H__

#include "epoll_event.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>

#define LISTENQUEUE 20
#define CONNECT_TIMEOUT_MS 500

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

enum SOCKET_TYPE {
    TYPE_LISTEN = 0,
    TYPE_ACCEPT,
    TYPE_CONNECT
};

enum SOCKET_STATE {
    STATE_DEFAULT = 0,
    STATE_LISTEN,
    STATE_CONNECT,
    STATE_READWRITE,
    STATE_CLOSE
};

class SocketEvent;

class SocketFdHandler {
public:
    SocketFdHandler(uint32_t ip,
                    int port,
                    SocketEvent* se,
                    int fd = -1)     // 如果为Connection fd， 则可不填
        : fd_(fd)
        , ip_(ip)
        , port_(port)
        , se_(se) {
    }

    ~SocketFdHandler() {
    }

    virtual void OnRecv(std::string& recveBuf) = 0;
    virtual void OnError(int fd, int err, std::string& error) = 0;

    void Send(std::string& sendBuf);

    void SetFd(int fd);

    std::string& sendBuf() { return sendBuf_; };        // for SocketEvent use when OnWrite

protected:
    std::string sendBuf_;

    int fd_;
    uint32_t ip_;
    int port_;

    SocketEvent* se_;
};

class SocketEvent : public EventHandler {
public:
    SocketEvent(TCP_UDP type = SOCKET_TCP, bool isEPOLLET = true);
    virtual ~SocketEvent();

    int Initialize();

    virtual void OnRead(int fd, uint32_t events);
    virtual void OnWrite(int fd, uint32_t events);
    virtual void OnError(int fd, uint32_t events, int err, std::string& error);

    virtual void OnAccept(int fd, uint32_t ip, int port) = 0;

    /*
     * delete sfd after DelSocket from SocketEvent
     */
    int AddListenSocket(std::string& ip_str, int port, uint32_t events = SOCKET_READ|SOCKET_ERROR);
    int AddConnection(std::string& ip_str, int port, SocketFdHandler* sfd, uint32_t events = SOCKET_READ|SOCKET_ERROR);
    int DelSocket(int fd);

    int Wait(uint64_t now_ms, int timeout_ms = 0);

    SocketFdHandler* GetHandler(int fd);

    int RemodSocketEvent(int fd);

    struct SocketInfo {
        int fd;
        SOCKET_TYPE type;
        SOCKET_STATE state;
        SocketFdHandler* handler; 
        uint64_t expired_time_ms;       // 只有connect fd 会用到
    };

protected:
    /*
     * delete sfd after DelSocket from SocketEvent
     */
    int AddSocket(int fd, SocketFdHandler* sfd, uint32_t events = SOCKET_READ|SOCKET_WRITE|SOCKET_ERROR);

private:
    int SetNonBlocking(int fd);

    TCP_UDP socket_type_;

    EpollEvent epoll_event_;

    //<fd, SocketInfo>
    std::map<int, SocketInfo> fd_si_;

    std::string recvBuf_;

    uint64_t timeout_ms_;
    uint64_t now_ms_;
    std::queue<int> connect_fds_;
};

}   // namespace dc

#endif  //  __DC_SOCKET_EVENT_H__
