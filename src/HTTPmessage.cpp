#include "HTTPmessage.h"

#include <string>
#include <format>
#include <iostream>
#include <memory>

HTTPMessage::HTTPMessage() : ByteBuffer(4096) {}

HTTPMessage::HTTPMessage(std::string const& sData) : ByteBuffer(sData.size() + 1) {
    putBytes((const uint8_t* const)sData.c_str(), sData.size() + 1);
}

HTTPMessage::HTTPMessage(const uint8_t* pData, uint32_t len) : ByteBuffer(pData, len){}

// put line
// 在当前位置向后的字节缓冲区添加一行（字符串）
// str 要放入字节缓冲区的字符串; crlf_end 如果为 true（默认），则以一个 \r \n）结束该行
void HTTPMessage::putLine(std::string str, bool crlf_end){
    if (crlf_end)
        str += "\r\n";
    
    putBytes((const uint8_t* const)str.c_str(), str.size());
}

// putHeaders
// 将当前在 "headrs" map 映射中的所有headers写入 ByteBuffer。
// 'Header: value'
void HTTPMessage::putHeaders(){
    for (auto const &[key, value] : headers){
        putLine(key + ": " + value, true);
    }
    // 以空行结束
    putLine();
}

// Get Line
// 读取一行的全部内容：字符串从当前位置读起，直到 CR 或 LF（以先到者为准），然后递增读取位置
// 直到超过该行最后一个 CR 或 LF, 返回字符串中该行的内容（不含 CR 或 LF）
std::string HTTPMessage::getLine(){
    std::string ret = "";
    int32_t startPos = getReadPos();
    bool newLineReached = false;
    char c=0;

    // 向返回的 tring 追加字符，直到缓冲区结束、出现 CR (13) 或 LF (10) 字符为止
    for (uint32_t i = startPos; i < size() ; ++i){
        // 如果下一个字节是 \r 或 \n，就到达了行的末尾，应该跳出循环
        c = peek();
        if ((c == 13) || (c==10)){
            newLineReached = true;
            break;
        }

        ret += getChar();
    }

    // 如果从未达到行结束符，则丢弃结果，并得出结论：没有其他行可解析了
    if (!newLineReached) {
        setReadPos(startPos);
        ret = "";
        return ret;
    }

    // 增加读取位置，直到 CR 或 LF 链结束，这样读取位置就会指向下一行
    // 另外，最多只能读取 2 个字符，这样就不会跳过一个只有 （r/n）的空行
    uint32_t k = 0;
    for (uint32_t i = getReadPos(); i < size(); ++i){
        if (k++ >= 2)
            break;
        c = getChar();
        if((c != 13) && (c != 10)){
            // 读取字符不是LF或CR则读取位置后退
            setReadPos(getReadPos() - 1);
            break;
        }
    }
    return ret;

}

// getStrElemnt
// 从当前缓冲区获取一个token，在delimiter处停止。以字符串形式返回标记
// delim 返回元素时要停止的定界符。默认为空格; return 在缓冲区中找到的token。如果未到达分隔符，则为空
std::string HTTPMessage::getStrElement(char delim){
    int32_t startPos = getReadPos();
    if (startPos < 0)
        return "";
    int32_t endPos = find(delim, startPos);
    if (endPos < 0)
        return "";
    
    if (startPos > endPos)
        return "";
    
    // 根据找到的结束位置计算尺寸
    uint32_t size = (endPos + 1) - startPos;
    if (size <= 0)
        return "";

    // 从缓冲区获取string直到分隔符
    auto str = std::make_unique<char[]>(size);
    getBytes((uint8_t*)str.get(), size);       // 从缓冲区取size长度内容放入str中
    str[size - 1] = 0x00;
    std::string ret = str.get();
    // 在分隔符之后增加读取位置
    setReadPos(endPos + 1);

    return ret;
}


