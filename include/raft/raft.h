#pragma once

#include <cstdlib>
#include <string>
#include "common.h"

namespace dcraft {

class Raft {
public:
    Raft(Config& c);
    virtual ~Raft();

    virtual int Apply(void* data) = 0;

    int AddNode();
    int RemoveNode();

private:
    void RunLeader();
    void RunFollower();
    void RunCondidate();

    Cluster* cluster_;
    LogStore* store_;
    LogReplicate* replicate_ 
    Fsm* fsm_;
    SnapShot* snap_shot_;
    Log* log_; 

    Node self_;
    std::vector<Node> others_;

};

}
