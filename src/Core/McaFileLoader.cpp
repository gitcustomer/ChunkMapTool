/**
 * @file McaFileLoader.cpp
 * @brief MCA (Minecraft Anvil) 文件加载器实现
 * 
 * MCA 是 Minecraft 的区域文件格式，用于存储世界数据。
 * 每个 MCA 文件存储一个 32×32 的 Chunk 区域（共 1024 个 Chunk）。
 * 
 * MCA 文件结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 0x0000 - 0x0FFF: Location Table (4KB)                      │
 * │   - 1024 个条目，每个 4 字节                                │
 * │   - 前 3 字节：Chunk 数据偏移量（以 4KB 扇区为单位）        │
 * │   - 第 4 字节：Chunk 数据占用的扇区数                       │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 0x1000 - 0x1FFF: Timestamp Table (4KB)                     │
 * │   - 1024 个条目，每个 4 字节                                │
 * │   - 存储每个 Chunk 的最后修改时间（Unix 时间戳）            │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 0x2000 - EOF: Chunk Data                                   │
 * │   - 每个 Chunk 数据格式：                                   │
 * │     - 4 字节：数据长度（大端序）                            │
 * │     - 1 字节：压缩类型（1=GZip, 2=Zlib, 3=未压缩）          │
 * │     - N 字节：压缩的 NBT 数据                               │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 文件命名规则：r.X.Z.mca
 *   - X: Region 的 X 坐标
 *   - Z: Region 的 Z 坐标
 *   - 例如：r.-1.2.mca 表示 Region(-1, 2)
 * 
 * Chunk 索引计算：
 *   - Region 内的 Chunk 索引 = localZ * 32 + localX
 *   - localX = chunkX & 31 (取低 5 位)
 *   - localZ = chunkZ & 31 (取低 5 位)
 * 
 * 参考文档：https://minecraft.wiki/w/Region_file_format
 */

#include "Core/McaFileLoader.h"
#include "Utils/Logger.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <regex>

