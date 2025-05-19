#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <signal.h>

#include "HTTPserver.h"
#include "Resourcehost.h"

static std::unique_ptr<HTTPServer> svr;

// 使用该功能忽略信号
void handleSigPipe([[maybe_unused]] int32_t snum){
    return;
}

// 终止信号处理器
void handleTermSig([[maybe_unused]] int32_t snum){
    svr->canRun = false;
}

int main()
{
    // 解析配置文件
    std::map<std::string, std::string, std::less<>> config;
    std::fstream cfile;
    std::string line;
    std::string key;
    std::string val;
    int32_t epos = 0;
    int32_t drop_uid = 0;
    int32_t drop_gid = 0;
    cfile.open("server.config");
    if (!cfile.is_open()){
        std::cout << "Unable to open server.config file in working directory" << std::endl;
        return -1;
    }
    while (getline(cfile, line)){
        // 跳过空行 或以a#开头
        if (line.length() == 0 || line.rfind("#", 0) == 0)
            continue;
        epos = line.find("=");
        key = line.substr(0, epos);
        val = line.substr(epos + 1, line.length());
        config.try_emplace(key, val);
    }
    cfile.close();
    // 验证 vhost、端口和磁盘路径是否存在
    if (!config.contains("vhost") || !config.contains("port") || !config.contains("diskpath")) {
        std::cout << "vhost, port, and diskpath must be supplied in the config, at a minimum" << std::endl;
        return -1;
    }
    // 将 vhost 分解为逗号分隔的列表（如果有多个 vhost 别名）
    std::vector<std::string> vhosts;
    std::string vhost_alias_str = config["vhost"];
    std::string delimiter = ",";
    std::string token;
    size_t pos = 0;
    do {
        pos = vhost_alias_str.find(delimiter);
        token = vhost_alias_str.substr(0, pos);
        vhosts.push_back(token);
        vhost_alias_str.erase(0, pos + delimiter.length());
    } while (pos != std::string::npos);

    // 检查可选的 drop_uid、drop_gid。 确保均已设置
    if (config.contains("drop_uid") && config.contains("drop_gid")) {
        drop_uid = atoi(config["drop_uid"].c_str());
        drop_gid = atoi(config["drop_gid"].c_str());

        if (drop_uid <= 0 || drop_gid <= 0) {
            // Both must be set, otherwise set back to 0 so we dont use
            drop_uid = drop_gid = 0;
        }
    }
    // 当套接字连接中断时，忽略 SIGPIPE “管道破裂 ”信号。
    signal(SIGPIPE, handleSigPipe);
    // 寄存器终止信号
    signal(SIGABRT, &handleTermSig);
    signal(SIGINT, &handleTermSig);
    signal(SIGTERM, &handleTermSig);

    // 实例化并启动服务器
    svr = std::make_unique<HTTPServer>(vhosts, atoi(config["port"].c_str()), 
                                        config["diskpath"], drop_uid, drop_gid);
    if (!svr->start()) {
        svr->stop();
        return -1;
    }
    // Run main event loop
    svr->process();

    // Stop the server
    svr->stop();

    return 0;
}