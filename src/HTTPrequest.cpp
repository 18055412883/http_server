#include "HTTPmessage.h"
#include "HTTPrequest.h"

#include<iostream>
#include<memory>

HTTPRequest::HTTPRequest() : HTTPMessage(){}

HTTPRequest::HTTPRequest(std::string const& sData) : HTTPMessage(sData){}

HTTPRequest::HTTPRequest(const uint8_t* pData, uint32_t len) : HTTPMessage(pData, len){}

// 获取方法名称并将其转换为相应的方法
// id 详细描述于method enum
// @param name 方法的字符串表示
// 返回相应的方法 ID，如果找不到方法，则返回 INVALID_METHOD
uint32_t HTTPRequest::methodStrToInt(std::string_view name) const {
    // 方法名称必须在 1 到 10 个字符之间。任何超出这些范围的内容都不应进行比较
    if (name.empty() || (name.size() >= 10))
        return INVALID_METHOD;
    
    // 循环查看 requestMethodStr 数组，并尝试将 name与数组中的已知方法进行匹配
    uint32_t ret = INVALID_METHOD;
    for(uint32_t i = 0; i < NUM_METHODS; ++i){
        if(name.compare(requestMethodStr[i]) == 0){
            ret = i;
            break;
        }
    }
    return ret;
}

// 获取method enum中的方法 ID 并返回相应的 std::string 表示
// param: mid 要查找的方法 ID
// return string 表示的方法名称。如果无法找到方法，则返回空白
std::string HTTPRequest::methodIntToStr(uint32_t mid) const{
    // ID 超出了可能的 requestMethodStr 索引范围
    if (mid >= NUM_METHODS)
        return "";
    
    // 返回与 id 匹配的 string
    return requestMethodStr[mid];
}

// 创建并返回 HTTP 请求的字节数组，该数组由 HTTPRequest 的变量构建而成。
// return 通过网络发送 的HTTPRequest 字节数组，
std::unique_ptr<uint8_t[]> HTTPRequest::create(){
    // 如果不是第一次调用 create() 时，清空字节缓冲区
    clear();
    // 插入初始行: <method> <path> <version>\r\n
    std::string mstr = "";
    mstr = methodIntToStr(method);
    if (mstr.empty()){
        std::cout << "Could not create HTTPRequest, unknown method id: "<< method << std::endl;
        return nullptr;
    }
    putLine(mstr + " " + requestUri + " " + version);
    // 放入所有的headers
    putHeaders();
    // 如果有正文数据，加入
    if ((data != nullptr) && dataLen > 0){
        putBytes(data, dataLen);
    }

    // 为返回的字节数组分配空间并返回
    auto sz = size();
    auto createRetData = std::make_unique<uint8_t[]>(sz);
    setReadPos(0);
    getBytes(createRetData.get(), sz);
    return createRetData;
}

// 通过解析 HTTP 数据填充 HTTPRequest 内部变量
// 如果成功，则参数为 True。如果为 false，则设置 parseErrorStr 以说明失败原因
bool HTTPRequest::parse(){
    // 从初始行<method> <path> <version>\r\n 获取元素
    std::string methodName = getStrElement();
    if (methodName.empty()) {
        parseErrorStr = "Empty method";
        return false;
    }
    // 将名称转换为内部枚举编号
    method = methodStrToInt(methodName);
    if (method == INVALID_METHOD){
        parseErrorStr = "Invalid Method";
        return false;
    }

    requestUri = getStrElement();
    if (requestUri.empty()){
        parseErrorStr = "No request URI";
        return false;
    }

    version = getLine();   // 行结束
    if (version.empty()) {
        parseErrorStr = "HTTP version string was empty";
        return false;
    }
    std::string httpv = "HTTP/1";
    if (version.substr(0, httpv.size()) != httpv){
        parseErrorStr = "HTTP version was invalid";
        return false;
    }

    // 使用 parseHeaders 辅助程序解析并填充headers map
    parseHeaders();

    // 只有 POST 和 PUT 可以包含content（headers后的数据）
    if((method != POST) && (method != PUT))
        return true;

    // 解析body信息
    if(!parseBody())
        return false;

    return true;
}


