/**
 * @file NbtParser.cpp
 * @brief NBT (Named Binary Tag) 格式解析器实现
 * 
 * NBT 是 Minecraft 使用的二进制数据存储格式，类似于二进制版的 JSON。
 * 它用于存储游戏中的各种数据，如区块、玩家数据、物品等。
 * 
 * NBT 格式特点：
 * 1. 二进制格式，紧凑高效
 * 2. 使用大端序（Big Endian）存储多字节数值
 * 3. 强类型系统，区分 Byte/Short/Int/Long/Float/Double
 * 4. 树形结构，支持嵌套的 Compound 和 List
 * 
 * 参考文档：https://minecraft.wiki/w/NBT_format
 */

#include "Core/NbtParser.h"
#include "Utils/Logger.h"
#include <sstream>
#include <cstring>
#include <stdexcept>

namespace MCATool {

// ============================================================================
// 字节序转换辅助函数
// ============================================================================
// 
// 为什么需要字节序转换？
// - Minecraft/NBT 使用大端序（Big Endian）：高位字节在前
// - 大多数现代 CPU（x86/x64/ARM）使用小端序（Little Endian）：低位字节在前
// - 读取 NBT 数据时，必须将大端序转换为本机字节序
//
// 示例：数值 0x12345678 的存储方式
// - 大端序（NBT 文件中）：12 34 56 78
// - 小端序（x86 内存中）：78 56 34 12
// ============================================================================

/**
 * @brief 16 位整数字节序交换（大端序 ↔ 小端序）
 * 
 * 原理：交换两个字节的位置
 * 输入：0xAABB → 输出：0xBBAA
 * 
 * @param value 原始值
 * @return 字节序交换后的值
 */
static inline int16_t swapInt16(int16_t value) {
    return ((value & 0xFF) << 8) |      // 低 8 位移到高位
           ((value >> 8) & 0xFF);        // 高 8 位移到低位
}

/**
 * @brief 32 位整数字节序交换（大端序 ↔ 小端序）
 * 
 * 原理：将 4 个字节完全反转
 * 输入：0xAABBCCDD → 输出：0xDDCCBBAA
 * 
 * @param value 原始值
 * @return 字节序交换后的值
 */
static inline int32_t swapInt32(int32_t value) {
    return ((value & 0xFF) << 24) |       // 第 0 字节 → 第 3 字节
           ((value & 0xFF00) << 8) |      // 第 1 字节 → 第 2 字节
           ((value >> 8) & 0xFF00) |      // 第 2 字节 → 第 1 字节
           ((value >> 24) & 0xFF);        // 第 3 字节 → 第 0 字节
}

/**
 * @brief 64 位整数字节序交换（大端序 ↔ 小端序）
 * 
 * 原理：将 8 个字节完全反转
 * 
 * @param value 原始值
 * @return 字节序交换后的值
 */
static inline int64_t swapInt64(int64_t value) {
    return ((value & 0xFFLL) << 56) |           // 第 0 字节 → 第 7 字节
           ((value & 0xFF00LL) << 40) |         // 第 1 字节 → 第 6 字节
           ((value & 0xFF0000LL) << 24) |       // 第 2 字节 → 第 5 字节
           ((value & 0xFF000000LL) << 8) |      // 第 3 字节 → 第 4 字节
           ((value >> 8) & 0xFF000000LL) |      // 第 4 字节 → 第 3 字节
           ((value >> 24) & 0xFF0000LL) |       // 第 5 字节 → 第 2 字节
           ((value >> 40) & 0xFF00LL) |         // 第 6 字节 → 第 1 字节
           ((value >> 56) & 0xFFLL);            // 第 7 字节 → 第 0 字节
}

/**
 * @brief 单精度浮点数字节序交换
 * 
 * 原理：将 float 的 4 个字节当作 uint32 进行交换
 * 使用 memcpy 避免类型双关（type punning）的未定义行为
 * 
 * @param value 原始浮点值
 * @return 字节序交换后的浮点值
 */
static inline float swapFloat(float value) {
    uint32_t temp;
    std::memcpy(&temp, &value, sizeof(float));  // float → uint32（按位复制）
    temp = swapInt32(temp);                      // 交换字节序
    float result;
    std::memcpy(&result, &temp, sizeof(float)); // uint32 → float（按位复制）
    return result;
}

/**
 * @brief 双精度浮点数字节序交换
 * 
 * 原理：将 double 的 8 个字节当作 uint64 进行交换
 * 
 * @param value 原始双精度浮点值
 * @return 字节序交换后的双精度浮点值
 */
static inline double swapDouble(double value) {
    uint64_t temp;
    std::memcpy(&temp, &value, sizeof(double)); // double → uint64（按位复制）
    temp = swapInt64(temp);                      // 交换字节序
    double result;
    std::memcpy(&result, &temp, sizeof(double)); // uint64 → double（按位复制）
    return result;
}

// ============================================================================
// NBT 解析入口函数
// ============================================================================

/**
 * @brief 从字节数组解析 NBT 数据
 * 
 * @param data NBT 二进制数据（通常是解压后的 Chunk 数据）
 * @return 解析后的根 Compound 标签
 */
std::shared_ptr<NbtCompound> NbtParser::parse(const std::vector<uint8_t>& data) {
    // 将字节数组转换为输入流，方便逐字节读取
    std::istringstream stream(std::string(data.begin(), data.end()));
    return parse(stream);
}

/**
 * @brief 从输入流解析 NBT 数据（核心解析函数）
 * 
 * NBT 文件结构：
 * ┌─────────────────────────────────────┐
 * │ 1 字节：根标签类型（必须是 0x0A）    │  ← TAG_Compound
 * │ 2 字节：根标签名称长度（大端序）     │
 * │ N 字节：根标签名称（UTF-8）          │
 * │ ... 根 Compound 的内容 ...          │
 * │ 1 字节：TAG_End (0x00)              │  ← 结束标记
 * └─────────────────────────────────────┘
 * 
 * @param stream 输入流
 * @return 解析后的根 Compound 标签
 * @throws std::runtime_error 如果格式不正确
 */
std::shared_ptr<NbtCompound> NbtParser::parse(std::istream& stream) {
    // 步骤 1：读取根标签类型（1 字节）
    // NBT 规范要求根标签必须是 TAG_Compound (0x0A)
    uint8_t rootType;
    stream.read(reinterpret_cast<char*>(&rootType), 1);
    
    if (rootType != static_cast<uint8_t>(NbtTagType::TAG_Compound)) {
        throw std::runtime_error("Root tag must be TAG_Compound, got: " + std::to_string(rootType));
    }
    
    // 步骤 2：读取根标签名称
    // 格式：2 字节长度（大端序）+ N 字节 UTF-8 字符串
    // 对于 Chunk 数据，根标签名称通常为空字符串
    std::string rootName = readString(stream);
    
    // 步骤 3：递归读取根 Compound 的内容
    auto rootTag = readTag(stream, NbtTagType::TAG_Compound);
    auto rootCompound = std::dynamic_pointer_cast<NbtCompound>(rootTag);
    
    if (!rootCompound) {
        throw std::runtime_error("Failed to parse root compound");
    }
    
    Logger::debug("NBT parsed successfully, root name: " + rootName);
    return rootCompound;
}

// ============================================================================
// NBT 标签读取函数（递归核心）
// ============================================================================

/**
 * @brief 根据标签类型读取标签内容（递归函数）
 * 
 * NBT 支持的标签类型：
 * ┌──────────────────┬──────┬─────────────────────────────────┐
 * │ 类型             │ ID   │ 说明                            │
 * ├──────────────────┼──────┼─────────────────────────────────┤
 * │ TAG_End          │ 0    │ 结束标记，无内容                │
 * │ TAG_Byte         │ 1    │ 1 字节有符号整数                │
 * │ TAG_Short        │ 2    │ 2 字节有符号整数（大端序）      │
 * │ TAG_Int          │ 3    │ 4 字节有符号整数（大端序）      │
 * │ TAG_Long         │ 4    │ 8 字节有符号整数（大端序）      │
 * │ TAG_Float        │ 5    │ 4 字节浮点数（大端序）          │
 * │ TAG_Double       │ 6    │ 8 字节浮点数（大端序）          │
 * │ TAG_Byte_Array   │ 7    │ 字节数组                        │
 * │ TAG_String       │ 8    │ UTF-8 字符串                    │
 * │ TAG_List         │ 9    │ 同类型元素的列表                │
 * │ TAG_Compound     │ 10   │ 键值对集合（类似 JSON 对象）    │
 * │ TAG_Int_Array    │ 11   │ 整数数组                        │
 * │ TAG_Long_Array   │ 12   │ 长整数数组                      │
 * └──────────────────┴──────┴─────────────────────────────────┘
 * 
 * @param stream 输入流
 * @param tagType 标签类型
 * @return 解析后的标签对象
 */
std::shared_ptr<NbtTag> NbtParser::readTag(std::istream& stream, NbtTagType tagType) {
    switch (tagType) {
        // ========== TAG_End (0) ==========
        // 结束标记，用于标识 Compound 的结束
        case NbtTagType::TAG_End:
            return nullptr;
        
        // ========== TAG_Byte (1) ==========
        // 1 字节有符号整数，范围 -128 ~ 127
        // 无需字节序转换（只有 1 个字节）
        case NbtTagType::TAG_Byte: {
            auto tag = std::make_shared<NbtByte>();
            stream.read(reinterpret_cast<char*>(&tag->value), 1);
            return tag;
        }
        
        // ========== TAG_Short (2) ==========
        // 2 字节有符号整数，范围 -32768 ~ 32767
        // 需要从大端序转换为小端序
        case NbtTagType::TAG_Short: {
            auto tag = std::make_shared<NbtShort>();
            tag->value = readInt16(stream);
            return tag;
        }
        
        // ========== TAG_Int (3) ==========
        // 4 字节有符号整数
        // 常用于存储坐标、版本号等
        case NbtTagType::TAG_Int: {
            auto tag = std::make_shared<NbtInt>();
            tag->value = readInt32(stream);
            return tag;
        }
        
        // ========== TAG_Long (4) ==========
        // 8 字节有符号整数
        // 常用于存储时间戳、种子等大数值
        case NbtTagType::TAG_Long: {
            auto tag = std::make_shared<NbtLong>();
            tag->value = readInt64(stream);
            return tag;
        }
        
        // ========== TAG_Float (5) ==========
        // 4 字节单精度浮点数（IEEE 754）
        case NbtTagType::TAG_Float: {
            auto tag = std::make_shared<NbtFloat>();
            tag->value = readFloat(stream);
            return tag;
        }
        
        // ========== TAG_Double (6) ==========
        // 8 字节双精度浮点数（IEEE 754）
        // 常用于存储实体坐标等高精度数值
        case NbtTagType::TAG_Double: {
            auto tag = std::make_shared<NbtDouble>();
            tag->value = readDouble(stream);
            return tag;
        }
        
        // ========== TAG_Byte_Array (7) ==========
        // 字节数组，格式：4 字节长度 + N 字节数据
        // 常用于存储旧版本的方块数据
        case NbtTagType::TAG_Byte_Array: {
            auto tag = std::make_shared<NbtByteArray>();
            int32_t length = readInt32(stream);  // 读取数组长度
            if (length < 0) {
                throw std::runtime_error("Invalid byte array length: " + std::to_string(length));
            }
            tag->value.resize(length);
            stream.read(reinterpret_cast<char*>(tag->value.data()), length);
            return tag;
        }
        
        // ========== TAG_String (8) ==========
        // UTF-8 字符串，格式：2 字节长度 + N 字节字符串
        // 常用于存储方块名称、实体 ID 等
        case NbtTagType::TAG_String: {
            auto tag = std::make_shared<NbtString>();
            tag->value = readString(stream);
            return tag;
        }
        
        // ========== TAG_List (9) ==========
        // 同类型元素的列表
        // 格式：1 字节元素类型 + 4 字节长度 + N 个元素
        // 注意：List 中的元素没有名称，只有值
        case NbtTagType::TAG_List: {
            auto tag = std::make_shared<NbtList>();
            
            // 读取元素类型（1 字节）
            uint8_t elementType;
            stream.read(reinterpret_cast<char*>(&elementType), 1);
            tag->elementType = static_cast<NbtTagType>(elementType);
            
            // 读取列表长度（4 字节，大端序）
            int32_t length = readInt32(stream);
            if (length < 0) {
                throw std::runtime_error("Invalid list length: " + std::to_string(length));
            }
            
            // 递归读取每个元素
            tag->value.reserve(length);
            for (int32_t i = 0; i < length; i++) {
                auto element = readTag(stream, tag->elementType);
                if (element) {
                    tag->value.push_back(element);
                }
            }
            return tag;
        }
        
        // ========== TAG_Compound (10) ==========
        // 键值对集合，类似 JSON 对象
        // 格式：多个命名标签 + TAG_End 结束
        // 每个子标签格式：1 字节类型 + 2 字节名称长度 + 名称 + 值
        case NbtTagType::TAG_Compound: {
            auto tag = std::make_shared<NbtCompound>();
            
            // 循环读取子标签，直到遇到 TAG_End
            while (true) {
                // 读取子标签类型
                uint8_t childType;
                stream.read(reinterpret_cast<char*>(&childType), 1);
                
                // TAG_End 表示 Compound 结束
                if (childType == static_cast<uint8_t>(NbtTagType::TAG_End)) {
                    break;
                }
                
                // 读取子标签（包含名称和值）
                std::string childName;
                auto childTag = readNamedTag(stream, static_cast<NbtTagType>(childType), childName);
                
                if (childTag) {
                    tag->tags[childName] = childTag;
                }
            }
            return tag;
        }
        
        // ========== TAG_Int_Array (11) ==========
        // 整数数组，格式：4 字节长度 + N 个 4 字节整数
        // 常用于存储高度图等数据
        case NbtTagType::TAG_Int_Array: {
            auto tag = std::make_shared<NbtIntArray>();
            int32_t length = readInt32(stream);
            if (length < 0) {
                throw std::runtime_error("Invalid int array length: " + std::to_string(length));
            }
            tag->value.resize(length);
            // 每个整数都需要字节序转换
            for (int32_t i = 0; i < length; i++) {
                tag->value[i] = readInt32(stream);
            }
            return tag;
        }
        
        // ========== TAG_Long_Array (12) ==========
        // 长整数数组，格式：4 字节长度 + N 个 8 字节长整数
        // 常用于存储方块状态的压缩数据（调色板索引）
        case NbtTagType::TAG_Long_Array: {
            auto tag = std::make_shared<NbtLongArray>();
            int32_t length = readInt32(stream);
            if (length < 0) {
                throw std::runtime_error("Invalid long array length: " + std::to_string(length));
            }
            tag->value.resize(length);
            // 每个长整数都需要字节序转换
            for (int32_t i = 0; i < length; i++) {
                tag->value[i] = readInt64(stream);
            }
            return tag;
        }
        
        default:
            throw std::runtime_error("Unknown NBT tag type: " + std::to_string(static_cast<int>(tagType)));
    }
}

// ============================================================================
// 辅助读取函数
// ============================================================================

/**
 * @brief 读取命名标签（先读名称，再读值）
 * 
 * Compound 中的子标签格式：
 * ┌─────────────────────────────────────┐
 * │ 2 字节：名称长度（大端序）          │
 * │ N 字节：名称（UTF-8 字符串）        │
 * │ ... 标签值（根据类型不同而不同）    │
 * └─────────────────────────────────────┘
 * 
 * @param stream 输入流
 * @param tagType 标签类型（已在外部读取）
 * @param name 输出参数：标签名称
 * @return 解析后的标签对象
 */
std::shared_ptr<NbtTag> NbtParser::readNamedTag(std::istream& stream, NbtTagType tagType, std::string& name) {
    name = readString(stream);           // 先读取名称
    return readTag(stream, tagType);     // 再读取值
}

/**
 * @brief 读取 UTF-8 字符串
 * 
 * 字符串格式：
 * ┌─────────────────────────────────────┐
 * │ 2 字节：长度（大端序，无符号）      │
 * │ N 字节：UTF-8 编码的字符串内容      │
 * └─────────────────────────────────────┘
 * 
 * @param stream 输入流
 * @return 解析后的字符串
 */
std::string NbtParser::readString(std::istream& stream) {
    // 读取字符串长度（2 字节，大端序）
    int16_t length = readInt16(stream);
    if (length < 0) {
        throw std::runtime_error("Invalid string length: " + std::to_string(length));
    }
    
    // 空字符串
    if (length == 0) {
        return "";
    }
    
    // 读取字符串内容
    std::vector<char> buffer(length);
    stream.read(buffer.data(), length);
    return std::string(buffer.begin(), buffer.end());
}

// ============================================================================
// 基本类型读取函数（带字节序转换）
// ============================================================================

/**
 * @brief 读取 16 位有符号整数（大端序 → 本机字节序）
 */
int16_t NbtParser::readInt16(std::istream& stream) {
    int16_t value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(int16_t));
    return swapInt16(value);  // 大端序 → 小端序
}

/**
 * @brief 读取 32 位有符号整数（大端序 → 本机字节序）
 */
int32_t NbtParser::readInt32(std::istream& stream) {
    int32_t value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
    return swapInt32(value);  // 大端序 → 小端序
}

/**
 * @brief 读取 64 位有符号整数（大端序 → 本机字节序）
 */
int64_t NbtParser::readInt64(std::istream& stream) {
    int64_t value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(int64_t));
    return swapInt64(value);  // 大端序 → 小端序
}

/**
 * @brief 读取单精度浮点数（大端序 → 本机字节序）
 */
float NbtParser::readFloat(std::istream& stream) {
    float value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(float));
    return swapFloat(value);  // 大端序 → 小端序
}

/**
 * @brief 读取双精度浮点数（大端序 → 本机字节序）
 */
double NbtParser::readDouble(std::istream& stream) {
    double value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(double));
    return swapDouble(value);  // 大端序 → 小端序
}

} // namespace MCATool
