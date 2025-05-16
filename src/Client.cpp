#include "Client.h"
Client::Client(int fd, sockaddr_in addr) : socketDesc(fd), clientAddr(addr){
}

Client::~Client(){
    clearSendQueue();
}

// 添加到send queue
void Client::addToSendQueue(SendQueueItem* item){
    sendQueue.push(item);
}

// 返回发送队列中当前 SendQueueItem 的数量
uint32_t Client::sendQueueSize() const {
    return sendQueue.size();
}

// 返回要发送给客户端的当前 SendQueueItem 对象
SendQueueItem* Client::nextInSendQueue(){
    if (sendQueue.empty())
        return nullptr;
    return sendQueue.front();
}

// 删除并dequeues队列中的第一个项目
void Client::dequeueFromSendQueue(){
    SendQueueItem* item = nextInSendQueue();
    if (item != nullptr){
        sendQueue.pop();
        delete item;
    }
}

void Client::clearSendQueue(){
    while(!sendQueue.empty()){
        delete sendQueue.front();
        sendQueue.pop();
    }
}
