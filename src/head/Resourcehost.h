#ifndef _RESOURCEHOST_H_
#define _RESOURCEHOST_H_

#include <string>
#include<unordered_map>
#include<memory>
#include<vector>

#include "Resource.h"

class ResourceHost{
private:
    std::string baseDiskPath;  // 本地文件系统路径

    std::string lookupMImeType(std::string const& ext) const;

    std::unique_ptr<Resource> readFile(std::string const& path, struct stat const& sb);   //  从 FS 将文件读入资源对象

    std::unique_ptr<Resource> readDirectory(std::string path, struct stat const& sb);  // 将目录列表或索引从 FS 读入资源对象

    std::string generateDirList(std::string const& dirPath) const;  // 根据 URI 提供目录列表的字符串

public:
    explicit ResourceHost(std::string const& base);
    ~ResourceHost() = default;

    std::unique_ptr<Resource> getResource(std::string const& uri);
};

#endif