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
    getBytes((uint8_t*)str.get(), size);       // 从缓冲区取size长度内容放入str中， unique_ptr的get方法返回一个该字符数组的原始指针这里是char*
    str[size - 1] = 0x00;                      // 字符数组的最后一个字节设置为 0x00，也就是 null 终止符。这样，字符串在内存中就变成了一个 C 风格的字符串（以 null 结尾），这是 std::string 所需要的格式。
    std::string ret = str.get();               // 返回的是一个指向字符数组的原始指针，它被传递给 std::string 构造函数。由于字符数组以 null 结尾，std::string 会自动从这个原始指针构造一个字符串对象，并存储在 ret 中。
    // 在分隔符之后增加读取位置
    setReadPos(endPos + 1);

    return ret;
}

//  parse headers
//  当 HTTP 消息（请求和响应）到达出现头信息的位置时，应调用此方法. 应调用该方法来解析并填充头信息的内部映射
//  解析头信息会将读取位置移到头信息结束时的空行之后。

void HTTPMessage::parseHeaders(){
    std::string hline = "";
    std::string app = "";

    //  获取第一个header
    hline = getLine();

    // 继续提取headers，直到出现空行（标头结束）为止
    while (hline.size() > 0){
        //  以逗号结尾的多行数值的情况
        app = hline;
        while (app[app.size() - 1] == ',')
        {
            app = getLine();
            hline += app;
        }
        addHeader(hline);
        hline = getLine();
    }
}

//  parse body
//  解析 HTTP 消息headers部分之后的所有内容。处理分块响应/请求
//  成功时返回 True。错误时为 False，parseErrorStr 设置原因
bool HTTPMessage::parseBody() {
    //  如果有正文数据，则应存在 Content-Length（正文数据的大小）。
    std::string hlenstr = "";
    uint32_t contentLen = 0;
    hlenstr = getHeaderValue("content-Length");   // 返回header map中对应的value
    // no body data to read
    if (hlenstr.empty())
        return true;
    
    contentLen = atoi(hlenstr.c_str());  // 字符串转整型， c_str()返回指向 C 风格字符串（即以 null 结尾的字符数组）的常量指针。
    
    // contentLen 不应超过缓冲区的剩余字节数
    // 在 bytesRemaining 中加 1，以便包含当前读取位置的字节
    if (contentLen > bytesRemaining() + 1){
         // 如果超过，则存在潜在的安全问题，我们无法可靠地解析
        //  dataLen = bytesRemaining();
        parseErrorStr = "content-length(" + hlenstr +") is greater than remaining bytes(" + std::to_string(bytesRemaining()) +")";
        return false;
    }
    else if(contentLen == 0){
        //  没有可以读取的
        return true;
    }
    else {
        // 否则，我们可以相信 Content-Length 是有效的，并读取指定的字节数
        dataLen = contentLen;
    }

    // 创建足够大的buffer存储data
    uint32_t dIdx = 0;
    uint32_t s = size();
    if(s > dataLen){
        parseErrorStr = "ByteBuffer size of " + std::to_string(s) + " is greater than dataLen " + std::to_string(dataLen);
        return false;
    }

    data = new uint8_t[dataLen];
    // 抓取从当前位置到末尾的所有字节
    for(uint32_t i = getReadPos(); i<s;++i){
        data[dIdx] = get(i);
        dIdx++;
    }

    // 可以在这里处理分块的请求/响应解析（带页脚），但此项目不这样做。
    return true;
}

// 从字符串向映射添加header
// 获取格式化的标题字符串 "Header: value"，对其进行解析，并将其作为键值对放入 std::map 中。
// param string包含格式化的header：value
void HTTPMessage::addHeader(std::string const& line){
    size_t kpos = line.find(':');
    if (kpos == std::string::npos) {
        std::cout<< "Could not addHeader:" << line << std::endl;
        return;
    }
    // 拒绝长度超过 32 个字符的 HTTP 标头密钥
    if (kpos > 32)
        return;
    std::string key = line.substr(0, kpos);
    if (key.empty())
        return;
    int32_t value_len = line.size() - kpos - 1;
    if(value_len <= 0)
        return;

    // 拒绝超过 4kb 的 HTTP header
    if (value_len > 4096)
        return;
    
    std::string value = line.substr(kpos + 1, value_len);

    // 跳过数值中所有前导空格
    int32_t i = 0;
    while (i < value.size() && value.at(i) == 0x20)  // at() 安全地访问字符串 中指定位置的字符， 索引超出范围则抛出异常
        ++i;
    
    value = value.substr(i, value.size());
    if (value.empty())
        return;
    
    // 将header添加进map
    addHeader(key,value);
}

// 向map添加header 键值对
void HTTPMessage::addHeader(std::string const& key, std::string const& value){
    headers.try_emplace(key, value);   // 键不存在 的情况下才“原地”构造并插入元素；若键已存在，则什么都不做。
}

// 向map中添加header键值对
// 整型转为字符串
void HTTPMessage::addHeader(std::string const& key, int32_t value){
    headers.try_emplace(key, std::to_string(value));
}

// 获取header value
// 给定header key，返回header map中对应的value
std::string HTTPMessage::getHeaderValue(std::string const& key) const{
    char c = 0;
    std::string key_lower = "";

    auto it = headers.find(key);
    // 键未找到，则尝试全小写的变体，因为有些客户端并不总是使用正确的大写字母
    if (it == headers.end()){
        for(uint32_t i = 0; i<key.length(); ++i){
            c = key.at(i);
            key_lower += tolower(c);
        }
        // 如果仍未找到，则返回空字符串，表示头值不存在
        it = headers.find(key_lower);
        if (it == headers.end())
            return "";
    }

    return it->second;
}

// get header string
// 从索引位置的header map 中获取格式完整的header:value string
std::string HTTPMessage::getHeaderStr(int32_t index) const{
    int32_t i = 0;
    std::string ret = "";
    for (auto const &[key, value] : headers){
        if (i == index){
            ret = key+": "+ value;
            break;
        }
        ++i;
    }
    return ret;
}

// get number of headers
uint32_t HTTPMessage::getNumHeaders() const {
    return headers.size();
}

//clear headers
void HTTPMessage::clearHeaders() {
    headers.clear();
}



