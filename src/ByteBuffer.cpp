#include "ByteBuffer.h"

#ifdef BB_UTILITY
#include <iomanip>
#include <iostream>
#include <string>
#endif

#ifdef BB_USE_
namespace bb{
#endif

    // 字节缓冲区构造函数
    // 在内部向量中保留指定大小, 默认值在 BB_DEFAULT_SIZE 中设置
    ByteBuffer::ByteBuffer(uint32_t size){
        buf.reserve(size);
        clear();
    }

    ByteBuffer::ByteBuffer(const uint8_t* arr, uint32_t size){
        // 如果提供的数组为 NULL，则分配一个与提供的大小相同的空白缓冲区
        if (arr==nullptr){
            buf.reserve(size);
            clear();
        }
        else{  // 将提供的数组放入
            buf.reserve(size);
            clear();
            putBytes(arr, size);
        }
    }

    // 清除内部矢量中的所有数据（保留原始预分配大小），将位置重置为 0
    void ByteBuffer::clear(){
        rpos = 0;
        wpos = 0;
        buf.clear();
    }

    // 在堆上分配字节缓冲区的精确副本，并返回一个指针
    // return 新克隆 ByteBuffer 的指针。如果没有更多可用内存，则为 NULL
    std::unique_ptr<ByteBuffer> ByteBuffer::clone(){
        auto ret = std::make_unique<ByteBuffer>(buf.size());
        
        for(uint32_t i = 0; i < buf.size(); ++i) {
            ret->put(get(i));
        }

        ret->setReadPos(0);
        ret->setWritePos(0);

        return ret;
    }

    // equals
    // 查看内部缓冲区中的每个字节并确保它们相同，将此字节缓冲区与另一个字节缓冲区进行比较
    // @param other 一个指向 ByteBuffer 的指针，用于与此 ByteBuffer 进行比较
    // return 如果内部缓冲区匹配，则返回 True。否则为假
    bool ByteBuffer::equals(const ByteBuffer* other) const {
        // 先比较size
        if(size() != other->size())
            return false;

        // byte by byte
        uint32_t len = size();
        for(uint32_t i = 0; i<len;++i){
            if(get(i) != other->get(i));
                return false;
        }
        return true;
    }

    // resize
    // 为大小为 newSize 的内部缓冲区重新分配内存。读写位置也将重置
    // newSize 为要分配的内存大小
    void ByteBuffer::resize(uint32_t newSize){
        buf.resize(newSize);
        rpos = 0;
        wpos = 0;
    }

    // size
    // 返回内部缓冲区的大小
    uint32_t ByteBuffer::size() const {
        return buf.size();
    }

    // Replace
    // 用字节 rep 替换出现的特定字节 key
    // @param key 被替换的字节; rep 用字节 rep 替换找到的键; start 从索引开始。默认情况下，start 为 0
    // firstOccurrenceOnly 如果为 true，只替换键的第一次出现。如果为 false，则替换所有出现的密钥。默认为false
    void ByteBuffer::replace(uint8_t key, uint8_t rep, uint32_t start, bool firstOccurrenceOnly){
        uint32_t len = buf.size();
        for (uint32_t i = start; i < len; ++i){
            uint8_t data = read<uint8_t>(i);  // 读取缓冲区i位置
            if((key!=0) && (data == 0))
                break;
            if(data == key){
                buf[i] = rep;
                if(firstOccurrenceOnly)
                    return;
            }

        }
    }

    // Read 
    uint8_t ByteBuffer::peek() const{
        return read<uint8_t>(rpos);
    }

    uint8_t ByteBuffer::get() {
        return read<uint8_t>();
    }

    uint8_t ByteBuffer::get(uint32_t index) const{
        return read<uint8_t>(index);
    }


    void ByteBuffer::getBytes(uint8_t* const out_buf, uint32_t out_len) {
        for (uint32_t i = 0; i < out_len; i++) {
            out_buf[i] = read<uint8_t>();
        }
    }

    char ByteBuffer::getChar() {
        return read<char>();
    }

    char ByteBuffer::getChar(uint32_t index) const {
        return read<char>(index);
    }

    double ByteBuffer::getDouble() {
        return read<double>();
    }

    double ByteBuffer::getDouble(uint32_t index) const {
        return read<double>(index);
    }

    float ByteBuffer::getFloat() {
        return read<float>();
    }

    float ByteBuffer::getFloat(uint32_t index) const {
        return read<float>(index);
    }

