#ifndef MCATOOL_DATASTRUCTURES_H
#define MCATOOL_DATASTRUCTURES_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <array>

namespace MCATool {

// ============================================================================
// NBT 数据结构
// ============================================================================

enum class NbtTagType : uint8_t {
    TAG_End = 0,
    TAG_Byte = 1,
    TAG_Short = 2,
    TAG_Int = 3,
    TAG_Long = 4,
    TAG_Float = 5,
    TAG_Double = 6,
    TAG_Byte_Array = 7,
    TAG_String = 8,
    TAG_List = 9,
    TAG_Compound = 10,
    TAG_Int_Array = 11,
    TAG_Long_Array = 12
};

// NBT 标签基类
struct NbtTag {
    NbtTagType type;
    virtual ~NbtTag() = default;
};

// NBT 基本类型
struct NbtByte : public NbtTag {
    int8_t value;
    NbtByte() { type = NbtTagType::TAG_Byte; }
};

struct NbtShort : public NbtTag {
    int16_t value;
    NbtShort() { type = NbtTagType::TAG_Short; }
};

struct NbtInt : public NbtTag {
    int32_t value;
    NbtInt() { type = NbtTagType::TAG_Int; }
};

struct NbtLong : public NbtTag {
    int64_t value;
    NbtLong() { type = NbtTagType::TAG_Long; }
};

struct NbtFloat : public NbtTag {
    float value;
    NbtFloat() { type = NbtTagType::TAG_Float; }
};

struct NbtDouble : public NbtTag {
    double value;
    NbtDouble() { type = NbtTagType::TAG_Double; }
};

struct NbtString : public NbtTag {
    std::string value;
    NbtString() { type = NbtTagType::TAG_String; }
};

// NBT 数组类型
struct NbtByteArray : public NbtTag {
    std::vector<int8_t> value;
    NbtByteArray() { type = NbtTagType::TAG_Byte_Array; }
};

struct NbtIntArray : public NbtTag {
    std::vector<int32_t> value;
    NbtIntArray() { type = NbtTagType::TAG_Int_Array; }
};

struct NbtLongArray : public NbtTag {
    std::vector<int64_t> value;
    NbtLongArray() { type = NbtTagType::TAG_Long_Array; }
};

// NBT 列表类型
struct NbtList : public NbtTag {
    NbtTagType elementType;
    std::vector<std::shared_ptr<NbtTag>> value;
    NbtList() { type = NbtTagType::TAG_List; }
};

// NBT 复合类型
struct NbtCompound : public NbtTag {
    std::map<std::string, std::shared_ptr<NbtTag>> tags;
    NbtCompound() { type = NbtTagType::TAG_Compound; }
    
    // 辅助方法
    bool hasTag(const std::string& name) const {
        return tags.find(name) != tags.end();
    }
    
    template<typename T>
    T* getTag(const std::string& name) {
        auto it = tags.find(name);
        if (it != tags.end()) {
            return dynamic_cast<T*>(it->second.get());
        }
        return nullptr;
    }
    
    template<typename T>
    const T* getTag(const std::string& name) const {
        auto it = tags.find(name);
        if (it != tags.end()) {
            return dynamic_cast<const T*>(it->second.get());
        }
        return nullptr;
    }
    
    int32_t getInt(const std::string& name, int32_t defaultValue = 0) const {
        auto it = tags.find(name);
        if (it != tags.end() && it->second->type == NbtTagType::TAG_Int) {
            return static_cast<NbtInt*>(it->second.get())->value;
        }
        return defaultValue;
    }
    
    std::string getString(const std::string& name, const std::string& defaultValue = "") const {
        auto it = tags.find(name);
        if (it != tags.end() && it->second->type == NbtTagType::TAG_String) {
            return static_cast<NbtString*>(it->second.get())->value;
        }
        return defaultValue;
    }
};

// ============================================================================
// MCA 文件数据结构
// ============================================================================

// Chunk 压缩类型
enum class CompressionType : uint8_t {
    GZip = 1,
    Zlib = 2,
    Uncompressed = 3
};

// Chunk 数据
struct ChunkData {
    int32_t chunkX;
    int32_t chunkZ;
    CompressionType compressionType;
    std::vector<uint8_t> nbtData;
    bool isEmpty;
    
