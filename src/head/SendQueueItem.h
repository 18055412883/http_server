#ifndef _SENDQUEUEITEM_H_
#define _SENDQUEUEITEM_H_

#include<cstdint>
#include<memory>

// Object 代表  客户端发送队列中的一段数据
// 包含一个指向发送缓冲区的指针，并跟踪当前发送的数据量（通过offset）。

class SendQueueItem{
private:
    std::unique_ptr<uint8_t[]> sendData;
    uint32_t sendSize;
    uint32_t sendOffset = 0;
    bool disconnect;   //  flag，指示是否应在此项目重新排队后断开客户端连接

public:
    SendQueueItem(std::unique_ptr<uint8_t[]> data, uint32_t size, bool dc) :sendData(std::move(data)), sendSize(size), disconnect(dc){}
    ~SendQueueItem() = default;
    SendQueueItem(SendQueueItem const&) = delete;  // 禁用拷贝构造
    SendQueueItem& operator=(SendQueueItem const&) = delete;
    SendQueueItem(SendQueueItem &&) = delete;  //  禁用移动构造
    SendQueueItem& operator=(SendQueueItem &&) = delete;

    void setOffset(uint32_t off){
        sendOffset = off;
    }

    uint8_t* getRawDataPointer() const {
        return sendData.get();
    }

    uint32_t getSize() const{
        return sendSize;
    }

    bool getDisconnect() const{
        return disconnect;
    }

    uint32_t getOffset() const {
        return sendOffset;
    }

};

#endif