namespace MCATool {

// ============================================================================
// 字节序转换函数
// ============================================================================

/**
 * @brief 32 位无符号整数字节序交换（大端序 ↔ 小端序）
 * 
 * MCA 文件头中的所有数值都是大端序存储的，
 * 需要转换为本机字节序（x86/x64 是小端序）才能正确使用。
 * 
 * @param value 原始值（大端序）
 * @return 转换后的值（小端序）
 */
static inline uint32_t swapUInt32(uint32_t value) {
    return ((value & 0xFF) << 24) |       // 第 0 字节 → 第 3 字节
           ((value & 0xFF00) << 8) |      // 第 1 字节 → 第 2 字节
           ((value >> 8) & 0xFF00) |      // 第 2 字节 → 第 1 字节
           ((value >> 24) & 0xFF);        // 第 3 字节 → 第 0 字节
}

// ============================================================================
// 主要加载函数
// ============================================================================

/**
 * @brief 加载 MCA 文件，返回 Region 对象
 * 
 * 加载流程：
 * 1. 打开文件
 * 2. 从文件名解析 Region 坐标
 * 3. 读取 8KB 文件头（Location Table + Timestamp Table）
 * 4. 遍历 1024 个 Chunk 位置，读取非空的 Chunk 数据
 * 
 * @param mcaFilePath MCA 文件路径
 * @return Region 对象，包含所有 Chunk 数据
 * @throws std::runtime_error 如果文件无法打开
 */
Region McaFileLoader::loadRegion(const std::string& mcaFilePath) {
    Logger::info("Loading MCA file: " + mcaFilePath);
    
    // 以二进制模式打开文件
    std::ifstream file(mcaFilePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open MCA file: " + mcaFilePath);
    }
    
    Region region;
    
    // ========== 步骤 1：从文件名解析 Region 坐标 ==========
    // 文件名格式：r.X.Z.mca，例如 r.-1.2.mca
    size_t lastSlash = mcaFilePath.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? 
                          mcaFilePath.substr(lastSlash + 1) : mcaFilePath;
    
    if (!parseRegionCoordinates(fileName, region.regionX, region.regionZ)) {
        Logger::warning("Failed to parse region coordinates from filename: " + fileName);
        region.regionX = 0;
        region.regionZ = 0;
    }
    
    // ========== 步骤 2：读取文件头 (8KB) ==========
    // Location Table: 1024 个 4 字节条目，记录每个 Chunk 的位置
    // Timestamp Table: 1024 个 4 字节条目，记录每个 Chunk 的修改时间
    uint32_t locations[1024];
    uint32_t timestamps[1024];
    readHeader(file, locations, timestamps);
    
    // ========== 步骤 3：读取所有非空 Chunk ==========
    int loadedChunks = 0;
    for (int i = 0; i < 1024; i++) {
        uint32_t location = locations[i];
        
        // location == 0 表示该位置没有 Chunk 数据
        if (location == 0) {
            continue;
        }
        
        // 解析 location 字段：
        // ┌─────────────────────────────────────────────┐
        // │ 位 31-8 (24位): 偏移量（以 4KB 扇区为单位）  │
        // │ 位 7-0 (8位): 扇区数量                      │
        // └─────────────────────────────────────────────┘
        uint32_t offset = (location >> 8) * 4096;  // 转换为字节偏移
        uint32_t sectorCount = location & 0xFF;    // 占用的扇区数
        
        if (sectorCount == 0) {
            continue;  // 无效 Chunk
        }
        
        try {
            // 读取 Chunk 数据
            ChunkData chunk = readChunk(file, offset, sectorCount);
            
            // 根据索引计算 Chunk 的全局坐标
            getChunkCoordinates(i, region.regionX, region.regionZ, chunk.chunkX, chunk.chunkZ);
            
            region.chunks.push_back(chunk);
            loadedChunks++;
        } catch (const std::exception& e) {
            // 记录错误但继续处理其他 Chunk
            int32_t chunkX, chunkZ;
            getChunkCoordinates(i, region.regionX, region.regionZ, chunkX, chunkZ);
            Logger::warning("Failed to read chunk (" + std::to_string(chunkX) + ", " + 
                          std::to_string(chunkZ) + "): " + e.what());
        }
    }
    
    file.close();
    
    Logger::info("Loaded " + std::to_string(loadedChunks) + " chunks from region (" + 
                std::to_string(region.regionX) + ", " + std::to_string(region.regionZ) + ")");
    
    return region;
}

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 从文件名解析 Region 坐标
 * 
 * 文件名格式：r.X.Z.mca 或 r.X.Z.mcr
 * 示例：
 *   - r.0.0.mca   → Region(0, 0)
 *   - r.-1.2.mca  → Region(-1, 2)
 *   - r.10.-5.mca → Region(10, -5)
 * 
 * @param fileName 文件名（不含路径）
 * @param regionX 输出参数：Region X 坐标
 * @param regionZ 输出参数：Region Z 坐标
 * @return 是否成功解析
 */
bool McaFileLoader::parseRegionCoordinates(const std::string& fileName, int32_t& regionX, int32_t& regionZ) {
    // 使用正则表达式匹配 r.X.Z.mca 格式
    // -?\d+ 匹配可选负号 + 一个或多个数字
    std::regex pattern(R"(r\.(-?\d+)\.(-?\d+)\.mca?)");
    std::smatch match;
    
    if (std::regex_search(fileName, match, pattern)) {
        regionX = std::stoi(match[1].str());  // 第一个捕获组：X 坐标
        regionZ = std::stoi(match[2].str());  // 第二个捕获组：Z 坐标
        return true;
    }
    
    return false;
}

/**
 * @brief 读取 MCA 文件头（8KB）
 * 
 * 文件头结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 0x0000 - 0x0FFF: Location Table (4KB, 1024 × 4 字节)       │
 * │   每个条目格式：                                            │
 * │   ┌────────────────────────────────────────────────────┐   │
 * │   │ 位 31-8: 偏移量（以 4KB 扇区为单位，大端序）        │   │
 * │   │ 位 7-0:  扇区数量                                   │   │
 * │   └────────────────────────────────────────────────────┘   │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 0x1000 - 0x1FFF: Timestamp Table (4KB, 1024 × 4 字节)      │
 * │   每个条目：Unix 时间戳（大端序）                           │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param file 文件流
 * @param locations 输出参数：Location Table (1024 个条目)
 * @param timestamps 输出参数：Timestamp Table (1024 个条目)
 */
void McaFileLoader::readHeader(std::ifstream& file, uint32_t locations[1024], uint32_t timestamps[1024]) {
    // 读取 Location Table (4KB = 1024 × 4 字节)
    for (int i = 0; i < 1024; i++) {
        uint32_t value;
        file.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
        locations[i] = swapUInt32(value);  // 大端序 → 小端序
    }
    
    // 读取 Timestamp Table (4KB = 1024 × 4 字节)
    for (int i = 0; i < 1024; i++) {
        uint32_t value;
        file.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
        timestamps[i] = swapUInt32(value);  // 大端序 → 小端序
    }
}

/**
 * @brief 读取单个 Chunk 的数据
 * 
 * Chunk 数据格式：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 4 字节：数据长度（大端序，包含压缩类型字节）               │
 * │ 1 字节：压缩类型                                           │
 * │   - 1 = GZip 压缩                                          │
 * │   - 2 = Zlib 压缩（最常用）                                │
 * │   - 3 = 未压缩                                             │
 * │ N 字节：压缩的 NBT 数据（N = 长度 - 1）                    │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param file 文件流
 * @param offset Chunk 数据在文件中的字节偏移量
 * @param sectorCount Chunk 数据占用的扇区数（每扇区 4KB）
 * @return ChunkData 对象
 */
ChunkData McaFileLoader::readChunk(std::ifstream& file, uint32_t offset, uint32_t sectorCount) {
    ChunkData chunk;
    
    // 定位到 Chunk 数据位置
    file.seekg(offset, std::ios::beg);
    
    // 读取数据长度（4 字节，大端序）
    // 这个长度包含了压缩类型字节，所以实际 NBT 数据长度 = length - 1
    uint32_t length;
    file.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));
    length = swapUInt32(length);
    
    // 验证长度的合理性
    if (length == 0 || length > sectorCount * 4096) {
        chunk.isEmpty = true;
        return chunk;
    }
    
    // 读取压缩类型（1 字节）
    // 1 = GZip, 2 = Zlib, 3 = 未压缩
    uint8_t compressionType;
    file.read(reinterpret_cast<char*>(&compressionType), 1);
    chunk.compressionType = static_cast<CompressionType>(compressionType);
    
    // 读取压缩的 NBT 数据
    // 长度 = length - 1（减去压缩类型字节）
    size_t dataLength = length - 1;
    chunk.nbtData.resize(dataLength);
    file.read(reinterpret_cast<char*>(chunk.nbtData.data()), dataLength);
    
    chunk.isEmpty = false;
    
    return chunk;
}

