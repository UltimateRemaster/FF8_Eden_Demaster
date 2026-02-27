#pragma once
#include <string>


namespace zzz
{
    inline const std::string zzzMain = "main.zzz";
    inline const std::string zzzOther = "other.zzz";
    
    struct zzzEntry {
        char fileName[260];
        uint64_t offset;
        uint32_t size;
    };
    
    void UnpackZzz(const std::string& path, const std::string& outputDir);
}
