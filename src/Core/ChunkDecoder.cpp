
/**
 * @file ChunkDecoder.cpp
 * @brief Chunk 解码器实现
 * 
 * 本文件负责将 MCA 文件中的 Chunk 数据解码为可读的方块数据。
 * 
 * 解码流程：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ ChunkData (压缩的二进制数据)                                │
 * │     ↓ Compression::decompress()                            │
 * │ 解压后的 NBT 二进制数据                                     │
 * │     ↓ NbtParser::parse()                                   │
 * │ NbtCompound (NBT 树形结构)                                  │
 * │     ↓ nbtToPalettedChunk()                                 │
 * │ PalettedChunk (调色板 + 压缩索引)                           │
 * │     ↓ palettedChunkToDecoded()                             │
 * │ DecodedChunk (完整的方块名称数组)                           │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 调色板压缩原理：
 * - 一个 Section (16×16×16) 有 4096 个方块
 * - 如果每个方块都存储完整名称，会非常浪费空间
 * - 调色板压缩：先列出所有不同的方块类型（调色板），然后用索引引用
 * - 例如：调色板 = ["air", "stone", "dirt"]
 *         数据 = [0, 0, 1, 2, 1, 0, ...]  (索引)
 *         解码 = ["air", "air", "stone", "dirt", "stone", "air", ...]
 * 
 * 索引的位压缩：
 * - 索引被打包到 int64 数组中，每个索引占用固定位数
 * - 位数 = ceil(log2(调色板大小))，最小为 4 位
 * - 例如：调色板有 16 种方块 → 每个索引 4 位 → 每个 int64 存 16 个索引
 */

#include "Core/ChunkDecoder.h"
#include "Utils/Logger.h"
#include <cmath>
#include <fstream>
#include <sstream>