/**
 * @brief 从 Chunk 索引计算全局 Chunk 坐标
 * 
 * Region 内的 Chunk 布局（32×32 = 1024 个 Chunk）：
 * ┌─────────────────────────────────────┐
 * │ 索引 = localZ * 32 + localX        │
 * │                                     │
 * │ localX = 索引 % 32  (0-31)         │
 * │ localZ = 索引 / 32  (0-31)         │
 * └─────────────────────────────────────┘
 * 
 * 全局坐标计算：
 *   chunkX = regionX * 32 + localX
 *   chunkZ = regionZ * 32 + localZ
 * 
 * @param chunkIndex Chunk 在 Region 中的索引 (0-1023)
 * @param regionX Region X 坐标
 * @param regionZ Region Z 坐标
 * @param chunkX 输出参数：Chunk 全局 X 坐标
 * @param chunkZ 输出参数：Chunk 全局 Z 坐标
 */
void McaFileLoader::getChunkCoordinates(int chunkIndex, int32_t regionX, int32_t regionZ, 
                                       int32_t& chunkX, int32_t& chunkZ) {
    // 计算 Region 内的局部坐标
    int localX = chunkIndex % 32;  // 取余得到 X
    int localZ = chunkIndex / 32;  // 整除得到 Z
    
    // 转换为全局坐标
    // 每个 Region 包含 32×32 个 Chunk
    chunkX = regionX * 32 + localX;
    chunkZ = regionZ * 32 + localZ;
}

} // namespace MCATool
