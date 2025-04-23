#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include "Client.h"
#include "HTTPrequest.h"
#include "HTTPresponse.h"
#include "Resourcehost.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// #ifdef __linux__
// #include <kqueue/sys/event.h>  //libkqueue Linux - 仅在从 Github 源编译 libkqueue 时有效
// #else
// #include <sys/event.h>
// #endif

// constexpr int32_t INVALID_SOCKET = -1;   INVALID_SOCKET 在windows中 winsock2.h已定义
constexpr uint32_t QUEUE_SIZE = 1024;

class HTTPServer 
{
    //Server Socket
    int32_t listenPort;
    int32_t listenSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int32_t dropUid;
    int32_t dropGid;

    // Kqueue
    struct timespec kqTimeout = {2, 0};  // 最长阻塞时间
    int32_t kqfd = -1;  // 队列描述符

    // struct kevent evList[QUEUE_SIZE];  //  已触发 kqueue 中过滤器的事件（每次最大 QUEUE_SIZE）
    fd_set readfds[QUEUE_SIZE];  // windows 下用文件描述符搭配select

    // client map,将套接字描述符映射到客户端对象
    std::unordered_map<int, std::shared_ptr<Client>> clientMap;

    //资源/文件系统
    std::vector<std::shared_ptr<ResourceHost>> hostList;  // 包含所有资源主机
    std::unordered_map<std::string, std::shared_ptr<ResourceHost>, std::hash<std::string>, std::equal_to<>> vhost;   // 虚拟主机。将主机字符串映射到资源主机，以便为请求提供服务
    //  使用string的默认哈希函数， 和默认相等比较函数

    //  连接处理
    void updateEvent(int ident, short filter, u_short flags, u_int fflags, int32_t data, void* udata);
    void acceptConnection();
    std::shared_ptr<Client> getClient(int clfd);
    void disconnectClient(std::shared_ptr<Client> cl, bool mapErase=true);
    void readClient(std::shared_ptr<Client> cl, int32_t data_len);
    bool writeClient(std::shared_ptr<Client> cl, int32_t avail_bytes);
    std::shared_ptr<ResourceHost> getResourceHostForRequest(const HTTPRequest* const req);

    // 请求处理
    void handleRequest(std::shared_ptr<Client> cl, HTTPRequest* const req);
    void handleGet(std::shared_ptr<Client> cl, const HTTPRequest* const req);
    void handleOptions(std::shared_ptr<Client> cl, const HTTPRequest* const req);
    void handleTrace(std::shared_ptr<Client> cl, HTTPRequest* const req);

    // 响应
    void sendStatusResponse(std::shared_ptr<Client> cl, int32_t status, std::string const& msg = "");
    void sendResponse(std::shared_ptr<Client> cl, std::unique_ptr<HTTPResponse> resp, bool disconnect);

    bool canRun=false;

    HTTPServer(std::vector<std::string> const& vhost_aliases, int32_t port, std::string const& diskpath, int32_t drop_uid=0, int32_t drop_gid=0);
    ~HTTPServer();

    bool start();
    void stop();

    void process();
};

#endif