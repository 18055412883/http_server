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

// 读取文件
// 从磁盘读取文件并返回相应的资源对象
// 这将创建一个新的资源对象--如果返回值不是空值，调用者应将其处理掉
// @param path 文件的完整磁盘路径
// @param sb 填充 stat 结构
// 成功加载后返回资源对象
std::unique_ptr<Resource> ResourceHost::readFile(std::string const& path, struct stat const& sb){
    // 确保webserver User 拥有文件
    if (!(sb.st_mode & S_SIRWXU))
        return nullptr;
    // 创建新资源对象并设置内容
    auto res = std::make_unique<Resource>(path);
    std::string name = res->getName();
    if (name.length() == 0)
        return nullptr;
    
    // 始终禁止隐藏文件
    if (name.starts_with(".")) {
        return nullptr;
    }
    std::ifstream file;
    uint32_t len = 0;
    
    // 打开文件
    file.open(path, std::ios::binary);

    // 如果文件打开失败，返回空
    if (!file.is_open())
        return nullptr;
    
    // 获取文件大小
    len = sb.st_size;

    // 为文件内容分配 memory 并读取内容
    auto fdata = new uint8_t[len];
    memset(fdata, 0x00,len);
    file.read((char*)fdata, len);

    // 关闭文件
    file.close();

    if (auto mimetype = lookupMimeType(res->getExtension());mimetype.length() != 0){
        res->setMimeType(mimetype);
    }
    else{
        res->setMimeType("application/octet-stream");
    }

    res->setData(fdata, len);
    return res;
}

// 读取目录
// 从磁盘读取目录（列表或索引）到资源对象中
// 这将创建一个新的资源对象--如果返回值不是空值，调用者应将其处理掉
// @param path 文件的完整磁盘路径
// @param sb 填充 stat 结构
// 成功加载后返回资源对象
std::unique_ptr<Resource> ResourceHost::readDirectory(std::string path, struct stat const& sb){
    // 如果路径末尾没有 /，则以 / 结尾（以保持一致)
    if (path.empty() || path[path.length() - 1] != '/')
        path += "/";
    // 探测有效索引
    uint32_t numIndexes = std::size(g_validIndexes);
    std::string loadIndex;
    struct stat sidx = {0};
    for (uint32_t i = 0; i < numIndexed; ++i){
        loadIndex = path + g_validIndexes[i];
        // 找到合适的索引文件加载并返回客户端
        if (stat(loadIndex.c_str(), &sidx) == 0)
            return readFile(loadIndex, sidx);
    }
    // 确保网络服务器 USER 拥有该目录
    if (!(sb.st_mode & S_IRWXU))
        return nullptr;

    // 生成 HTML 目录列表
    std::string listing = generateDirList(path);

    uint32_t slen = listing.length();
    auto sdata = new uint8_t[slen];
    memset(sdata, 0x00, slen);
    strnpy((char*)sdata, listing.c_str(), slen);

    auto res = std::make_unique<Resource>(path, true);
    res->setMimeType("text/html");
    res->setData(sdata, slen);
    return res;
}

// 返回由相对路径 dirPath 提供的 HTML 目录列表
// @param path 文件的完整磁盘路径
// @return 返回目录的 HTML 字符串表示。如果目录无效，则返回空白字符串
std::string ResourceHost::generateDirList(std::string const& path) const {
    // 从整个路径中删除开头的 baseDiskPath，只获取相对 uri
    size_t uri_pos = path.find(baseDiskPath);
    std::string uri = "?";
    if (uri_pos != std::string::npos)
        uri = path.substr(uri_pos + baseDiskPath.length());
    std::stringstream ret;
    ret << "<html><head><title>" << uri << "</title></head><body>";
    const struct dirent* ent = nullptr;
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr)
        return "";
    // 页面标题，显示所列目录的 URI
    ret << "<h1>Index of " << uri << "</h1><hr /><br />";
    // 将所有文件和目录添加到返回值中
    while ((ent = readdir(dir)) != nullptr){
        // 跳过隐藏文件 (以 . 开头)
        if (ent->d_name[0] == '.')
            continue;
        // 显示目录中对象的链接：
        ret << "<a href=\"" << uri << ent->d_name << "\">" << ent->d_name << "</a><br />";
    }

    closedir(dir);

    ret << "</body></html>";
    return ret.str();
}

// 从文件系统读取资源
// 返回一个新的资源对象--如果返回值不是空值，调用者应将其处理掉
// @param uri 请求中发送的 URI
// @reutn 如果无法加载资源，则返回 NULL
std::unique_ptr<Resource> ResourceHost::getResource(std::string const& uri) {
    if (uri.length() > 255 || uri.empty())
        return nullptr;

    // 不允许目录遍历
    if (uri.contains("../") || uri.contains("/.."))
        return nullptr;
    
    // 使用 stat 收集有关资源的信息：确定它是目录还是文件，检查它是否为组/用户所有，修改次数
    std::string path = baseDiskPath + uri;
    struct stat sb = {0};
    if (stat(path.c_str(), &sb) != 0)
        return nullptr;
    
    if (sb.st_mode & S_IFDIR){
        // 从 FS 将目录列表或索引读入内存
        return readDirectory(path, sb);
    } else if (sb.st_mode & S_IFREG){
        // 尝试从 FS 将文件加载到内存中
        return readFile(path, sb);
    } else {

    }
    return nullptr;
}