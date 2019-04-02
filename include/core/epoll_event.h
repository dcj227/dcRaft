#ifndef __DC_EPOLL_EVENT_H__
#define __DC_EPOLL_EVENT_H__

#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>

namespace dc {

class EventHandler {
public:
    virtual void OnRead(int fd) = 0;
    virtual void OnWrite(int fd) = 0;
    virtual void OnError(int fd, int err, std::string& error) = 0;
};

class EpollEvent {
public:
    EpollEvent(bool isEPOLLET);
    virtual ~EpollEvent();

    int Initialize();

    int AddEvent(int fd, EventHandler* efd, uint32_t events);
    int ModEvent(int fd, uint32_t events);
    int DelEvent(int fd);

    int Wait(int timeout);

    struct EH {
        epoll_event ee;
        EventHandler* efd;
    };

private:
    int epoll_fd_;
    epoll_event* events_;       // 接收事件
    bool isEPOLLET_;

    // <fd, EH>
    std::map<int, EH> fd_eh_;
};

}

#endif  //  __DC_EPOLL_EVENT_H__