    uint32_t ByteBuffer::getInt() {
        return read<uint32_t>();
    }

    uint32_t ByteBuffer::getInt(uint32_t index) const {
        return read<uint32_t>(index);
    }

    uint64_t ByteBuffer::getLong() {
        return read<uint64_t>();
    }

    uint64_t ByteBuffer::getLong(uint32_t index) const {
        return read<uint64_t>(index);
    }

    uint16_t ByteBuffer::getShort() {
        return read<uint16_t>();
    }

    uint16_t ByteBuffer::getShort(uint32_t index) const {
        return read<uint16_t>(index);
    }

    // write
    void ByteBuffer::put(const ByteBuffer* src) {  // 写入字符串
        uint32_t len = src->size();
        for (uint32_t i = 0; i < len; i++)
            append<uint8_t>(src->get(i));
    }

    void ByteBuffer::put(uint8_t b) {
        append<uint8_t>(b);
    }

    void ByteBuffer::put(uint8_t b, uint32_t index) {
        insert<uint8_t>(b, index);
    }

    void ByteBuffer::putBytes(const uint8_t* const b, uint32_t len, uint32_t index) {
        wpos = index;

        // 将数据一个字节一个字节地插入 i+ starting 位置的内部缓冲区
        for (uint32_t i = 0; i < len; i++)
            append<uint8_t>(b[i]);
    }

    void ByteBuffer::putChar(char value) {
        append<char>(value);
    }

    void ByteBuffer::putChar(char value, uint32_t index) {
        insert<char>(value, index);
    }

    void ByteBuffer::putDouble(double value) {
        append<double>(value);
    }

    void ByteBuffer::putDouble(double value, uint32_t index) {
        insert<double>(value, index);
    }
    void ByteBuffer::putFloat(float value) {
        append<float>(value);
    }

    void ByteBuffer::putFloat(float value, uint32_t index) {
        insert<float>(value, index);
    }

    void ByteBuffer::putInt(uint32_t value) {
        append<uint32_t>(value);
    }

    void ByteBuffer::putInt(uint32_t value, uint32_t index) {
        insert<uint32_t>(value, index);
    }

    void ByteBuffer::putLong(uint64_t value) {
        append<uint64_t>(value);
    }

    void ByteBuffer::putLong(uint64_t value, uint32_t index) {
        insert<uint64_t>(value, index);
    }

    void ByteBuffer::putShort(uint16_t value) {
        append<uint16_t>(value);
    }

    void ByteBuffer::putShort(uint16_t value, uint32_t index) {
        insert<uint16_t>(value, index);
    }

    // Utility
#ifdef BB_UTILITY
    void ByteBuffer::setName(std::string_view n){
        name = n;
    }
    std::string ByteBuffer::getName() const {
        return name;
    }

    void ByteBuffer::printInfo() const {
        uint32_t length = buf.size();
        std::cout << "ByteBuffer " << name << " Length: " << length << ". Info Print" << std::endl;
    }

    void ByteBuffer::printAH() const {
        uint32_t length = buf.size();
        std::cout << "ByteBuffer " << name << " Length: " << length << ". ASCII & Hex Print" << std::endl;
        for (uint32_t i = 0; i < length; i++) {
            std::cout << "0x" << std::setw(2) << std::setfill('0') << std::hex << int(buf[i]) << " ";
        }
        std::printf("\n");
        for (uint32_t i = 0; i < length; i++) {
            std::cout << (char)buf[i] << " ";
        }
        std::cout << std::endl;
    }

    void ByteBuffer::printAscii() const {
        uint32_t length = buf.size();
        std::cout << "ByteBuffer " << name << " Length: " << length << ". ASCII Print" << std::endl;
        for (uint32_t i = 0; i < length; i++) {
            std::cout << (char)buf[i] << " ";
        }
        std::cout << std::endl;
    }

    void ByteBuffer::printHex() const {
        uint32_t length = buf.size();
        std::cout << "ByteBuffer " << name << " Length: " << length << ". Hex Print" << std::endl;
        for (uint32_t i = 0; i < length; i++) {
            std::cout << "0x" << std::setw(2) << std::setfill('0') << std::hex << int(buf[i]) << " ";
        }
        std::cout << std::endl;
    }

    void ByteBuffer::printPosition() const {
        uint32_t length = buf.size();
        std::cout << "ByteBuffer " << name << " Length: " << length << " Read Pos: " << rpos << ". Write Pos: "
                << wpos << std::endl;
    }



#endif
#ifdef BB_USE_NS
}
#endif


