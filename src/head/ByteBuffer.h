#ifndef _BYTEBUFFER_H_
#define _BYTEBUFFER_H_
#endif

#include<cstdint>
#include<vector>
#include<memory>

#ifdef BB_UTILITY
#include<string>
#endif

constexpr uint32_t BB_DEFAULT_SIZE = 4096;  //constexpr是修饰一个常量表达式

#ifdef BB_USE_NS
namespace bb{


class ByteBuffer{
private:
    uint32_t rpos = 0;
    uint32_t wpos = 0;
    std::vector<uint8_t> buf;  // 缓冲区

#ifdef BB_UTILITY
    std::string name="";
#endif

    template<typename T>
    T read(){
        T data = read<T>(rpos);
        rpos += sizeof(T);
        return data;
    }

    template<typename T>
    T read(uint32_t index) const {  //  从缓冲区 中 index位置读取
        if (index + sizeof(T) <= buf.size())
            return *((T* const) &buf[index]);   // (T* const)&buf[index] 强行将 指向 buf[index]的指针强制转换成类型T的指针，然后解引用读取T类型数据

        return 0;
    }

    template<typename T> 
    void append(T data){
        uint32_t s = sizeof(data);

        if(size() < (wpos + s))
            buf.resize(wpos + s);  // 缓冲区大小重分配
        memcpy(&buf[wpos], (uint8_t*)&data, s);

        wpos += s;
    }

    template<typename T> 
    void insert(T data, uint32_t index){
        if ((index + sizeof(data)) > size()){
            buf.resize(size() + (index + sizeof(data)));  // 不能使用reserve 
            // 虽然 buf 会有更多的内存空间可用，memcpy 仍然无法写入数据，因为缓冲区的实际大小没有被更新，可能导致越界访问
        }

        memcpy(&buf[index], (uint8_t*)& data, sizeof(data));
        wpos = index + sizeof(data);
    }

public:
    explicit ByteBuffer(uint32_t size = BB_DEFAULT_SIZE);
    explicit ByteBuffer(const uint8_t* arr, uint32_t size);
    virtual ~ByteBuffer() = default;
    
    uint32_t bytesReamining() const;  // 从当前读取位置到缓冲区结束的字节数
    void clear();  // 清除vector并重置读写位置
    std::unique_ptr<ByteBuffer> clone();  // 返回contents和state(rpos、wpos)完全相同的 ByteBuffer 新实例
    bool equals(const ByteBuffer* other) const;  // 比较contents是否相同
    void resize(uint32_t newSize);
    uint32_t size() const;  // 内部vector的大小

    // Basic Searching (Linear)
    template<typename T> 
    int32_t find(T key, uint32_t start=0){  
        int32_t ret = -1;
        uint32_t len = buf.size()
        for(uint32_t i = start; i < len; ++i){
            T data = read<T>(i);
            if((key != 0) && (data == 0))  // 没有找到，超出了缓冲区的边界
                break;
            if(data == key){
                ret = i;
                break;
            }
        }
        return ret;
    }

    void replace(uint8_t key, uint8_t rep, uint32_t start = 0, bool firstOccurrenceOnly=false);

    //Read
    uint8_t peek() const;  // 从当前位置读取并返回缓冲区中的下一个字节，但不递增读取位置
    uint8_t get(); // 读取缓冲区当前位置的字节，然后将位置递增
    uint8_t get(uint32_t index) const;  // 读取索引处的字节
    void getBytes(uint8_t* const out_buf, uint32_t out_len);  // 读取长度为 len 的数组 buf
    char getChar();
    char getChar(uint32_t index) const;

    double getDouble();
    double getDouble(uint32_t index) const;
    float getFloat();
    float getFloat(uint32_t index) const;

    uint32_t getInt();
    uint32_t getInt(uint32_t index) const;
    uint64_t getLong();
    uint64_t getLong(uint32_t index) const;
    uint16_t getShort();
    uint16_t getShort(uint32_t index) const;

    // Write
    void put(const ByteBuffer* src);  // 写入另一个字节缓冲区 (src) 的全部内容
    void put(uint8_t b);
    void put(uint8_t b, uint32_t index);  // 根据索引写入
    void putBytes(const uint8_t* const b, uint32_t len);
    void putBytes(const uint8_t* const b, uint32_t len, uint32_t index);
    void putChar(char value);
    void putChar(char value, uint32_t index);
    void putDouble(double value);
    void putDouble(double value, uint32_t index);
    void putFloat(float value);
    void putFloat(float value, uint32_t index);
    void putInt(uint32_t value);
    void putInt(uint32_t value, uint32_t index);
    void putLong(uint64_t value);
    void putLong(uint64_t value, uint32_t index);
    void putShort(uint16_t value);
    void putShort(uint16_t value, uint32_t index);

    // buf position
    void setReadPos(uint32_t r){
        rpos = r;
    }

    uint32_t getReadPos() const {
        return rpos;
    }

    void setWritePos(uint32_t w){
        wpos = w;
    }

    uint32_t getWritePos() const {
        return wpos;
    }

#ifdef BB_UTILITY
    void setName(std::string_veiw n);
    std::string getName() const;
    void printInfo() const;
    void printAH() const;
    void printAscii() const;
    void printHex() const;
    void printPosition() const;
#endif

};
#ifdef BB_USE_NS
} 
#endif


