#ifndef _HTTPRESPONSE_H_
#define _HTTPRESPONSE_H_

#include "HTTPmessage.h"

#include<memory>

class HTTPResponse final: public HTTPMessage{
private:
    int32_t status = 0;
    std::string reason = "";

    void determineReasonStr();
    void determineStatusCode();

public:
    HTTPResponse();
    explicit HTTPResponse(std::string const& sData);
    explicit HTTPResponse(const uint8_t* pData, uint32_t len);
    ~HTTPResponse() override = default;

    std::unique_ptr<uint8_t[]> create() override;
    bool parse() override;

    void setStatus(int32_t scode) {
        status = scode;
        determineReasonStr();
    }

    std::string getReadson() const {
        return reason;
    }
};

#endif