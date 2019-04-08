#include "socket_event.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace dc {

void SocketFdHandler::Send(std::string& sendBuf) {
    sendBuf_.append(sendBuf, 0, sendBuf.size());
    if (se_) {
        se_->RemodSocketEvent(fd_);
    } 
};

void SocketFdHandler::SetFd(int fd) {
    fd_ = fd;
}


SocketEvent::SocketEvent(TCP_UDP type, bool isEPOLLET)
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
    return 0;
}

void SocketEvent::OnRead(int fd, uint32_t events) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        fprintf(stderr, "SocketEvent, fd OnRead but not in fd_si_ map, remove it, fd:%d\n", fd);
        epoll_event_.DelEvent(fd);
        return;
    }

    if (it->second.type == TYPE_LISTEN) {
        sockaddr_in cli_addr;
        socklen_t cli_len;
        int cli_fd = accept(fd, (sockaddr *)&cli_addr, &cli_len);

        if (cli_fd < 0) {
            fprintf(stderr, "SocketEvent, accept error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
        } else {
            reinterpret_cast<SocketEvent*>(it->second.handler)->OnAccept(cli_fd, cli_addr.sin_addr.s_addr, cli_addr.sin_port);
        }

    } else if (it->second.type == TYPE_CONNECT && it->second.state == STATE_CONNECT) {  // 连接成功

        int err = 0;
        socklen_t len = sizeof(err);
        int status = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (status == 0) {              // connect success
            it->second.state = STATE_READWRITE;
            OnRead(fd, SOCKET_READ);
        } else {
            std::string error = strerror(err);
            it->second.handler->OnError(fd, err, error);
        }

        events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
        int ret = epoll_event_.AddEvent(fd, this, events);
        if (ret != 0) {
            fprintf(stderr, "SocketEvent, fd OnRead AddEvent error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
        }

    } else {
        int count = recv(fd, const_cast<char*>(recvBuf_.data()), recvBuf_.capacity(), MSG_PEEK);
        if (count < 0) {
            fprintf(stderr, "SocketEvent, OnRead recv error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
            // TODO
        } else if (count == 0) {
            DelSocket(it->second.fd); 
        } else if (it->second.handler) {
            it->second.handler->OnRecv(recvBuf_);
        }
    }

    return;
}

void SocketEvent::OnWrite(int fd, uint32_t events) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        fprintf(stderr, "SocketEvent, fd OnWrite but not in fd_si_ map, remove it, fd:%d\n", fd);
        epoll_event_.DelEvent(fd);
        return;
    }

    if (it->second.type == TYPE_CONNECT && it->second.state == STATE_CONNECT) {     // 连接成功

        int err = 0;
        socklen_t len = sizeof(err);
        int status = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (status == 0) {              // connect success
            it->second.state = STATE_READWRITE;
        } else {
            std::string error = strerror(err);
            it->second.handler->OnError(fd, err, error);
        }

        events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
        int ret = epoll_event_.AddEvent(fd, this, events);
        if (ret != 0) {
            fprintf(stderr, "SocketEvent, fd OnWrite AddEvent error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
        }
    }

    SocketFdHandler* handler = it->second.handler;
    if (handler) {
        std::string& sendBuf = handler->sendBuf();
        int count = send(fd, sendBuf.c_str(), sendBuf.size(), 0);
        if (count < 0) {
            fprintf(stderr, "SocketEvent, OnWrite send error, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
            // TODO 
        } else if (static_cast<unsigned int>(count) < sendBuf.size()) {
            std::string leftBuf(sendBuf, count, sendBuf.size() - count);
            sendBuf.swap(leftBuf);
        } else {
            //sendBuf.clear();
        }
    }
}

void SocketEvent::OnError(int fd, uint32_t events, int err, std::string& error) {
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
void SocketEvent::OnAccept(int fd, uint32_t ip, int port) {
    if (fd < 0) {
        fprintf(stderr, "SocketEvent, fd OnAccept fd < 0, fd:%d\n", fd);
        return;
    }

    SocketFdHandler* phandler = new SocketFdHandler(fd, ip, port, this);
    
    int ret = AddSocket(fd, phandler);
    if (ret != 0) {
        fprintf(stderr, "SocketEvent, OnAccept, AddSocket fail, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
    }
}
*/

int SocketEvent::AddListenSocket(std::string& ip_str, int port, uint32_t events) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton(ip_str.c_str(), &(addr.sin_addr)); 
    addr.sin_port=htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    SetNonBlocking(fd);

    bind(fd, (sockaddr *)&addr, sizeof(addr));

    listen(fd, LISTENQUEUE);

    events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
    int ret = epoll_event_.AddEvent(fd, this, events);
    if (ret != 0) {
        close(fd);
        fprintf(stderr, "SocketEvent, AddEvent fail, ip:%s, port:%d, errno:%d, error:%s\n", ip_str.c_str(), port, errno, strerror(errno));
    }

    SocketInfo si = {fd, TYPE_LISTEN, STATE_LISTEN, NULL, 0}; 
    fd_si_[fd] = si; 

    return ret;
}

int SocketEvent::AddConnection(std::string& ip_str, int port, SocketFdHandler* sfd, uint32_t events) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton(ip_str.c_str(), &(addr.sin_addr)); 
    addr.sin_port=htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    SetNonBlocking(fd);

    int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    if (ret == 0) {     // 连接成功, 本地连接可能出现
        events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
        int ret = epoll_event_.AddEvent(fd, this, events);
        if (ret != 0) {
            close(fd);

            fprintf(stderr, "SocketEvent, AddConnection fail, ip:%s, port:%d, errno:%d, error:%s\n", ip_str.c_str(), port, errno, strerror(errno));
            return -1;
        }

        sfd->SetFd(fd);
         
        SocketInfo si = {fd, TYPE_CONNECT, STATE_READWRITE, sfd, 0}; 
        fd_si_[fd] = si; 

        //sfd->OnRecv(recvBuf_);

        return 0;
    } else if (errno == EINPROGRESS) {      // 正在建立连接
        sfd->SetFd(fd);

        SocketInfo si = {fd, TYPE_CONNECT, STATE_CONNECT, sfd, now_ms_ + timeout_ms_ + CONNECT_TIMEOUT_MS};
        fd_si_[fd] = si; 

        connect_fds_.push(fd); 

        return -2;
    } else {
        fprintf(stderr, "SocketEvent, AddConnection fail, ip:%s, port:%d, errno:%d, error:%s\n", ip_str.c_str(), port, errno, strerror(errno));
        return -1;
    }
}

int SocketEvent::DelSocket(int fd) {
    int ret = epoll_event_.DelEvent(fd);
    if (ret != 0) {
        fprintf(stderr, "SocketEvent, DelSocket fail, fd:%d, errno:%d, error:%s\n", fd, errno, strerror(errno));
    }

    fd_si_.erase(fd);

    close(fd);

    return 0;
}

int SocketEvent::Wait(uint64_t now_ms, int timeout_ms) {
    timeout_ms_ = timeout_ms;
    now_ms_ = now_ms;

    while (1) {
        if (connect_fds_.size() <= 0) {
            break;
        }

        int fd = connect_fds_.front();
        std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
        if (it == fd_si_.end()) {
            connect_fds_.pop();
            continue;
        }

        if (it->second.expired_time_ms < now_ms) {
            if (it->second.handler) {
                std::string error = "connect timeout.";
                it->second.handler->OnError(fd, -2, error);
            } 
            fd_si_.erase(it);
            connect_fds_.pop();
            continue;
        }
        break;
    };

    return epoll_event_.Wait(timeout_ms);
}

SocketFdHandler* SocketEvent::GetHandler(int fd) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it == fd_si_.end()) {
        return NULL;
    }

    return it->second.handler;
}

int SocketEvent::RemodSocketEvent(int fd) {
    return epoll_event_.RemodEvent(fd); 
}

int SocketEvent::AddSocket(int fd, SocketFdHandler* sfd, uint32_t events) {
    std::map<int, SocketInfo>::iterator it = fd_si_.find(fd);
    if (it != fd_si_.end()) {
        return -1;
    }

    SetNonBlocking(fd);

    SocketInfo si = {fd, TYPE_ACCEPT, STATE_READWRITE, sfd, 0}; 
    fd_si_[fd] = si; 

    events = events & SOCKET_READ & SOCKET_WRITE & SOCKET_ERROR;
    return epoll_event_.AddEvent(fd, this, events);
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
