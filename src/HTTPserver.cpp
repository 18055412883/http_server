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

//  接受连接
//  当 runServer() 检测到新连接时，该函数将被调用。它会尝试接受待处理的连接，实例化一个客户端对象，并添加到客户端映射中。
void HTTPServer::acceptConnection(){
    // 使用预设地址信息设置新客户
    sockaddr_in clientAddr;
    int32_t clientAddrLen = sizeof(clientAddr);
    int32_t clfd = INVALID_SOCKET;

    // 接受待处理连接并重新获取客户描述符
    clfd = accept(listenSocket, (sockaddr*)&clientAddr,(socklen_t*)&clientAddrLen);
    if(clfd ==INVALID_SOCKET)
        return;

    // 将socket设置为非阻塞
    fcntl(clfd, F_SETFL, O_NONBLOCK);

    // 添加 kqueue 事件，以跟踪新客户端套接字的 “读取 ”和 “写入 ”事件
    updateEvent(clfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    updateEvent(clfd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, NULL);

    // 创建一个客户端对象到client map
    auto cl = std::make_unique<Client>(clfd, clientAddr);
    std::cout << "[" << cl->getClientIP() << "] connected" << std::endl;
    clientMap.try_emplace(clfd, std::move(cl));
}

// get client
// 根据clientMap中的套接字描述符编号查找客户
// 参数 clfd 客户端套接字描述符
// 如果找到，返回客户端对象指针。否则为空
std::shared_ptr<Client> HTTPServer::getClient(int clfd){
    auto it = clientMap.find(clfd);
    if (it == clientMap.end()){
        return nullptr;
    }
    return it->second;
}

// 断开客户端连接
// 关闭客户端的套接字描述符，并将其从 FD 映射、客户端映射和内存中释放出来
//  @param cl 客户端对象指针
//  当 mapErase 为 true 时，从客户端映射中删除客户端。
//  如果正在对客户端映射进行操作，而我们又不想立即删除映射条目，则需要使用该参数
void HTTPServer::disconnectClient(std::shared_ptr<Client> cl, bool mapErase){
    if (cl == nullptr){
        return;
    }
    std::cout << "[" << cl->getClientIP() << "] disconnected" << std::endl;
    // cong kqueue 中删除套接字事件
    updateEvent(cl->getSocket(), EVFILT_READ, EV_DELETE, 0, 0, NULL);
    updateEvent(cl->getSocket(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    // 关闭套接字描述符
    close(cl->getSocket());

    // 从 clientMap 中删除客户端
    if (mapErase)
        clientMap.erase(cl->getSocket());
}

// 读取客户端请求
// 从表示有数据等待的客户端接收数据。将接收到的数据传递给 handleRequest()
// 同时检测套接字状态中的任何错误
// @param cl 发送数据的客户端指针
// @param data_len 等待读取的字节数
void HTTPServer::readClient(std::shared_ptr<Client> cl, int32_t data_len){
    if (cl == nullptr){
        return;
    }
    // 如果读取过滤器触发时数据为 0 字节，客户端可能需要断开连接
    // 默认将 data_len 设置为以太网最大 MTU
    if (data_len == 0){
        data_len = 1400;
    }

    auto pData = std::make_unique<uint8_t[]>(data_len);

    int32_t flags = 0;
    ssize_t lenRecv = recv(cl->getSocket(), pData.get(), data_len, flags);
    // 确定客户端套接字的状态并采取行动
    if (lenRecv == 0){
        // client 断开连接
        std::cout << "[" << cl->getClientIP() << "] has opted to close the connection" << std::endl;
        disconnectClient(cl, true);
    }
    else if (lenRecv < 0){
        disconnectClient(cl, true);
    } else {
        // 把data放入HTTPRequest并发送给handleRequest()处理
        auto req = new HTTPRequest(pData.get(), lenRecv);
        handleRequest(cl, req);
        delete req;
    }
}

// 写入client
// 客户端表示已读写。如果发送队列中有一个项目，则向套接字写入 avail_bytes 字节数
// @param cl 发送数据的客户端指针
// @param avail_bytes 发送缓冲区中可供写入的字节数
bool HTTPServer::writeClient(std::shared_ptr<Client> cl, int32_t avail_bytes){
    if(cl == nullptr){
        return false;
    }
    
    int32_t actual_sent = 0;  // 实际发送的字节数
    int32_t attempt_sent = 0;  // 尝试发送的字节数

    if (avail_bytes >1400){
        // 限制最大发送字节数
        avail_bytes = 1400;
    }
    else if (avail_bytes == 0){
        // 有时操作系统在可以发送数据时报告为 0 - 尝试涓流数据
        // 操作系统最终会增加可用字节数
        avail_bytes = 64;
    }

    auto item = cl->nextInSendQueue();
    if (item == nullptr)
        return false;
    
    const uint8_t* const pData = item->getRawDataPointer();
    // 项目尚未发送的数据量
    int32_t remaining = item->getSize() - item->getOffset();
    bool disconnect = item->getDisconnect();

    if (avail_bytes >= remaining) {
        // 发送缓冲区大于我们的需要，其余项目可以发送
        attempt_sent = remaining;
    } else {
        // 发送缓冲区小于我们的需要，发送可用的数量
        attempt_sent = avail_bytes;
    }

    // 发送数据并按实际发送量递增偏移量
    actual_sent = send(cl->getSocket(), pData + (item->getOffset()), attempt_sent, 0);
    if (actual_sent >= 0)
        item->setOffset(item->getOffset() + actual_sent);
    else
        disconnect = true;

    // std::cout << "[" << cl->getClientIP() << "] was sent " << actual_sent << " bytes " << std::endl;

    // 不再需要 SendQueueItem。去队列和删除
    if (item->getOffset() >= item->getSize())
        cl->dequeueFromSendQueue();

    if (disconnect) {
        disconnectClient(cl, true);
        return false;
    }

    return true;
}

// 处理来自客户端的请求。将请求发送到相应的处理函数
//  对应 HTTP 操作（GET、HEAD 等)
//  @param cl 客户端对象，请求来自该对象
//  @param req HTTPRequest 对象，包含原始数据包数据

void HTTPServer::handleRequest(std::shared_ptr<Client> cl, HTTPRequest* const req){
    // 解析 request
    // 如果有错误，发送错误响应
    if (!req->parse()){
        std::cout << "[" << cl->getClientIP() << "] There was an error processing the request of type: " << req->methodIntToStr(req->getMethod()) << std::endl;
        std::cout << req->getParseError() << std::endl;
        sendErrorResponse(cl, Status(BAD_REQUEST));
        return;
    }
    std::cout << "[" << cl->getClientIP() << "] " << req->methodIntToStr(req->getMethod()) << " " << req->getRequestUri() << std::endl;

    // 发送request 到correct handler
    switch (req->getMethod()){
        case Method(HEAD):
        case Method(GET):
            handleGet(cl, req);
            break;
        case Method(OPTIONS):
            handleOptions(cl, req);
            break;
        case Method(TRACE):
            handleTrace(cl, req);
            break;
        default:
            std::cout << "[" << cl->getClientIP() << "] Could not handle or determine request of type " << req->methodIntToStr(req->getMethod()) << std::endl;
            sendStatusResponse(cl, Status(NOT_IMPLEMENTED));
            break;
    }
}

//  处理 GET 或 HEAD 请求，为客户端提供适当的响应
//  @param cl 客户端对象，请求来自该对象
//  @param req HTTPRequest 对象，包含原始数据包数据
void HTTPServer::handleGet(std::shared_ptr<Client> cl, HTTPRequest* const req){
    auto resHost = this->getResourceHostForRequest(req);

    // 无法确定资源主机或客户端指定的主机无效
    if (resHost == nullptr){
        sendStatusResponse(cl, Status(BAD_REQUEST), "Invalid/No Host specified");
        return
    }
    // 检查被请求资源是否存在
    auto uri = req->getRequestUri();
    auto r = resHost->getResource(uri);
    if (r!= nullptr){
        std::cout << "[" << cl->getClientIP() << "] " << "Sending file: " << uri << std::endl;

        auto resp = std::make_unique<HTTPResponse>();
        resp->setStatus(Status(OK));
        resp->addHeader("Content-Type", r->getMimeType());
        resp->addHeader("Content-Length", r->getSize());

        // 只有在 GET 请求时才发送信息正文
        if (req->getMethod() == Method(GET))
            resp->setData(r->getData(), r->getSize());
        
        bool dc = false;
        // HTTP/1.0 默认关闭连接
        if (req->getVersion().compare(HTTP_VERSION_10) == 0)
            dc = true;
        
        // 如果指定了连接：关闭，则应在请求处理完毕后终止连接
        if (auto con_val = req->getHeader("Connection"); con_val.compare("close")==0)
            dc = true;

        sendResponse(cl, std::move(resp), dc);

    }else{
        // 资源不存在
        std::cout << "[" << cl->getClientIP() << "] " << "File not found: " << uri << std::endl;
        sendStatusResponse(cl, Status(NOT_FOUND));
    }
}
// 处理 OPTIONS 请求
// OPTIONS 返回服务器 (*) 或特定资源允许的能力
// @param cl 请求资源的客户端
// @param req 请求状态
void HTTPServer::handleOptions(std::shared_ptr<Client> cl, [[maybe_unused]] const HTTPRequest* const req) {
    // 返回服务器的能力，而不是为每个资源计算能力
    std::string allow = "HEAD, GET, OPTIONS, TRACE";

    auto resp = std::make_unique<HTTPResponse>();
    resp->setStatus(Status(OK));
    resp->addHeader("Allow", allow);
    resp->addHeader("Content-Length", "0"); // Required

    sendResponse(cl, std::move(resp), true);
}

// 处理 TRACE 请求
// TRACE: 逐字发回服务器收到的请求
// @param cl 请求资源的客户端
// @param req 请求状态
void HTTPServer::handleTrace(std::shared_ptr<Client> cl, HTTPRequest* const req) {
    // 获取请求的字节数组表示
    uint32_t len = req->size();
    auto buf = std::make_unique<uint8_t[]>(len);
    req->setReadPos(0); //将读取位置设置在起始位置，因为请求已被读取到终点
    req->getBytes(buf.get(), len);

    // 发送以整个请求为正文的响应
    auto resp = std::make_unique<HTTPResponse>();
    resp->setStatus(Status(OK));
    resp->addHeader("Content-Type", "message/http");
    resp->addHeader("Content-Length", len);
    resp->setData(buf.get(), len);
    sendResponse(cl, std::move(resp), true);
}

// 发送状态响应
//  向客户端发送预定义的 HTTP 状态代码响应，其中只包含状态代码和所需的标头，然后断开客户端连接
//  @param cl 要向其发送状态代码的客户端 与 HTTPMessage.h 中的枚举相对应的状态代码
//  @param msg 附加到正文的额外信息
void HTTPServer::sendStatusResponse(std::shared_ptr<client> cl, int32_t status, const std::string const& msg){
    auto resp = std::make_unique<HTTPResponse>();
    resp->setStatus(status);

    // body: reason string + additional msg
    std::string body = resp->getReason();
    if (msg.length() > 0 )
        body += ": " + msg;
    
    uint32_t slen = body.length();
    auto sdata = new uint8_t[slen];
    memset(sdata, 0x00, slen);
    strncpy((char*)sdata, body.c_str(), slen);

    resp->addHeader("Content-Type", "text/plain");
    resp->addHeader("Content-Length", slen);
    resp->setData(sdata, slen);

    sendResponse(cl, std::move(resp), true);
}

// 发送 response
// 向特定客户端发送通用 HTTPResponse 数据包
//  * @param Cl 待发送的客户端
//  * @param buf 含有待发送数据的字节缓冲区
//  * @param disconnect 服务器是否应在发送后断开与客户端的连接（可选，默认 = false）
void HTTPServer::sendResponse(std::shared_ptr<Client> cl, std::unique_ptr<HTTPResponse> resp, bool disconnect){
    resp->addHeader("Server", "httpserver/1.0");

    // 使用日期标头对响应进行时间标记
    std::string tstr;
    char tbuf[36] = {0};
    time_t rawtime;
    struct tm ptm = {0};
    time(&rawtime);
    if (gmtime_r(&rawtime, &ptm) != nullptr){
        strftime(tbuf, 36, "%a, %d %b %Y %H:%M:%S GMT", &ptm);
        tstr = tbuf;
        resp->addHeader("Date", tstr);
    }
    // 如果这是服务器发送的最终响应，则包含 Connection: close 头信息
    if (disconnect){
        resp->addHeader("Connection", "close");
    }

    // 通过创建响应获取原始数据（我们负责在 process() 中对其进行清理）
    // 将数据添加到客户端的发送队列中
    cl->addToSendQueue(new SendQueueItem(resp->create(), resp->size(), disconnect));
}

// 获取资源主机
//  根据请求的路径 检索 适当的 ResourceHost 实例 
//  @param req 请求状态
std::shared_ptr<ResourceHost> HTTPServer::getResourceHostForRequest(const HTTPRequest* const req){
    // 确定合适的虚拟主机
    std::string host = "";
    //  读取请求中指定的主机（符合 HTTP/1.1 要求）
    if (req->getVersion().compare(HTTP_VERSION_11) == 0){
        host = req->getHeaderValue("Host");
        // 所有虚拟主机都附加了端口，因此如果不存在端口，则需要将其附加到主机上
        if (!host.contains(":")){
            host.append(":" + listenPort);
        }
        auto it = vhosts.find(host);

        if (it != vhosts.end()){
            return it->second;
        }
    } else {
        // Temporary： HTTP/1.0 会给出 hostList 中的第一个资源主机
        // TODO: 允许管理员指定 "默认资源主机
        if (!hostList.empty())
            return hostList[0];
    }
    return nullptr;
}
