#include "Resourcehost.h"

#include <memory>
#include <sstream>
#include <string>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// 有效文件可用作目录索引
const static std::vector<std::string> g_validIndexes = {
    "index.html",
    "index.htm",
};

// 将文件扩展名与其MIME类型相关联的字典
const static std::unordered_map<std::string, std::string, std::hash<std::string>, std::equal_to<>> g_mimeMap = {
#include "MimeTypes.inc"
};

ResourceHost::ResourceHost(std::string const& base) : baseDiskPath(base){
    // TODO: 检查 baseDiskPath 是否是有效路径
}

// 在字典中查找 MIME 类型
// @param ext 用于查找的文件扩展名
// @return MIME 类型，作为字符串。如果未找到类型，则返回空字符串
std::string ResourceHost::lookupMimeType(std::string const& ext) const{
    auto it = g_mimeMap.find(ext);
    if (it == g_mimeMap.end())
        return "";
    
    return it->second;
}
