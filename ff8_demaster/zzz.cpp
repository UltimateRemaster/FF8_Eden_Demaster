#include "zzz.h"

#include <filesystem>
#include <fstream>

#include "debug.h"

void zzz::UnpackZzz(const std::string& path, const std::string& outputDir)
{
    //double-check if the file exists
    if (!std::filesystem::exists(path))
    {
        OutputDebug("[ZZZ] File does not exist: %s\n", path.c_str());
        return;
    }
    
    std::ifstream zzz(path, std::ios::binary);
    if (!zzz.is_open()) return;
    
    OutputDebug("[ZZZ] Unpacking ZZZ file %s\n", path.c_str());
    
    uint32_t fileCount = 0;
    zzz.read(reinterpret_cast<char*>(&fileCount), sizeof(uint32_t));
    
    std::vector<zzzEntry> entries(fileCount);
    
    for (uint32_t i = 0; i < fileCount; ++i)
    {
        zzzEntry entry;
        uint32_t nameSize = 0;
        zzz.read(reinterpret_cast<char*>(&nameSize), sizeof(uint32_t));
        zzz.read(entry.fileName, nameSize);
        entry.fileName[nameSize] = '\0';
        
        zzz.read(reinterpret_cast<char*>(&entry.offset), sizeof(uint64_t));
        zzz.read(reinterpret_cast<char*>(&entry.size), sizeof(uint32_t));
        entries[i] = entry;
    }

    uint32_t localIndex = 0;
    for (const auto& entry : entries)
    {
        OutputDebug("[ZZZ] (%u/%u) Unpacking file %s...\n",localIndex + 1, fileCount, entry.fileName);
        std::filesystem::path outPath = std::filesystem::path(outputDir) / entry.fileName;
        std::filesystem::create_directories(outPath.parent_path());

        // Extract file
        std::vector<char> buffer(entry.size);
        zzz.seekg(entry.offset);
        zzz.read(buffer.data(), entry.size);

        std::ofstream outFile(outPath, std::ios::binary);
        outFile.write(buffer.data(), entry.size);
        localIndex++;
    }
    zzz.close();
}
