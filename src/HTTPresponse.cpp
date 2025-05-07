#include "HTTPmessage.h"
#include "HTTPresponse.h"

#include <string>
#include <memory>

HTTPResponse::HTTPResponse() : HTTPMessage(){}

HTTPResponse::HTTPResponse(std::string const& sDate) :HTTPMessage(sDate){}

HTTPResponse::HTTPResponse(const uint8_t* pData, uint32_t len) : HTTPMessage(pData, len){}

// 根据解析的响应原因字符串 确定 状态代码
// 原因字符串是非标准的，因此需要更改此方法，以便处理
// 带有不同类型字符串的响应

void HTTPResponse::determineStatusCode(){
    if(reason.find("Continue") != std::string::npos){
        status = Status(CONTINUE);
    }else if (reason.find("OK") != std::string::npos){
        status = Status(OK);
    }else if (reason.find("Bad Request") != std::string::npos){
        status = Status(BAD_REQUEST);
    }else if(reason.find("NOT_FOUND") != std::string::npos){
        status = Status(NOT_FOUND);
    }else if (reason.find("Server Error") != std::string::npos){
        status = Status(SERVER_ERROR);
    }else if (reason.find("Not Implemented")){
        status = Status(NOT_IMPLEMENTED);
    }else {
        status = Status(NOT_IMPLEMENTED);
    }
}

// 根据响应的状态代码确定原因字符串
void HTTPResponse::determineReasonStr() {
    switch (status) {
    case Status(CONTINUE):
        reason = "Continue";
        break;
    case Status(OK):
        reason = "OK";
        break;
    case Status(BAD_REQUEST):
        reason = "Bad Request";
        break;
    case Status(NOT_FOUND):
        reason = "Not Found";
        break;
    case Status(SERVER_ERROR):
        reason = "Internal Server Error";
        break;
    case Status(NOT_IMPLEMENTED):
        reason = "Not Implemented";
        break;
    default:
        break;
    }
}

// 创建并返回 HTTP 响应的字节数组，该数组由 HTTPResponse 的变量构建而成
// 调用者将负责清理返回的字节数组
// @return HTTPResponse 的字节数组将通过网络发送
std::unique_ptr<uint8_t[]> HTTPResponse::create(){
    // 当不是第一次调用 create() 时，清空字节缓冲区
    clear();
    // 插入状态行： <version> <status code> <reason>\r\n
    putLine(version + " " + std::to_string(status) + " " + reason);

    putHeaders();

    // 如果有正文数据，立即添加
    if ((data != nullptr) && dataLen > 0){
        putBytes(data, dataLen);
    }

    // 为返回的字节数组分配空间并返回
    auto createRetData = std::make_unique<uint8_t[]>(size());
    setReadPos(0);
    getBytes(createRetData.get(), size());
    return createRetData;
}

// 通过解析 HTTP 数据来填充 HTTPResponse 内部变量
// 如果成功，则参数为 True。如果为 false，则设置 parseErrorStr 以说明失败原因
bool HTTPResponse::parse() {
    std::string statusstr;
    // 从状态行中获取元素： <version> <status code> <reason>\r\n
    version = getStrElement();
    statusstr = getStrElement();
    determineStatusCode();
    reason = getLine();

    // 使用 parseHeaders 辅助程序解析并填充标头图
    parseHeaders();

    if (!parseBody())
        return false;

    return true;
}

