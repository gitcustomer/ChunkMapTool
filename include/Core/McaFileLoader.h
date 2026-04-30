#ifndef MCATOOL_MCAFILELOADER_H
#define MCATOOL_MCAFILELOADER_H

#include "Core/DataStructures.h"
#include <string>
#include <memory>

namespace MCATool {

/**
 * @brief MCA (Minecraft Anvil) 文件加载器
 * 
 * 负责加载和解析 .mca 格式的 Region 文件
 * MCA 文件格式:
 * - Header (8KB): Location Table (4KB) + Timestamp Table (4KB)
 * - Chunk Data: 变长的压缩 NBT 数据
 */
class McaFileLoader {
public:
    /**
     * @brief 加载 MCA 文件
     * @param mcaFilePath MCA 文件路径
     * @return Region 对象，包含所有 Chunk 数据
     */
    static Region loadRegion(const std::string& mcaFilePath);
    
    /**
     * @brief 从 MCA 文件名解析 Region 坐标
     * @param fileName 文件名（例如: "r.0.0.mca" 或 "r.-1.2.mca"）
     * @param regionX 输出参数：Region X 坐标
     * @param regionZ 输出参数：Region Z 坐标
     * @return 是否成功解析
     */
    static bool parseRegionCoordinates(const std::string& fileName, int32_t& regionX, int32_t& regionZ);

private:
    /**
     * @brief 读取 MCA 文件头（8KB）
     * @param file 文件流
     * @param locations 输出参数：Location Table (1024 个条目)
     * @param timestamps 输出参数：Timestamp Table (1024 个条目)
     */
    static void readHeader(std::ifstream& file, uint32_t locations[1024], uint32_t timestamps[1024]);
    
    /**
     * @brief 读取单个 Chunk 数据
     * @param file 文件流
     * @param offset Chunk 数据在文件中的偏移量（字节）
     * @param sectorCount Chunk 数据占用的扇区数（每扇区 4KB）
     * @return ChunkData 对象
     */
    static ChunkData readChunk(std::ifstream& file, uint32_t offset, uint32_t sectorCount);
    
    /**
     * @brief 从 Chunk 索引计算 Chunk 坐标
     * @param chunkIndex Chunk 在 Region 中的索引 (0-1023)
     * @param regionX Region X 坐标
     * @param regionZ Region Z 坐标
     * @param chunkX 输出参数：Chunk X 坐标
     * @param chunkZ 输出参数：Chunk Z 坐标
     */
    static void getChunkCoordinates(int chunkIndex, int32_t regionX, int32_t regionZ, 
                                   int32_t& chunkX, int32_t& chunkZ);
};

} // namespace MCATool

#endif // MCATOOL_MCAFILELOADER_H
