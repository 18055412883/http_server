#include "HTTPserver.h"

#include <vector>
#include <string>
#include <ctime>
#include <memory>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <kqueue/sys/event.h>
#else
#include <sys/event.h>
#endif


// server 构造函数
// 初始化状态和服务器变量
// @param vhost_aliases HTTP 服务器将响应的主机名列表
// @param port 虚拟主机监听的端口
// @param diskpath vhost 服务文件夹的路径
// @param drop_uid UID 在 bind() 之后设置为 uid。 如果为 0 则忽略
// @param drop_gid 在 bind() 后设置为 GID。 若为 0 则忽略
HTTPServer::HTTPServer(std::vector<std::string> const& vhost_aliases, int32_t port,
                        std::string const& diskpath, int32_t drop_uid, int32_t drop_gid):
                        listenPort(port),
                        dropUid(drop_uid),
                        dropGid(drop_gid){
    std::cout << "Port: " << port << std::endl;
    std::cout << "Disk path: " << diskpath << std::endl;
    // 在磁盘上创建一个为基本路径 ./htdocs 服务的资源主机
    auto resHost = std::make_shared<ResourceHost>(diskpath);
    hostList.push_back(resHost);
    // 始终为 localhost/127.0.0.1 提供服务（这就是为什么我们只在 hostList 中添加了一个 ResourceHost 的原因）
    vhost.try_emplace("localhost:" + listenPort, resHost);
    vhost.try_emplace("127.0.0.1:" + listenPort, resHost);
    // 设置为 htdocs 服务的资源主机，以提供 vhost 别名
    for (auto const& vh : vhost_aliases){
        if (vh.length() >= 122){
            std::cout << "vhost " << vh << "too long, skipping" << std::endl;
            continue;
        }
        std::cout << "vhost: " << vh << std::endl;
        vhost.try_emplace(vh + ":" + listenPort, resHost);
    } 
}

// server 析构函数
HTTPServer::~HTTPServer(){
    hostList.clear();
    vhosts.clear();
}

// start server
// 通过请求socket handle、binding和进入监听状态来初始化服务器套接字
// 如果初始化成功，则返回 True。否则为 False
bool HTTPServer::start(){
    canRun = true;
    // 创建一个监听套接字的handle
    listtenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET){
        std::cout << "Could not create socket" << std::endl;
        return false;
    }

    // 设置套接字为非阻塞模式
    fcntl(listenSocket, F_SETFL, O_NONBLOCK);

    // 填充server address 结构
    memset(&serverAddr, 0, sizeof(struct sockaddr_in));  // clear struct
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(listenPort);  // 设置端口（从主机顺序转换为净字节顺序）
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 智能选择服务器主机地址


    // 绑定套接字到服务器地址
    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0){
        std::cout << "Failed to bind to the address" << std::endl;
        return false;
    }
    // 可选择删除 uid/gid
    if (dopUid > 0 && dropGid > 0){
        if (setgid(dropGid) != 0){
            std::cout << "setgid to " << dropGid << " failed!" << std::endl;
            return false;
        }
        if (setuid(dropUid) != 0){
            std::cout << "setuid to " << dropUid << " failed!" << std::endl;
            return false;
        }
        std::cout << "Successfully dropped uid to " << dropUid << " and gid to " << dropGid << std::endl;
    }
    // 监听 将套接字置于监听状态，随时准备接受连接
    // 接受队列中积压的操作系统最大连接数
    if (listen(listenSocket, SOMAXCONN) != 0){
        std::cout << "Failed to put the socket in a listening state" << std::endl;
        return false;
    }
    // setup kqueue
    kqfd = kqueue();
    if (kqfd == -1){
        std::cout << "Could not create the kernel event queue!" << std::endl;
        return false;
    }
    // 让 kqueue 监视监听套接字
    updateEvent(listenSocket, EVFILT_READ, EV_ADD, 0, 0, NULL);

    canRun = true;
    std::cout << "Server ready. Listening on port " << listenPort << "..." <<std::endl;
    return true;
}

// 停止服务器
// 断开所有客户端连接，清理在 start() 中创建的所有服务器资源
void HTTPServer::stop(){
    canRun = false;
    if (listenSocket != INVALID_SOCKET){
        // 关闭所有打开的连接，并从内存中删除客户端
        for (auto& [clfd, cl] : clientMap)
            disconnectClient(cl, false);
        // clear map
        clientMap.clear();

        // 从 kqueue 中移除监听套接字
        updateEvent(listenSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        // 关闭监听套接字并将其释放给操作系统
        shutdown(listenSocket, SHUT_RDWR);
        close(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    if (kqfd != -1) {
        close(kqfd);
        kqfd = -1;
    }

    std::cout << "Server shutdown!" << std::endl;
}

// 更新事件
// 通过创建适当的 kevent 更新 kqueue
void HTTPServer::updateEvent(int ident, short filter, u_short flags, 
                            u_int fflags, int32_t data, void* udata) {
    struct kevent kev;
    EV_SET(&kev, ident, filter, flags, fflags, data, udata);
    kevent(kqfd, &kev, 1, NULL, 0, NULL);
}

// 主服务器处理函数，用于检查监听套接字上是否有新连接或要读取的数据
void HTTPServer::process(){
    int32_t nev = 0;  // kevent 返回的更改事件数

    while (canRun){
        //获取在 evList 中触发读取事件的已更改套接字描述符列表
        // 在标头中设置超时
        nev = kevent(kqfd, NULL, 0, evList, QUEUE_SIZE, &kqTimeOut);
        if (nev <= 0){
            continue;
        }
        // 只循环查看 evList 数组中发生变化的套接字
        for(int i = 0; i < nev; ++i){
            // 客户端等待连接
            if (evList[i].ident == (uint32_t)listenSocket) {
                acceptConnection();
                continue;
            }

            // 客户端描述符触发事件
            auto cl = getClient(evList[i].ident);  // 标识包含客户端套接字描述符
            if (cl == nullptr){
                std::cout << "Could not find client" << std::endl;
                // 从 kqueue 中删除套接字事件
                updateEvent(evList[i].ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                updateEvent(evList[i].ident, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

                close(evList[i].ident);
                continue;
            }

            // 客户端希望断开连接
            if (evList[i].flags & EV_EOF){
                disconnectClient(cl, true);
                continue;
            }

            if (evList[i].filter == EVFILT_READ){
                // 读取客户端请求
                readClient(cl, evList[i].data);

                // 让 kqueue 禁用 “读取 ”事件的跟踪，启用 “写入 ”事件的跟踪
                updateEvent(evList[i].ident, EVFILT_READ, EV_DISABLE, 0, 0, NULL);
                updateEvent(evList[i].ident, EVFILT_WRITE, EV_ENABLE, 0, 0, NULL);
            } else if(evList[i].filter == EVFILT_WRITE){
                if (!writeClient(cl, evList[i].data)){
                    updateEvent(evList[i].ident, EVFILT_READ, EV_ENABLE, 0, 0, NULL);
                    updateEvent(evList[i].ident, EVFILT_WRITE, EV_DISABLE, 0, 0, NULL);
                }
            }
        }
    }
}