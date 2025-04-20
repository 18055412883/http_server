#ifndef _HTTPMESSAGE_H_
#define _HTTPMESSAGE_H_

#include<map>
#include<memory>
#include<string>

#include"ByteBuffer.h"
// #include <string_view>

const std::string HTTP_VERSION_10 = "HTTP/1.0";
const std::string HTTP_VERSION_11 = "HTTP/1.1";
const std::string DEFAULT_HTTP_VERSION = HTTP_VERSION_11;
constexpr uint32_t NUM_METHODS = 9;
constexpr uint32_t INVALID_METHOD = 9999;
static_assert(NUM_METHODS < INVALID_METHOD, "INVALID_METHOD must be greater than NUM_METHODS");

// HTTP Methods(Requests)

enum Method{
    HEAD = 0,
    GET = 1,
    POST = 2,
    PUT = 3,
    DEL = 4,
    TRACE = 5,
    OPTIONS = 6,
    CONNECT = 7,
    PATCH = 8
};

const static char* const requestMethodStr[NUM_METHODS] = {
    "HEAD",  // 0
    "GET",  //1,
    "POST",  // 2,
    "PUT",  // 3,
    "DEL",  // 4,
    "TRACE",  // 5,
    "OPTIONS",  // 6,
    "CONNECT",  // 7,
    "PATCH"  // 8
};

enum Status {
    CONTINUE = 100,
    
    OK = 200,
    
    BAD_REQUEST = 400,
    NOT_FOUND = 404,

    SERVER_ERROR = 500,
    NOT_IMPLENTED = 501
};

class HTTPMessage : public ByteBuffer{
private:
    std::map<std::string, std::string, std::less<>> headers;  // less<> 是一个函数对象（函数指针），用于提供 默认的排序准则。
    // 它定义了元素的顺序——即它会比较两个元素的大小关系并返回布尔值。 
    // std::less<key> 默认使用 升序排序，并且对于 std::string，它会根据字典顺序进行排序（即按字母升序排列字符串）

public:
    std::string parseErrorStr = "";
    std::string version = DEFAULT_HTTP_VERSION;
    uint8_t* data = nullptr;
    uint32_t dataLen = 0;

    HTTPMessage();
    explicit HTTPMessage(std::string const& sData);
    explicit HTTPMessage(const uint8_t* pData, uint32_t len);
    ~HTTPMessage() override = default;
    
    virtual std::unique_ptr<uint8_t[]> create() = 0;  //纯虚函数
    virtual bool parse() = 0;

    // create
    void putLine(std::string str = "", bool crlf_end = true);
    void putHeaders();

    // parse
    std::string getLine();
    std::string getStrElement(char delim =0x20);  // 0x20 = "space"
    void parseHeaders();
    bool parseBody();

    // header map manipulation
    void addHeader(std::string const& line);
    void addHeader(std::string const& key, std::string const& value);
    void addHeader(std::string const& key, int32_t value);
    std::string getHeaderValue(std::string const& key) const;
    std::string getHeaderStr(int32_t index) const;
    uint32_t getNumHeaders() const;
    void clearHeaders();

    std::string getParseError() const{
        return parseErrorStr;
    }

    void setVersion(std::string_view v){
        version = v;
    }

    std::string getVersion() const{
        return version;
    }

    void setData(uint8_t* d, uint32_t len){
        data = d;
        dataLen = len;
    }
    uint8_t* getData(){
        return data;
    }

    uint32_t getDataLength() const{
        return dataLen;
    }

};

#endif