#ifndef __DC_RAFT_COMMON_LOG_H__
#define __DC_RAFT_COMMON_LOG_H__

#include <string.h>

namespace dc {

void RAFT_LOG_INIT(uint32_t file_size, uint32_t file_num);
void RAFT_LOG_CLEAN();
Log* RAFT_LOG(std::string& id = "");

class Log {
public:
    static uint32_t s_file_size_;
    static uint32_t s_file_num_;

public:
    Log(std::string& name, uint32_t f_size, uint32_t f_num);
    virtual ~Log();

    virtual void Trace(string& s);
    virtual void Debug(string& s);
    virtual void Info(string& s);
    virtual void Warning(string& s);
    virtual void Error(string& s);
    virtual void Fatal(string& s);

private:
    std::string name_;
};

}  // namespace dc

#endif      //  __DC_RAFT_COMMON_LOG_H__
