#include "epoll_event.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace dc {

#define EVENT_SIZE 1024             // for epoll_create

EpollEvent::EpollEvent(bool isEPOLLET)
    : epoll_fd_(-1)
    , events_(NULL)
    , isEPOLLET_(isEPOLLET) {
}

EpollEvent::~EpollEvent() {
    if (events_) {
        delete [] events_;
        events_ = NULL;
    }

    if (epoll_fd_ != -1) {
        close(epoll_fd_);
    }
}

int EpollEvent::Initialize() {
    epoll_fd_ = epoll_create(EVENT_SIZE);
    if (epoll_fd_ < 0) {
        fprintf(stderr, "epoll_create error: %d, %s\n", errno, strerror(errno));
        return errno;
    }

    events_ = new epoll_event[EVENT_SIZE]; 

    return 0;
}

int EpollEvent::AddEvent(int fd, EventHandler* efd, uint32_t events) {

    epoll_event ee;
    ee.events = isEPOLLET_ ? events : events | EPOLLET;
    ee.data.fd = fd;

    int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ee); 

    if (ret == 0) {
        EH eh = {ee, efd};
        fd_eh_[fd] = eh;   
    }

    return ret;
}

int EpollEvent::ModEvent(int fd, uint32_t events) {
    std::map<int, EH>::iterator it = fd_eh_.find(fd);
    if (it == fd_eh_.end()) {
        fprintf(stderr, "fd not in epoll, can not mod, fd: %d\n", fd);
        return -1;
    }

    it->second.ee.events = events;

    return  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &it->second.ee); 
}

int EpollEvent::RemodEvent(int fd) {
    std::map<int, EH>::iterator it = fd_eh_.find(fd);
    if (it == fd_eh_.end()) {
        fprintf(stderr, "fd not in epoll, can not mod, fd: %d\n", fd);
        return -1;
    }

    return  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &it->second.ee); 
}

int EpollEvent::DelEvent(int fd) {
    std::map<int, EH>::iterator it = fd_eh_.find(fd);
    if (it == fd_eh_.end()) {
        return 0;
    } else {
        int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &it->second.ee); 
        if (ret == 0) {
            fd_eh_.erase(fd);
        }
        return ret;
    }
}

int EpollEvent::Wait(int timeout) {
    int nfds = epoll_wait(epoll_fd_, events_, EVENT_SIZE, timeout); 

    for (int i = 0; i < nfds; i++) {
        std::map<int, EH>::iterator it = fd_eh_.find(events_[i].data.fd);
        
        if (events_[i].events & (EPOLLERR|EPOLLHUP)) {
            fprintf(stderr, "EPOLLERR | EPOLLHUP fd: %d\n", events_[i].data.fd);

            if (it != fd_eh_.end() && it->second.efd) {
                //int err = events_[i].events & EPOLLERR ? EPOLLERR : (events_[i].events & EPOLLHUP)
                std::string error = strerror(errno);
                it->second.efd->OnError(it->second.ee.data.fd, errno, error);
            }
        }

        if ((events_[i].events & (EPOLLERR|EPOLLHUP)) 
            && (events_[i].events & (EPOLLIN|EPOLLOUT)) == 0) {
            // since nginx do this way
            events_[i].events |= EPOLLIN|EPOLLOUT;
        }

        if (events_[i].events & EPOLLIN) {
            if (it != fd_eh_.end() && it->second.efd) {
                it->second.efd->OnRead(it->second.ee.data.fd);
            }
        }

        if (events_[i].events & EPOLLOUT) {
            if (it != fd_eh_.end() && it->second.efd) {
                it->second.efd->OnWrite(it->second.ee.data.fd);
            }
        }
    }

    return 0;
}

}  // namespace dc