namespace MCATool {

// ============================================================================
// 静态成员变量初始化
// ============================================================================

// 旧版本方块 ID 到名称的映射表（从 ids.json 加载）
std::map<std::string, std::string> ChunkDecoder::s_blockIdToNameMapping;
// 映射表是否已加载
bool ChunkDecoder::s_mappingLoaded = false;

// ============================================================================
// 主解码函数
// ============================================================================

/**
 * @brief 解码 Chunk 数据（主入口函数）
 * 
 * 完整的解码流程：
 * 1. 解压 NBT 数据（Zlib/GZip）
 * 2. 解析 NBT 二进制格式为树形结构
 * 3. 从 NBT 提取调色板格式的 Chunk 数据
 * 4. 将调色板索引展开为完整的方块名称数组
 * 
 * @param chunkData 原始 Chunk 数据（包含压缩的 NBT）
 * @return 解码后的 Chunk（包含完整的方块名称数组）
 * @throws std::runtime_error 如果 Chunk 为空或解码失败
 */
DecodedChunk ChunkDecoder::decode(const ChunkData& chunkData) {
    // 检查是否为空 Chunk
    if (chunkData.isEmpty) {
        throw std::runtime_error("Cannot decode empty chunk");
    }
    
    // ========== 步骤 1：解压 NBT 数据 ==========
    // MCA 文件中的 Chunk 数据通常使用 Zlib 压缩
    // 压缩类型：1=GZip, 2=Zlib, 3=未压缩
    std::vector<uint8_t> decompressedData = Compression::decompress(
        chunkData.nbtData, 
        static_cast<uint8_t>(chunkData.compressionType)
    );
    
    // ========== 步骤 2：解析 NBT ==========
    // 将二进制 NBT 数据解析为树形结构
    auto nbtCompound = NbtParser::parse(decompressedData);
    
    // ========== 步骤 3：转换为 PalettedChunk ==========
    // 从 NBT 树中提取 Chunk 的结构化数据
    PalettedChunk palettedChunk = nbtToPalettedChunk(nbtCompound);
    palettedChunk.chunkX = chunkData.chunkX;
    palettedChunk.chunkZ = chunkData.chunkZ;
    
    // ========== 步骤 4：解码为 DecodedChunk ==========
    // 将调色板索引展开为完整的方块名称数组
    DecodedChunk decodedChunk = palettedChunkToDecoded(palettedChunk);
    
    return decodedChunk;
}

// ============================================================================
// NBT 到 PalettedChunk 转换
// ============================================================================

/**
 * @brief 将 NBT Compound 转换为 PalettedChunk
 * 
 * Chunk NBT 结构（1.18+ 新格式）：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ root                                                        │
 * │ ├── DataVersion: int                                        │
 * │ ├── xPos: int                                               │
 * │ ├── zPos: int                                               │
 * │ ├── Status: string ("full", "empty", ...)                   │
 * │ ├── sections: list                                          │
 * │ │   └── [0..N]: compound                                    │
 * │ │       ├── Y: byte (Section Y 坐标，-4 到 19)              │
 * │ │       ├── block_states: compound                          │
 * │ │       │   ├── palette: list (方块调色板)                  │
 * │ │       │   │   └── [0..M]: compound                        │
 * │ │       │   │       ├── Name: string ("minecraft:stone")    │
 * │ │       │   │       └── Properties: compound (可选)         │
 * │ │       │   └── data: long_array (压缩的索引数据)           │
 * │ │       └── biomes: compound (生物群系数据)                 │
 * │ │           ├── palette: list                               │
 * │ │           └── data: long_array                            │
 * │ ├── block_entities: list (方块实体)                         │
 * │ └── entities: list (实体，1.17+ 可能在单独文件)             │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 旧版本格式（1.17 之前）：
 * - 数据在 "Level" 标签下
 * - Section 使用 "Sections" 而非 "sections"
 * - 方块数据使用 "Blocks" + "Data" 字节数组而非调色板
 * 
 * @param nbtCompound NBT 根节点
 * @return PalettedChunk 对象
 */
PalettedChunk ChunkDecoder::nbtToPalettedChunk(const std::shared_ptr<NbtCompound>& nbtCompound) {
    PalettedChunk chunk;
    
    // ========== 获取数据根节点（兼容新旧版本）==========
    // 旧版本（1.17 之前）：数据在 "Level" 标签下
    // 新版本（1.18+）：数据直接在根节点下
    const NbtCompound* levelPtr = nullptr;
    
    // 尝试获取 "Level" 标签（旧版本格式）
    levelPtr = nbtCompound->getTag<NbtCompound>("Level");
    
    // 如果没有 Level 标签，尝试其他兼容性路径
    if (!levelPtr) {
        // 尝试获取空字符串标签（某些版本的格式）
        const NbtCompound* emptyTagPtr = nbtCompound->getTag<NbtCompound>("");
        if (emptyTagPtr) {
            // 尝试在空标签下查找 Level
            const NbtCompound* nestedLevelPtr = emptyTagPtr->getTag<NbtCompound>("Level");
            if (nestedLevelPtr) {
                levelPtr = nestedLevelPtr;
            } else {
                // 如果还是没有，使用空字符串标签本身
                levelPtr = emptyTagPtr;
            }
        }
    }
    
    // 如果仍然没有 Level 标签，直接使用根 Compound（新版本格式）
    if (!levelPtr) {
        levelPtr = nbtCompound.get();
    }
    
    // 现在 levelPtr 指向正确的 Level Compound（只是借用指针，不拥有所有权）
    
    // 提取基本信息
    chunk.chunkX = levelPtr->getInt("xPos", 0);
    chunk.chunkZ = levelPtr->getInt("zPos", 0);
    chunk.dataVersion = levelPtr->getInt("DataVersion", 0);
    chunk.status = levelPtr->getString("Status", "");
    
    // 提取 sections（支持新旧格式：sections 或 Sections）
    auto sectionsList = levelPtr->getTag<NbtList>("sections");
    if (!sectionsList) {
        sectionsList = levelPtr->getTag<NbtList>("Sections");
    }
    if (sectionsList) {
        chunk.sections = extractSections(sectionsList);
    }
    
    // 提取 block_entities 和 entities
    // 注意：这些方法需要从 levelPtr 提取，但签名要求 shared_ptr
    // 我们需要修改这些方法的实现，让它们接受原始指针
    chunk.blockEntities = extractBlockEntitiesFromPtr(levelPtr);
    chunk.entities = extractEntitiesFromPtr(levelPtr);
    
    Logger::debug("Converted NBT to PalettedChunk: (" + std::to_string(chunk.chunkX) + 
                  ", " + std::to_string(chunk.chunkZ) + "), " + 
                  std::to_string(chunk.sections.size()) + " sections");
    
    return chunk;
}

// ============================================================================
// PalettedChunk 到 DecodedChunk 转换
// ============================================================================

/**
 * @brief 将 PalettedChunk 解码为 DecodedChunk
 * 
 * 这是解码的最后一步，将调色板索引展开为完整的方块名称数组。
 * 
 * 解码过程：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ PalettedChunk                                               │
 * │ ├── palette: ["air", "stone", "dirt"]                       │
 * │ └── data: [0,0,1,2,1,0,...]  (压缩的索引)                   │
 * │                     ↓                                       │
 * │ DecodedChunk                                                │
 * │ └── blockNames: ["air","air","stone","dirt","stone","air"]  │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param palettedChunk 调色板格式的 Chunk
 * @return 解码后的 Chunk（包含完整的方块名称数组）
 */
DecodedChunk ChunkDecoder::palettedChunkToDecoded(const PalettedChunk& palettedChunk) {
    DecodedChunk decoded;
    decoded.chunkX = palettedChunk.chunkX;
    decoded.chunkZ = palettedChunk.chunkZ;
    decoded.blockEntities = palettedChunk.blockEntities;
    decoded.entities = palettedChunk.entities;
    
    // 解码每个 Section（16×16×16 的子区块）
    for (const auto& section : palettedChunk.sections) {
        DecodedSection decodedSection;
        decodedSection.sectionY = section.sectionY;
        
        // ========== 解码方块数据 ==========
        // 每个 Section 有 16×16×16 = 4096 个方块
        if (!section.blockStates.palette.empty()) {
            auto blockNames = decodePalettedContainer(
                section.blockStates.data,   // 压缩的索引数据
                section.blockStates.palette, // 方块调色板
                4096                         // 方块数量：16×16×16
            );
            
            // 复制到固定大小的数组
            for (size_t i = 0; i < std::min(blockNames.size(), size_t(4096)); i++) {
                decodedSection.blockNames[i] = blockNames[i];
            }
        }
        
        // ========== 解码生物群系数据 ==========
        // 每个 Section 有 4×4×4 = 64 个生物群系采样点
        // （生物群系分辨率比方块低，每 4 格一个采样点）
        if (!section.biomes.palette.empty()) {
            auto biomeNames = decodePalettedContainer(
                section.biomes.data,    // 压缩的索引数据
                section.biomes.palette, // 生物群系调色板
                64                      // 采样点数量：4×4×4
            );
            

            // 复制到固定大小的数组
            for (size_t i = 0; i < std::min(biomeNames.size(), size_t(64)); i++) {
                decodedSection.biomeNames[i] = biomeNames[i];
            }
        }
        
        decoded.sections.push_back(decodedSection);
    }
    
    Logger::debug("Decoded PalettedChunk to DecodedChunk: (" + 
                  std::to_string(decoded.chunkX) + ", " + 
                  std::to_string(decoded.chunkZ) + "), " + 
                  std::to_string(decoded.sections.size()) + " sections");
    
    return decoded;
}  
// ============================================================================
// 调色板容器解码
// ============================================================================

/**
 * @brief 解码调色板容器（方块或生物群系）
 * 
 * 调色板压缩原理：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 调色板 (palette): ["air", "stone", "dirt", "grass"]        │
 * │ 索引数据 (data): 压缩在 int64 数组中的索引                  │
 * │                                                             │
 * │ 解码过程：                                                  │
 * │ 1. 计算每个索引需要的位数（基于调色板大小）                 │
 * │ 2. 从 int64 数组中解包索引                                  │
 * │ 3. 用索引查找调色板，得到方块/生物群系名称                  │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param data 压缩的索引数据（int64 数组）
 * @param palette 调色板（方块/生物群系名称列表）
 * @param count 要解码的元素数量（方块=4096，生物群系=64）
 * @return 解码后的名称数组
 */
std::vector<std::string> ChunkDecoder::decodePalettedContainer(
    const std::vector<int64_t>& data,
    const std::vector<std::string>& palette,
    int count
) {
    std::vector<std::string> result(count);
    
    // 特殊情况 1：调色板只有一个元素
    // 这意味着整个 Section 都是同一种方块（如全是空气）
    // 此时不需要 data 数组，直接填充即可
    if (palette.size() == 1) {
        std::fill(result.begin(), result.end(), palette[0]);
        return result;
    }
    
    // 特殊情况 2：没有数据
    // 使用调色板的第一个元素填充（通常是空气）
    if (data.empty()) {
        std::fill(result.begin(), result.end(), palette.empty() ? "minecraft:air" : palette[0]);
        return result;
    }
    
    // 计算每个索引需要的位数
    // 例如：调色板有 16 种方块 → 需要 4 位来表示索引 0-15
    int bitsPerBlock = calculateBitsPerBlock(palette.size());
    
    // 从 int64 数组中解包索引
    std::vector<int> indices = unpackIndices(data, bitsPerBlock, count);
    
    // 根据索引查找调色板，得到方块名称
    for (int i = 0; i < count && i < static_cast<int>(indices.size()); i++) {
        int index = indices[i];
        if (index >= 0 && index < static_cast<int>(palette.size())) {
            result[i] = palette[index];
        } else {
            result[i] = "minecraft:air";  // 索引越界时使用默认值
        }
    }
    
    return result;
}

// ============================================================================
// 位打包索引解包
// ============================================================================

/**
 * @brief 从 int64 数组中解包位打包的索引
 * 
 * 位打包原理：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 假设每个索引占 4 位，一个 int64 有 64 位                    │
 * │ 那么一个 int64 可以存储 64 / 4 = 16 个索引                  │
 * │                                                             │
 * │ int64 的位布局（4 位/索引）：                               │
 * │ ┌────┬────┬────┬────┬────┬────┬────┬────┬─────────────────┐ │
 * │ │idx0│idx1│idx2│idx3│idx4│idx5│idx6│idx7│ ... │idx15│     │ │
 * │ │4bit│4bit│4bit│4bit│4bit│4bit│4bit│4bit│     │4bit │     │ │
 * │ └────┴────┴────┴────┴────┴────┴────┴────┴─────────────────┘ │
 * │ 位 0-3  4-7  8-11 ...                                       │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 注意：Minecraft 1.16+ 的索引不会跨越 int64 边界
 * 但为了兼容性，代码仍然处理了跨越的情况
 * 
 * @param data int64 数组（压缩的索引数据）
 * @param bitsPerBlock 每个索引占用的位数
 * @param count 要解包的索引数量
 * @return 解包后的索引数组
 */
std::vector<int> ChunkDecoder::unpackIndices(
    const std::vector<int64_t>& data,
    int bitsPerBlock,
    int count
) {
    std::vector<int> indices;
    indices.reserve(count);
    
    // 创建位掩码，用于提取指定位数的值
    // 例如：bitsPerBlock=4 → mask=0b1111=15
    int64_t mask = (1LL << bitsPerBlock) - 1;
    
    int currentLong = 0;  // 当前处理的 int64 索引
    int bitOffset = 0;    // 当前在 int64 中的位偏移
    
    for (int i = 0; i < count; i++) {
        // 检查是否已经读完所有数据
        if (currentLong >= static_cast<int>(data.size())) {
            break;
        }
        
        if (bitOffset + bitsPerBlock <= 64) {
            // ========== 情况 1：索引完全在当前 int64 内 ==========
            // 直接右移并掩码提取
            int64_t value = (data[currentLong] >> bitOffset) & mask;
            indices.push_back(static_cast<int>(value));
            
            // 移动位偏移
            bitOffset += bitsPerBlock;
            if (bitOffset >= 64) {
                // 移动到下一个 int64
                currentLong++;
                bitOffset = 0;
            }
        } else {
            // ========== 情况 2：索引跨越两个 int64 ==========
            // 这种情况在 Minecraft 1.16+ 中不会发生，但为了兼容性保留
            int bitsFromCurrent = 64 - bitOffset;  // 从当前 int64 取的位数
            int bitsFromNext = bitsPerBlock - bitsFromCurrent;  // 从下一个 int64 取的位数
            
            // 从当前 int64 提取低位部分
            int64_t lowBits = (data[currentLong] >> bitOffset) & ((1LL << bitsFromCurrent) - 1);
            
            // 移动到下一个 int64
            currentLong++;
            if (currentLong >= static_cast<int>(data.size())) {
                break;
            }
            
            // 从下一个 int64 提取高位部分
            int64_t highBits = data[currentLong] & ((1LL << bitsFromNext) - 1);
            
            // 组合高位和低位
            int64_t value = (highBits << bitsFromCurrent) | lowBits;
            
            indices.push_back(static_cast<int>(value));
            bitOffset = bitsFromNext;
        }
    }
    
    return indices;
}

/**
 * @brief 根据调色板大小计算每个索引需要的位数
 * 
 * Minecraft 1.16+ 协议规定：
 * - 方块索引的最小位数为 4 位
 * - 生物群系索引的最小位数为 1 位（但通常也用 4 位）
 * 
 * 位数计算规则：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 调色板大小    │ 需要的位数  │ 可表示的范围                  │
 * ├───────────────┼─────────────┼───────────────────────────────┤
 * │ 1 - 16        │ 4 位        │ 0 - 15                        │
 * │ 17 - 32       │ 5 位        │ 0 - 31                        │
 * │ 33 - 64       │ 6 位        │ 0 - 63                        │
 * │ 65 - 128      │ 7 位        │ 0 - 127                       │
 * │ 129 - 256     │ 8 位        │ 0 - 255                       │
 * │ 257 - 512     │ 9 位        │ 0 - 511                       │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param paletteSize 调色板大小（不同方块/生物群系的数量）
 * @return 每个索引需要的位数
 */
int ChunkDecoder::calculateBitsPerBlock(int paletteSize) {
    // Minecraft 1.16+ 协议规定：方块索引位数的最小值为 4 位
    // 即使调色板只有 1-16 个元素，也至少使用 4 位来表示
    if (paletteSize <= 16) return 4;   // 4 位可表示 0-15
    if (paletteSize <= 32) return 5;   // 5 位可表示 0-31
    if (paletteSize <= 64) return 6;   // 6 位可表示 0-63
    if (paletteSize <= 128) return 7;  // 7 位可表示 0-127
    if (paletteSize <= 256) return 8;  // 8 位可表示 0-255
    return 9;  // 最多 9 位（512 个不同的方块）
}

// ============================================================================
// Section 提取函数
// ============================================================================

/**
 * @brief 从 NBT List 中提取所有 Section 数据
 * 
 * Section 是 Chunk 的垂直分段，每个 Section 是 16×16×16 的立方体。
 * Minecraft 1.18+ 的世界高度为 -64 到 319，共 384 格，
 * 需要 24 个 Section（Y = -4 到 Y = 19）。
 * 
 * Section NBT 结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ section: compound                                           │
 * │ ├── Y: byte (Section Y 坐标)                                │
 * │ ├── block_states: compound (新版本格式)                     │
 * │ │   ├── palette: list                                       │
 * │ │   │   └── [0..N]: compound                                │
 * │ │   │       ├── Name: string                                │
 * │ │   │       └── Properties: compound (可选)                 │
 * │ │   └── data: long_array                                    │
 * │ ├── biomes: compound (1.18+ 格式)                           │
 * │ │   ├── palette: list                                       │
 * │ │   └── data: long_array                                    │
 * │ └── (旧版本) Blocks: byte_array + Data: byte_array          │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param sectionsList NBT List（包含多个 Section）
 * @return Section 数组
 */
std::vector<Section> ChunkDecoder::extractSections(const NbtList* sectionsList) {
    std::vector<Section> sections;
    
    // 遍历每个 Section
    for (const auto& sectionTag : sectionsList->value) {
        auto sectionCompound = std::dynamic_pointer_cast<NbtCompound>(sectionTag);
        if (!sectionCompound) {
            continue;
        }
        
        Section section;
        
        // ========== 获取 Section Y 坐标 ==========
        // 支持 "Y"（Byte 类型）和 "Y"（Int 类型）两种格式
        auto yByte = sectionCompound->getTag<NbtByte>("Y");
        if (yByte) {
            section.sectionY = yByte->value;
        } else {
            section.sectionY = sectionCompound->getInt("Y", 0);
        }
        
        // ========== 提取方块状态数据 ==========
        auto blockStatesCompound = sectionCompound->getTag<NbtCompound>("block_states");
        if (blockStatesCompound) {
            // 新版本格式（1.18+）：使用调色板压缩
            section.blockStates = extractBlockStates(blockStatesCompound);
        } else {
            // 旧版本格式（1.12 及之前）：使用 "Blocks" + "Data" 字节数组
            section.blockStates = convertLegacySection(sectionCompound.get());
        }
        
        // ========== 提取生物群系数据（可选，1.18+）==========
        auto biomesCompound = sectionCompound->getTag<NbtCompound>("biomes");
        if (biomesCompound) {
            // 提取生物群系调色板
            auto paletteList = biomesCompound->getTag<NbtList>("palette");
            if (paletteList) {
                section.biomes.palette = extractBiomePalette(paletteList);
            }
            
            // 提取生物群系数据
            auto dataArray = biomesCompound->getTag<NbtLongArray>("data");
            if (dataArray) {
                section.biomes.data = dataArray->value;
            }
        }
        
        sections.push_back(section);
    }
    
    return sections;
}

/**
 * @brief 从 NBT Compound 中提取方块状态数据
 * 
 * block_states 结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ block_states: compound                                      │
 * │ ├── palette: list (方块调色板)                              │
 * │ │   └── [0..N]: compound                                    │
 * │ │       ├── Name: string (如 "minecraft:stone")             │
 * │ │       └── Properties: compound (可选，方块状态属性)       │
 * │ │           ├── facing: string (如 "north")                 │
 * │ │           ├── half: string (如 "top")                     │
 * │ │           └── ...                                         │
 * │ └── data: long_array (压缩的索引数据)                       │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param blockStatesCompound block_states NBT Compound
 * @return BlockStates 对象
 */
BlockStates ChunkDecoder::extractBlockStates(const NbtCompound* blockStatesCompound) {
    BlockStates blockStates;
    
    // 提取调色板
    auto paletteList = blockStatesCompound->getTag<NbtList>("palette");
    if (paletteList) {
        blockStates.palette = extractPalette(paletteList);
    }
    
    // 提取压缩的索引数据
    auto dataArray = blockStatesCompound->getTag<NbtLongArray>("data");
    if (dataArray) {
        blockStates.data = dataArray->value;
    }
    
    return blockStates;
}

/**
 * @brief 从 NBT List 中提取方块调色板
 * 
 * 调色板条目格式：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ palette_entry: compound                                     │
 * │ ├── Name: string (方块 ID，如 "minecraft:oak_stairs")       │
 * │ └── Properties: compound (可选，方块状态)                   │
 * │     ├── facing: "east"                                      │
 * │     ├── half: "bottom"                                      │
 * │     └── shape: "straight"                                   │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 输出格式：
 * - 无属性："minecraft:stone"
 * - 有属性："minecraft:oak_stairs[facing=east,half=bottom,shape=straight]"
 * 
 * @param paletteList 调色板 NBT List
 * @return 方块名称数组（包含属性）
 */
std::vector<std::string> ChunkDecoder::extractPalette(const NbtList* paletteList) {
    std::vector<std::string> palette;
    
    for (const auto& paletteEntry : paletteList->value) {
        auto entryCompound = std::dynamic_pointer_cast<NbtCompound>(paletteEntry);
        if (!entryCompound) {
            continue;
        }
        
        // 获取方块名称
        std::string blockName = entryCompound->getString("Name", "minecraft:air");
        
        // 提取方块属性（如果存在）
        // 属性用于区分同一方块的不同状态，如楼梯的朝向、门的开关状态等
        auto propertiesCompound = entryCompound->getTag<NbtCompound>("Properties");
        if (propertiesCompound && !propertiesCompound->tags.empty()) {
            // 将属性格式化为 [key1=value1,key2=value2,...] 格式
            blockName += "[";
            bool first = true;
            for (const auto& prop : propertiesCompound->tags) {
                if (!first) {
                    blockName += ",";
                }
                first = false;
                
                auto propString = std::dynamic_pointer_cast<NbtString>(prop.second);
                if (propString) {
                    blockName += prop.first + "=" + propString->value;
                }
            }
            blockName += "]";
        }
        
        palette.push_back(blockName);
    }
    
    return palette;
}

/**
 * @brief 从 NBT List 中提取生物群系调色板
 * 
 * 生物群系调色板与方块调色板不同：
 * - 方块调色板的元素是 Compound（包含 Name 和 Properties）
 * - 生物群系调色板的元素是 String（直接是生物群系 ID）
 * 
 * 示例：["minecraft:plains", "minecraft:forest", "minecraft:river"]
 * 
 * @param paletteList 生物群系调色板 NBT List
 * @return 生物群系名称数组
 */
std::vector<std::string> ChunkDecoder::extractBiomePalette(const NbtList* paletteList) {
    std::vector<std::string> palette;
    
    for (const auto& paletteEntry : paletteList->value) {
        // 生物群系调色板的元素是 NbtString，而不是 NbtCompound
        auto entryString = std::dynamic_pointer_cast<NbtString>(paletteEntry);
        if (entryString) {
            palette.push_back(entryString->value);
        }
    }
    
    return palette;
}

// ============================================================================
// 实体提取函数
// ============================================================================

/**
 * @brief 从 NBT Compound 中提取方块实体（shared_ptr 版本）
 * @param nbtCompound NBT 根节点
 * @return 方块实体数组
 */
std::vector<BlockEntity> ChunkDecoder::extractBlockEntities(const std::shared_ptr<NbtCompound>& nbtCompound) {
    return extractBlockEntitiesFromPtr(nbtCompound.get());
}

/**
 * @brief 从 NBT Compound 中提取方块实体
 * 
 * 方块实体（Block Entity / Tile Entity）是需要存储额外数据的特殊方块，
 * 如箱子（存储物品）、告示牌（存储文字）、熔炉（存储燃烧进度）等。
 * 
 * block_entities 结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ block_entities: list                                        │
 * │ └── [0..N]: compound                                        │
 * │     ├── id: string (如 "minecraft:chest")                   │
 * │     ├── x: int (方块 X 坐标)                                │
 * │     ├── y: int (方块 Y 坐标)                                │
 * │     ├── z: int (方块 Z 坐标)                                │
 * │     └── ... (其他特定于方块类型的数据)                      │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param nbtCompound NBT Compound 指针
 * @return 方块实体数组
 */
std::vector<BlockEntity> ChunkDecoder::extractBlockEntitiesFromPtr(const NbtCompound* nbtCompound) {
    std::vector<BlockEntity> blockEntities;
    
    if (!nbtCompound) {
        return blockEntities;
    }
    
    // 获取 block_entities 列表
    auto blockEntitiesList = nbtCompound->getTag<NbtList>("block_entities");
    if (!blockEntitiesList) {
        return blockEntities;
    }
    
    // 遍历每个方块实体
    for (const auto& entityTag : blockEntitiesList->value) {
        auto entityCompound = std::dynamic_pointer_cast<NbtCompound>(entityTag);
        if (!entityCompound) {
            continue;
        }
        
        BlockEntity entity;
        entity.id = entityCompound->getString("id", "");  // 方块实体类型
        entity.x = entityCompound->getInt("x", 0);        // X 坐标
        entity.y = entityCompound->getInt("y", 0);        // Y 坐标
        entity.z = entityCompound->getInt("z", 0);        // Z 坐标
        entity.data = entityCompound;                      // 保存完整数据以供后续使用
        
        blockEntities.push_back(entity);
    }
    
    return blockEntities;
}

/**
 * @brief 从 NBT Compound 中提取实体（shared_ptr 版本）
 * @param nbtCompound NBT 根节点
 * @return 实体数组
 */
std::vector<Entity> ChunkDecoder::extractEntities(const std::shared_ptr<NbtCompound>& nbtCompound) {
    return extractEntitiesFromPtr(nbtCompound.get());
}

/**
 * @brief 从 NBT Compound 中提取实体
 * 
 * 实体（Entity）是游戏中的动态对象，包括生物、掉落物、矿车等。
 * 注意：从 Minecraft 1.17 开始，实体数据可能存储在单独的文件中。
 * 
 * entities 结构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ entities: list                                              │
 * │ └── [0..N]: compound                                        │
 * │     ├── id: string (如 "minecraft:pig")                     │
 * │     ├── Pos: list [double, double, double] (位置)           │
 * │     ├── Motion: list [double, double, double] (速度)        │
 * │     ├── Rotation: list [float, float] (旋转)                │
 * │     └── ... (其他特定于实体类型的数据)                      │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @param nbtCompound NBT Compound 指针
 * @return 实体数组
 */
std::vector<Entity> ChunkDecoder::extractEntitiesFromPtr(const NbtCompound* nbtCompound) {
    std::vector<Entity> entities;
    
    if (!nbtCompound) {
        return entities;
    }
    
    // 获取 entities 列表
    auto entitiesList = nbtCompound->getTag<NbtList>("entities");
    if (!entitiesList) {
        return entities;
    }
    
    // 遍历每个实体
    for (const auto& entityTag : entitiesList->value) {
        auto entityCompound = std::dynamic_pointer_cast<NbtCompound>(entityTag);
        if (!entityCompound) {
            continue;
        }
        
        Entity entity;
        entity.id = entityCompound->getString("id", "");  // 实体类型
        
        // 提取位置（Pos 是一个包含 3 个 double 的列表）
        auto posList = entityCompound->getTag<NbtList>("Pos");
        if (posList && posList->value.size() >= 3) {
            for (size_t i = 0; i < 3; i++) {
                auto doubleTag = std::dynamic_pointer_cast<NbtDouble>(posList->value[i]);
                if (doubleTag) {
                    entity.pos.push_back(doubleTag->value);
                }
            }
        }
        
        entity.data = entityCompound;  // 保存完整数据以供后续使用
        entities.push_back(entity);
    }
    
    return entities;
}

// ============================================================================
// 旧版本格式转换
// ============================================================================

/**
 * @brief 转换旧版本 Section 格式（Minecraft 1.12 及之前）
 * 
 * 旧版本格式与新版本的区别：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 旧版本（1.12 及之前）：                                     │
 * │ ├── Blocks: byte_array[4096] (方块 ID，0-255)               │
 * │ └── Data: byte_array[2048] (方块数据值，每个 4 位)          │
 * │                                                             │
 * │ 新版本（1.13+）：                                           │
 * │ └── block_states: compound                                  │
 * │     ├── palette: list (方块调色板)                          │
 * │     └── data: long_array (压缩的索引)                       │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * 转换过程：
 * 1. 读取 Blocks 和 Data 数组
 * 2. 组合 Block ID 和 Data 值，构建唯一方块列表（调色板）
 * 3. 将方块 ID 映射为方块名称（使用 ids.json）
 * 4. 将索引打包为新版本的 long_array 格式
 * 
 * @param sectionCompound Section NBT Compound
 * @return 转换后的 BlockStates 对象
 */
BlockStates ChunkDecoder::convertLegacySection(const NbtCompound* sectionCompound) {
    BlockStates blockStates;
    
    if (!sectionCompound) {
        return blockStates;
    }
    
    // ========== 读取旧版本数据 ==========
    // Blocks: 4096 字节，每个字节是一个方块 ID (0-255)
    auto blocksArray = sectionCompound->getTag<NbtByteArray>("Blocks");
    // Data: 2048 字节，每个字节存储两个方块的数据值（每个 4 位）
    auto dataArray = sectionCompound->getTag<NbtByteArray>("Data");
    
    if (!blocksArray || blocksArray->value.size() != 4096) {
        Logger::warning("Legacy section missing Blocks array or invalid size");
        return blockStates;
    }
    
    const auto& blocks = blocksArray->value;
    const auto* data = dataArray ? &dataArray->value : nullptr;
    
    // ========== 构建调色板 ==========
    // 遍历所有方块，收集唯一的 (Block ID, Data) 组合
    std::map<uint16_t, std::string> blockIdToName;
    std::vector<uint16_t> blockIndices(4096);
    
    for (int i = 0; i < 4096; i++) {
        uint8_t blockId = static_cast<uint8_t>(blocks[i]);
        uint8_t blockData = 0;
        
        // 提取 nibble 数据（每个字节存储两个 4 位值）
        // 偶数索引取低 4 位，奇数索引取高 4 位
        if (data && i < static_cast<int>(data->size() * 2)) {
            if (i % 2 == 0) {
                blockData = (*data)[i / 2] & 0x0F;        // 低 4 位
            } else {
                blockData = ((*data)[i / 2] >> 4) & 0x0F; // 高 4 位
            }
        }
        
        // 组合 Block ID 和 Data 为唯一标识
        // 高 8 位是 Block ID，低 4 位是 Data
        uint16_t combinedId = (static_cast<uint16_t>(blockId) << 4) | blockData;
        blockIndices[i] = combinedId;
        
        // 如果这个组合 ID 还没有映射，创建映射
        if (blockIdToName.find(combinedId) == blockIdToName.end()) {
            blockIdToName[combinedId] = legacyBlockIdToName(blockId, blockData);
        }
    }
    
    // ========== 构建调色板列表 ==========
    std::vector<std::string> palette;
    std::map<uint16_t, int> combinedIdToPaletteIndex;
    
    for (const auto& pair : blockIdToName) {
        combinedIdToPaletteIndex[pair.first] = palette.size();
        palette.push_back(pair.second);
    }
    
    // ========== 转换为新版本的 long_array 格式 ==========
    int bitsPerIndex = calculateBitsPerBlock(palette.size());
    int indicesPerLong = 64 / bitsPerIndex;  // 每个 int64 能存储的索引数
    int longCount = (4096 + indicesPerLong - 1) / indicesPerLong;  // 需要的 int64 数量
    std::vector<int64_t> longData(longCount, 0);
    
    // 将索引打包到 int64 数组中
    for (int i = 0; i < 4096; i++) {
        uint16_t combinedId = blockIndices[i];
        int paletteIndex = combinedIdToPaletteIndex[combinedId];
        
        int longIndex = i / indicesPerLong;
        int bitOffset = (i % indicesPerLong) * bitsPerIndex;
        
        longData[longIndex] |= (static_cast<int64_t>(paletteIndex) << bitOffset);
    }
    
    blockStates.palette = palette;
    blockStates.data = longData;
    
    Logger::info("Converted legacy section with " + std::to_string(palette.size()) + " unique blocks");
    
    return blockStates;
}

// ============================================================================
// 旧版本方块 ID 映射
// ============================================================================

/**
 * @brief 获取旧版本方块 ID 映射文件的路径
 * 
 * ids.json 文件格式：
 * {
 *   "0": "minecraft:air",
 *   "1": "minecraft:stone",
 *   "1:1": "minecraft:granite",
 *   "1:2": "minecraft:polished_granite",
 *   ...
 * }
 * 
 * @return JSON 文件路径
 */
std::string ChunkDecoder::getLegacyIdsPath() {
    // 尝试多个可能的路径
    std::vector<std::string> possiblePaths = {
        "ids.json"
    };
    
    for (const auto& path : possiblePaths) {
        std::ifstream file(path);
        if (file.good()) {
            return path;
        }
    }
    
    // 默认返回第一个路径
    return possiblePaths[0];
}

/**
 * @brief 加载旧版本方块 ID 映射表
 * 
 * 从 ids.json 文件加载方块 ID 到方块名称的映射。
 * 映射表格式：
 * - "blockId" → "minecraft:block_name"（无数据值）
 * - "blockId:data" → "minecraft:block_name"（有数据值）
 * 
 * 例如：
 * - "0" → "minecraft:air"
 * - "1" → "minecraft:stone"
 * - "1:1" → "minecraft:granite"
 * 
 * @return 是否成功加载
 */
bool ChunkDecoder::loadLegacyBlockIdMapping() {
    // 如果已经加载过，直接返回
    if (s_mappingLoaded) {
        return true;
    }
    
    try {
        std::string legacyIdsPath = getLegacyIdsPath();
        std::ifstream file(legacyIdsPath);
        
        if (!file.is_open()) {
            Logger::warning("Legacy IDs file not found at: " + legacyIdsPath);
            return false;
        }
        
        // 读取整个文件内容
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonContent = buffer.str();
        file.close();
        
        // 简单的 JSON 解析（不依赖第三方库）
        s_blockIdToNameMapping.clear();
        
        // 移除首尾的大括号
        size_t start = jsonContent.find('{');
        size_t end = jsonContent.rfind('}');
        if (start == std::string::npos || end == std::string::npos) {
            Logger::error("Invalid JSON format in legacy IDs file");
            return false;
        }
        
        jsonContent = jsonContent.substr(start + 1, end - start - 1);
        
        // 逐字符解析 JSON 键值对
        size_t i = 0;
        while (i < jsonContent.length()) {
            // 跳过空白字符和逗号
            while (i < jsonContent.length() && (std::isspace(jsonContent[i]) || jsonContent[i] == ',')) {
                i++;
            }
            
            if (i >= jsonContent.length()) break;
            
            // 解析 key（以引号开始）
            if (jsonContent[i] != '"') break;
            i++;  // 跳过开始的引号
            
            size_t keyStart = i;
            while (i < jsonContent.length() && jsonContent[i] != '"') {
                i++;
            }
            
            if (i >= jsonContent.length()) break;
            std::string key = jsonContent.substr(keyStart, i - keyStart);
            i++;  // 跳过结束的引号
            
            // 跳过冒号和空白字符
            while (i < jsonContent.length() && (std::isspace(jsonContent[i]) || jsonContent[i] == ':')) {
                i++;
            }
            
            if (i >= jsonContent.length()) break;
            
            // 解析 value（以引号开始）
            if (jsonContent[i] != '"') break;
            i++;  // 跳过开始的引号
            
            size_t valueStart = i;
            while (i < jsonContent.length() && jsonContent[i] != '"') {
                i++;
            }
            
            if (i >= jsonContent.length()) break;
            std::string value = jsonContent.substr(valueStart, i - valueStart);
            i++;  // 跳过结束的引号
            
            // 添加到映射表
            s_blockIdToNameMapping[key] = value;
        }
        
        s_mappingLoaded = true;
        Logger::info("Successfully loaded " + std::to_string(s_blockIdToNameMapping.size()) + 
                    " legacy block ID mappings from JSON");
        return s_blockIdToNameMapping.size() > 0;
        
    } catch (const std::exception& ex) {
        Logger::error("Error loading legacy IDs mapping from JSON: " + std::string(ex.what()));
        s_mappingLoaded = false;
        return false;
    }
}

/**
 * @brief 将旧版本方块 ID 和数据值转换为方块名称
 * 
 * 查找顺序：
 * 1. 先尝试 "blockId:blockData" 格式（精确匹配）
 * 2. 再尝试 "blockId" 格式（忽略数据值）
 * 3. 如果都找不到，返回默认名称
 * 
 * @param blockId 方块 ID (0-255)
 * @param blockData 方块数据值 (0-15)
 * @return 方块名称字符串
 */
std::string ChunkDecoder::legacyBlockIdToName(uint8_t blockId, uint8_t blockData) {
    // 首先尝试加载映射表
    if (loadLegacyBlockIdMapping()) {
        // 尝试查找 "blockId:blockData" 格式的映射（精确匹配）
        std::string keyWithData = std::to_string(blockId) + ":" + std::to_string(blockData);
        auto it = s_blockIdToNameMapping.find(keyWithData);
        if (it != s_blockIdToNameMapping.end()) {
            return it->second;
        }
        
        // 如果没找到，尝试只用 blockId 查找（忽略数据值）
        std::string keyWithoutData = std::to_string(blockId);
        it = s_blockIdToNameMapping.find(keyWithoutData);
        if (it != s_blockIdToNameMapping.end()) {
            return it->second;
        }
    }
    
    // 如果映射表中没有找到，使用默认的回退名称
    return "minecraft:legacy_block_" + std::to_string(blockId) + "_" + std::to_string(blockData);
}

} // namespace MCATool
