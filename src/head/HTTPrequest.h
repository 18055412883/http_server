#ifndef _HTTPREQUEST_H_
#define _HTTPREQUEST_H_

#include "HTTPmessage.h"

class HTTPRequest final: public HTTPMessage{
private:
    uint32_t method = 0;
    std::string requestUri = "";

public:
    HTTPRequest();
    explicit HTTPRequest(std::string const& sData);
    explicit HTTPRequest(const uint8_t* pData, uint32_t len);
    ~HTTPRequest() override = default;

    std::unique_ptr<uint8_t[]> create() override;
    bool parse() override;

    uint32_t methodStrToInt(std::string_view name) const;
    std::string methodIntToStr(uint32_t mid) const;

    // info getter&setter
    void setMethod(uint32_t m){
        method = m;
    }

    uint32_t getMethod() const{
        return method;
    }

    void setRequestUri(std::string_view u){
        requestUri = u;
    }

    std::string getRequestUuri() const{
        return requestUri;
    }
};

#endif