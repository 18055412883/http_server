#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include<string>
#include<memory>

class Resource{
private:
    uint8_t* data = nullptr;  // file data
    uint32_t size = 0;        // 无符号整数
    std::string mimeType = "";
    std::string location;     // 服务器内的磁盘路径
    bool directory;

public:
    Resource(std::string const& loc, bool dir=false);
    ~Resource();

    // setter
    void setData(uint8_t* d, uint32_t s){
        data = d;
        size = s;
    }
    void setMimeType(std::string_view mt) {
        mimeType = mt;
    }

    // getter
    std::string getMimeType() const {
        return mimeType;
    }

    std::string getLocation() const {
        return location;
    }
    bool isDirectory() const {
        return directory;
    }

    uint8_t* getData() const {
        return data;
    }

    uint32_t getSize() const {
        return size;
    }

    // get file name
    std::string getName() const{
        std::string name="";
        if(auto slash_pos = location.find_last_of("/"); slash_pos != std::string::npos)     // 提取路径或文件名：通过查找 location 字符串中最后一个 / 字符的位置，然后从该位置之后提取文件名或路径的最后一部分。
        // 通过 if 语句的条件部分初始化 slash_pos 变量。这种写法使得 slash_pos 只在 if 语句的作用域内有效。
            name = location.substr(slash_pos + 1);
        return name;
    }

    // get file extension
    std::string getExtension() const {
        std::string ext="";
        if(auto ext_pos = location.find_last_of("."); ext_pos != std::string::npos)
            ext = location.substr(ext_pos + 1);
        return ext;
    }
};

#endif