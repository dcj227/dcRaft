#pragma once

#include <cstdlib>
#include <cstdio>
#include <string>
#include <memory>
#include <rapidjson/document.h>
#include "log.h"

/*
{
    "clusters":{
        "self":{
            "ip":"1.1.1.1",
            "port":17873
        },
        "others":[
        {
            "ip":"1.1.1.2",
            "port":17873
        },
        {
            "ip":"1.1.1.3",
            "port":17873
        }
        ]
    },
    "raft":{
        "data_dir":"/data/.raft/data",
        "snapshot":"snapshot"
    },
    "log":{
        "dir":"/data/.raft/log",
        "file":"raft.log",
        "size":"100M",
        "num":100
    }
}
*/

namespace dcraft {

#define DEFAULT_RAFT_PORT 17873
#define DEFAULT_DATA_DIR "data/.raft/data"
#define DEFAULT_SNAPSHOT "snapshot"
#define DEFAULT_SNAPSHOT_FILE "snapshot.dat"
#define DEFAULT_LOG_DIR "/data/.raft/log"
#define DEFAULT_LOG_FILE "raft.log"
#define DEFAULT_LOG_SIZE 100 * 1024 * 1024
#define DEFAULT_LOG_NUM 100

class Config {
public:
    Config(std::string& path)
        : log_dir_(DEFAULT_LOG_DIR)
        , log_file_(DEFAULT_LOG_FILE)
        , log_size_(DEFAULT_LOG_SIZE)
        , log_num_(DEFAULT_LOG_NUM)
        , data_dir_(DEFAULT_DATA_DIR)
        , snapshot_dir_(DEFAULT_SNAPSHOT)
        , snapshot_file_(DEFAULT_SNAPSHOT_FILE) {
        conf_file_ = path;
    }

    int Initialize() {
        std::shared_ptr<FILE> f_ptr = make_shared<FILE>(fopen(path.c_str(), "r"), [f]() { if (f) { fclose(f); } });
        if (!f_ptr.get()) {
            fprintf(stderr, "open conf file error!\n");
            return -1;
        }

        rapidjson::Document document;
        rapidjson::FileStream inputStream(f);
        newDoc.ParseStream<0>(inputStream);

        if (document.HasParseError()) {
            fprintf(stderr, "Json Parse error: %d\n", document.GetParseError());
            return -1;
        }

        if (document.HasMember("log")) {
            rapidjson::Value& jlog = document["log"];
            if (jlog.HasMember("dir") && !jlog["dir"].asString().empty()) {
                log_dir_ = jlog["dir"].asString();
            }
            if (jlog.HasMember("file") && !jlog["file"].asString().empty()) {
                log_file_ = jlog["file"].asString();
            }
            if (jlog.HasMember("size") && !jlog["size"].asString().empty()) {
                std::string log_size_str = jlog["size"].asString();
                log_size_ = std::atoi(log_size_str.substr(0, log_size_str.size() -1) * 1024 * 1024;
            }
            if (jlog.HasMember("num")) {
                log_num_ = jlog["num"].asInt();
            }
            
        }

        // init log     
        RAFT_LOG_INIT(file_size_, file_num);
    
        if (!document.HasMember("clusters")) {
            RAFT_LOG()->Error("Json conf has no clusters.\n");
            return -1;
        }
        rapidjson::Value& jclusters = document["clusters"];

        if (!jclusters.HasMember("self")) {
            RAFT_LOG()->Error("Json conf has no self.\n");
            return -1;
        }

        rapidjson::Value& jself = jclusters["self"];
        if (jself.HasMember("ip") || jself["ip"].asString().empty()) {
            RAFT_LOG()->Error("Json conf empty ip.\n");
            return -1;
        }
        std::string ip_str = jself["ip"].asString();
        uint32_t ip = IpStrToInt(ip_str);
        if (ip == 0) {
            RAFT_LOG()->Error("Json conf incorrect ip.\n");
            return -1;
        }
        uint32_t port = jself.HasMember("port") ? jself["port"].asInt() : DEFAULT_RAFT_PORT;
        self_ = {ip, ip_str, port, IdByIpPort(ip, port), RaftRole::DEFAULT, ""};

        if (!jclusters.HasMember("others")) {
            RAFT_LOG()->Error("Json conf has no others.\n");
            return -1;
        }
        rapidjson::Value& jothers = jclusters["others"];
        for (int i = 0; i < jothers.Size(); i++) {
            rapidjson::Value& jitem = jothers[i];
            if (jitem.HasMember("ip") || jitem["ip"].asString().empty()) {
                RAFT_LOG()->Error("Json conf other empty ip.\n");
                return -1;
            }

            std::string ip_str = jitem["ip"].asString();
            uint32_t ip = IpStrToInt(ip_str);
            if (ip == 0) {
                RAFT_LOG()->Error("Json conf incorrect ip.\n");
                return -1;
            }
            uint32_t port = jitem.HasMember("port") ? jitem["port"].asInt() : DEFAULT_RAFT_PORT;
            Node other = {ip, ip_str, port, IdByIpPort(ip, port), RaftRole::DEFAULT, ""};

            others_.push_back(other);
        }

        if (document.HasMember("raft")) {
            rapidjson::Value& jraft = document["raft"];
            if (jraft.HasMember("dir") && !jraft["dir"].asString().empty()) {
               data_dir_ = jraft["dir"].asString();
            }
            if (jraft.HasMember("snapshot") && !jraft["snapshot"].asString().empty()) {
               snapshot_dir__ = jraft["snapshot"].asString();
            }
        }
    }
    
    std::string log_dir_;
    std::string log_file_;
    uint32_t log_size_;
    uint32_t log_num_;

    std::string data_dir_;

    Node self_;
    std::vector<Node> others_;

    // 不能配置
    std::string snapshot_dir_;        // 相对于data_dir_的路径, 文件名snapshot.dat
    std::string snapshot_file_;

    std::string conf_file_;
};

}   // namespace dc
