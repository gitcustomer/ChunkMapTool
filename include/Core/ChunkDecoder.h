#ifndef MCATOOL_CHUNKDECODER_H
#define MCATOOL_CHUNKDECODER_H

#include "Core/DataStructures.h"
#include "Core/NbtParser.h"
#include "Utils/Compression.h"
#include <memory>

namespace MCATool {

/**
 * @brief Chunk 解码器
 * 
 * 负责将 MCA Chunk 数据解码为可读的方块数据
 * 流程: ChunkData -> NBT -> PalettedChunk -> DecodedChunk
 */
class ChunkDecoder {
public:
    /**
     * @brief 解码 Chunk 数据
     * @param chunkData 原始 Chunk 数据（包含压缩的 NBT）
     * @return 解码后的 Chunk
     */
    static DecodedChunk decode(const ChunkData& chunkData);
    
    /**
     * @brief 将 NBT Compound 转换为 PalettedChunk
     * @param nbtCompound NBT 根节点
     * @return PalettedChunk 对象
     */
    static PalettedChunk nbtToPalettedChunk(const std::shared_ptr<NbtCompound>& nbtCompound);
    
    /**
     * @brief 将 PalettedChunk 解码为 DecodedChunk
     * @param palettedChunk Paletted 格式的 Chunk
     * @return 解码后的 Chunk（包含完整的方块名称数组）
     */
    static DecodedChunk palettedChunkToDecoded(const PalettedChunk& palettedChunk);

private:
    /**
     * @brief 解码 Paletted Container 数据
     * @param data 压缩的索引数据（long 数组）
     * @param palette 调色板（方块名称列表）
     * @param count 要解码的元素数量（通常是 4096 或 64）
     * @return 解码后的字符串数组
     */
    static std::vector<std::string> decodePalettedContainer(
        const std::vector<int64_t>& data,
        const std::vector<std::string>& palette,
        int count
    );
    
    /**
     * @brief 解包可变位宽的索引数据
     * @param data long 数组
     * @param bitsPerBlock 每个索引占用的位数
     * @param count 要解包的索引数量
     * @return 索引数组
     */
    static std::vector<int> unpackIndices(
        const std::vector<int64_t>& data,
        int bitsPerBlock,
        int count
    );
    
    /**
     * @brief 计算每个索引需要的位数
     * @param paletteSize 调色板大小
     * @return 位数（最小为 4）
     */
    static int calculateBitsPerBlock(int paletteSize);
    
    /**
     * @brief 从 NBT List 中提取 Section 数据
     * @param sectionsList NBT List（包含多个 Section）
     * @return Section 数组
     */
    static std::vector<Section> extractSections(const NbtList* sectionsList);
    
    /**
     * @brief 从 NBT Compound 中提取 BlockStates
     * @param blockStatesCompound NBT Compound
     * @return BlockStates 对象
     */
    static BlockStates extractBlockStates(const NbtCompound* blockStatesCompound);
    
    /**
     * @brief 从 NBT List 中提取方块调色板
     * @param paletteList NBT List
     * @return 方块名称数组
     */
    static std::vector<std::string> extractPalette(const NbtList* paletteList);
    
    /**
     * @brief 从 NBT List 中提取生物群系调色板
     * @param paletteList NBT List
     * @return 生物群系名称数组
     */
    static std::vector<std::string> extractBiomePalette(const NbtList* paletteList);
    
    /**
     * @brief 转换旧版本 Section（使用 Blocks 和 Data 数组）
     * @param sectionCompound Section NBT Compound
     * @return BlockStates 对象
     */
    static BlockStates convertLegacySection(const NbtCompound* sectionCompound);
    
    /**
     * @brief 将旧版本 Block ID 和 Data 转换为方块名称
     * @param blockId 方块 ID (0-255)
     * @param blockData 方块数据值 (0-15)
     * @return 方块名称字符串
     */
    static std::string legacyBlockIdToName(uint8_t blockId, uint8_t blockData);
    
    /**
     * @brief 加载旧版本 Block ID 映射表（从 JSON 文件）
     * @return 是否成功加载
     */
    static bool loadLegacyBlockIdMapping();
    
    /**
     * @brief 获取 legacy ids.json 文件路径
     * @return JSON 文件路径
     */
    static std::string getLegacyIdsPath();

private:
    // 静态映射表：key 为 "blockId" 或 "blockId:blockData"，value 为方块名称
    static std::map<std::string, std::string> s_blockIdToNameMapping;
    static bool s_mappingLoaded;
    
    /**
     * @brief 从 NBT Compound 中提取方块实体
     * @param nbtCompound NBT 根节点
     * @return 方块实体数组
     */
    static std::vector<BlockEntity> extractBlockEntities(const std::shared_ptr<NbtCompound>& nbtCompound);
    
    /**
     * @brief 从 NBT Compound 指针中提取方块实体（内部使用）
     * @param nbtCompound NBT Compound 指针
     * @return 方块实体数组
     */
    static std::vector<BlockEntity> extractBlockEntitiesFromPtr(const NbtCompound* nbtCompound);
    
    /**
     * @brief 从 NBT Compound 中提取实体
     * @param nbtCompound NBT 根节点
     * @return 实体数组
     */
    static std::vector<Entity> extractEntities(const std::shared_ptr<NbtCompound>& nbtCompound);
    
    /**
     * @brief 从 NBT Compound 指针中提取实体（内部使用）
     * @param nbtCompound NBT Compound 指针
     * @return 实体数组
     */
    static std::vector<Entity> extractEntitiesFromPtr(const NbtCompound* nbtCompound);
};

} // namespace MCATool

#endif // MCATOOL_CHUNKDECODER_H
