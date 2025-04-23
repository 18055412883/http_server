#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "SendQueueItem.h"

// #include <netinet/in.h>
// #include <arpa/inet.h>
#include <winsock2.h>
#include <windows.h>
#include <queue>

class Client{
    int32_t socketDesc;
    sockaddr_in clientAddr;
    std::queue<SendQueueItem*> sendQueue;

public:
    Client(int fd, sockaddr_in addr);
    ~Client();
    Client& operator=(Client const&) = delete;  // 禁用
    Client(Client &&) = delete;
    Client& operator=(Client &&) = delete;      // 移动构造

    sockaddr_in getClientAddr() const{
        return clientAddr;
    }

    int32_t getSocket() const {
        return socketDesc;
    }

    char* getClientIP(){
        return inet_ntoa(clientAddr.sin_addr);
    }

    void addToSendQueue(SendQueueItem* item);
    uint32_t sendQueueSize() const;
    SendQueueItem* nextInSendQueue();
    void dequeueFromSendQueue();
    void clearSendQueue();

};

#endif