    ChunkData() : chunkX(0), chunkZ(0), compressionType(CompressionType::Zlib), isEmpty(true) {}
};

// Region 数据
struct Region {
    int32_t regionX;
    int32_t regionZ;
    std::vector<ChunkData> chunks;
    
    Region() : regionX(0), regionZ(0) {}
};

// ============================================================================
// Paletted Chunk 数据结构
// ============================================================================

// Section 中的方块状态数据
struct BlockStates {
    std::vector<std::string> palette;  // 方块调色板
    std::vector<int64_t> data;         // 压缩的索引数据
};

// 生物群系数据
struct BiomeData {
    std::vector<std::string> palette;  // 生物群系调色板
    std::vector<int64_t> data;         // 压缩的索引数据
};

// Section（16x16x16 的子区块）
struct Section {
    int32_t sectionY;                  // Section Y 坐标
    BlockStates blockStates;           // 方块状态数据
    BiomeData biomes;                  // 生物群系数据（可选）
    
    Section() : sectionY(0) {}
};

// 方块实体（箱子、告示牌等）
struct BlockEntity {
    std::string id;                    // 方块实体类型
    int32_t x, y, z;                   // 位置
    std::shared_ptr<NbtCompound> data; // 额外数据
    
    BlockEntity() : x(0), y(0), z(0) {}
};

// 实体（生物、物品等）
struct Entity {
    std::string id;                    // 实体类型
    std::vector<double> pos;           // 位置 [x, y, z]
    std::shared_ptr<NbtCompound> data; // 额外数据
};

// Paletted Chunk（调色板压缩的 Chunk）
struct PalettedChunk {
    int32_t chunkX;
    int32_t chunkZ;
    int32_t dataVersion;               // 数据版本
    std::string status;                // 生成状态
    std::vector<Section> sections;     // Section 数组
    std::vector<BlockEntity> blockEntities;
    std::vector<Entity> entities;
    
    PalettedChunk() : chunkX(0), chunkZ(0), dataVersion(0) {}
};

// ============================================================================
// Decoded Chunk 数据结构
// ============================================================================

// 解码后的 Section（包含完整的方块名称数组）
struct DecodedSection {
    int32_t sectionY;
    std::array<std::string, 4096> blockNames;  // 16x16x16 = 4096
    std::array<std::string, 64> biomeNames;    // 4x4x4 = 64（生物群系）
    
    DecodedSection() : sectionY(0) {
        blockNames.fill("minecraft:air");
        biomeNames.fill("minecraft:plains");
    }
};

// 解码后的 Chunk
struct DecodedChunk {
    int32_t chunkX;
    int32_t chunkZ;
    std::vector<DecodedSection> sections;
    std::vector<BlockEntity> blockEntities;
    std::vector<Entity> entities;
    
    DecodedChunk() : chunkX(0), chunkZ(0) {}
};

// ============================================================================
// 辅助结构
// ============================================================================

// 3D 坐标
struct Vec3i {
    int32_t x, y, z;
    
    Vec3i() : x(0), y(0), z(0) {}
    Vec3i(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    
    bool operator==(const Vec3i& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// 2D 坐标（用于 Chunk 索引）
struct Vec2i {
    int32_t x, z;
    
    Vec2i() : x(0), z(0) {}
    Vec2i(int32_t x, int32_t z) : x(x), z(z) {}
    
    bool operator<(const Vec2i& other) const {
        if (x != other.x) return x < other.x;
        return z < other.z;
    }
    
    bool operator==(const Vec2i& other) const {
        return x == other.x && z == other.z;
    }
};

} // namespace MCATool

#endif // MCATOOL_DATASTRUCTURES_H
