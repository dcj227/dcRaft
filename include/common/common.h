#ifndef __DC_RAFT_COMMON_H__
#define __DC_RAFT_COMMON_H__

namespace dc {

enum RaftRole {
    DEFAULT = 0,
    LEADER = 1,
    FOLLOWER,
    CONDIDATE
};

struct Node {
    uint32_t        ip;
    std::string     ip_str; 
    uint32_t        port;
    
    uint64_t        id;
    RaftRole        role;
    std::string     role_str;       // Leader, Follower, Condidate, ""
};

}   // namespace dc

#endif  //  __DC_RAFT_COMMON_H__

