#include "socket_event.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace dc {

SocketEvent::SocketEvent(TCP_UDP type/* = SOCKET_TCP*/, bool isEPOLLET/* = true*/)
    : socket_type_(type)
    , epoll_event_(isEPOLLET) {
    recvBuf_.reserve(4096);
}

SocketEvent::~SocketEvent() {
    std::map<int, SocketInfo>::iterator it;
    for (it = fd_si_.begin(); it != fd_si_.end(); it++) {
        DelSocket(it->second.fd);
    }
}

int SocketEvent::Initialize() {
    if (epoll_event_.Initialize() != 0) {
        return -1;
    }
}

void SocketEvent::OnRead(int fd) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        fprintf(stderr, "SocketEvent, fd OnRead but not in fd_si_ map, remove it, fd:%d\n", fd);
        epoll_event_.DelEvent(fd);
        return;
    }

    if (it->second.listen) {
        sockaddr_in cli_addr;
        socklen_t cli_len;
        int cli_fd = accept(fd, (sockaddr *)&cli_addr, &cli_len);

        if (cli_fd < 0) {
            fprintf(stderr, "SocketEvent, accept error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
        } else {
            reinterpret_cast<SocketEvent*>(it->second.handler)->OnAccept(cli_fd, cli_addr, cli_len);
        }
        
    } else {
        int count = recv(fd, const_cast<char*>(recvBuf_.data()), recvBuf_.capacity(), MSG_PEEK);
        if (count < 0) {
            fprintf(stderr, "SocketEvent, OnRead recv error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
            // TODO
        } else if (count == 0) {
            DelSocket(it->second.fd); 
        } else {
            it->second.handler->OnRecv(recvBuf_);
        }
    }

    return;
}

void SocketEvent::OnWrite(int fd) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        fprintf(stderr, "SocketEvent, fd OnWrite but not in fd_si_ map, remove it, fd:%d\n", fd);
        epoll_event_.DelEvent(fd);
        return;
    }

    SocketFdHandler* handler = it->second.handler;
    if (handler) {
        std::string& sendBuf = handler->sendBuf();
        int count = send(fd, sendBuf.c_str(), sendBuf.size(), 0);
        if (count < 0) {
            fprintf(stderr, "SocketEvent, OnWrite send error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
            // TODO 
        } else if (count < sendBuf.size()) {
            std::string leftBuf(sendBuf, count, sendBuf.size() - count);
            sendBuf.swap(leftBuf);
        } else {
            sendBuf.empty();
        }
    }
}

void SocketEvent::OnError(int fd, int err, std::string& error) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        fprintf(stderr, "SocketEvent, fd OnWrite but not in fd_si_ map, remove it, fd:%d\n", fd);
        epoll_event_.DelEvent(fd);
        return;
    }
    SocketFdHandler* handler = it->second.handler;
    if (handler) {
        handler->OnError(fd, err, error);
        fprintf(stderr, "SocketEvent, OnError fd:%d, errno:%d, error:%s\n", fd, err, error.c_str());
    }
}

/*
void SocketEvent::OnAccept(int fd) {
    if (fd < 0) {
        fprintf(stderr, "SocketEvent, fd OnAccept fd < 0, fd:%d\n", fd);
        return;
    }

    SocketFdHandler* phandler = new SocketFdHandler();
    
    int ret = AddSocket(fd, phandler);
    if (ret != 0) {
        fprintf(stderr, "SocketEvent, OnAccept, AddSocket fail, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
        return;
    }
}
*/

int SocketEvent::AddSocket(int fd, SocketFdHandler* sfd, uint32_t events/* = SOCKET_READ|SOCKET_WRITE|SOCKET_ERROR*/) {

    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it != fd_si_.end()) {
        return -1;
    }

    SetNonBlocking(fd);

    SocketInfo si = {fd, false, sfd}; 
    fd_si_[fd] = si; 

    events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
    return epoll_event_.AddEvent(fd, this, events);
}

int SocketEvent::AddListenSocket(std::string& ip_s, int port, SocketFdHandler* sfd, uint32_t events/* = SOCKET_READ|SOCKET_ERROR*/) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton(ip_s.c_str(), &(addr.sin_addr)); 
    addr.sin_port=htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    SetNonBlocking(fd);

    bind(fd, (sockaddr *)&addr, sizeof(addr));

    listen(fd, LISTENQUEUE);

    SocketInfo si = {fd, true, sfd}; 
    fd_si_[fd] = si; 

    events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
    return epoll_event_.AddEvent(fd, this, events);
}

/*
int SocketEvent::ModSocketEvent(int fd, uint32_t events) {
    events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
    return epoll_event_.ModEvent(fd, events); 
}
*/

int SocketEvent::DelSocket(int fd) {
    int ret = epoll_event_.DelEvent(fd);
    if (ret != 0) {
        fprintf(stderr, "SocketEvent, DelSocket fail, fd:%d, errno:%d, error\n", fd, errno, strerror(errno));
    }

    fd_si_.erase(fd);

    close(fd);

    return 0;
}

int SocketEvent::Wait(int timeout) {
    return epoll_event_.Wait(timeout);
}

SocketFdHandler* SocketEvent::GetHandler(int fd) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        return NULL;
    }

    return it->second.handler;
}

int SocketEvent::SetNonBlocking(int fd) {
    int opts;
    opts = fcntl(fd, F_GETFL);
    if(opts<0) {
        perror("fcntl(sock, GETFL)");
        return -1; 
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(fd, F_SETFL, opts) < 0) {
        perror("fcntl(sock, SETFL, opts)");
        return -1; 
    } 
    return 0;
}

}   // namespace dc
