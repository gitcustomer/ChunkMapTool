#ifndef MCATOOL_NBTPARSER_H
#define MCATOOL_NBTPARSER_H

#include "Core/DataStructures.h"
#include <vector>
#include <memory>
#include <istream>

namespace MCATool {

/**
 * @brief NBT 格式解析器
 * 
 * 支持解析 Minecraft NBT (Named Binary Tag) 格式
 * 参考: https://minecraft.wiki/w/NBT_format
 */
class NbtParser {
public:
    /**
     * @brief 从字节数组解析 NBT 数据
     * @param data NBT 二进制数据
     * @return 解析后的根 Compound 标签
     */
    static std::shared_ptr<NbtCompound> parse(const std::vector<uint8_t>& data);
    
    /**
     * @brief 从输入流解析 NBT 数据
     * @param stream 输入流
     * @return 解析后的根 Compound 标签
     */
    static std::shared_ptr<NbtCompound> parse(std::istream& stream);

private:
    /**
     * @brief 读取标签（递归）
     * @param stream 输入流
     * @param tagType 标签类型
     * @return 解析后的标签
     */
    static std::shared_ptr<NbtTag> readTag(std::istream& stream, NbtTagType tagType);
    
    /**
     * @brief 读取命名标签（包含名称）
     * @param stream 输入流
     * @param tagType 标签类型
     * @param name 输出参数：标签名称
     * @return 解析后的标签
     */
    static std::shared_ptr<NbtTag> readNamedTag(std::istream& stream, NbtTagType tagType, std::string& name);
    
    /**
     * @brief 读取字符串
     * @param stream 输入流
     * @return 字符串
     */
    static std::string readString(std::istream& stream);
    
    /**
     * @brief 读取大端序的 int16
     */
    static int16_t readInt16(std::istream& stream);
    
    /**
     * @brief 读取大端序的 int32
     */
    static int32_t readInt32(std::istream& stream);
    
    /**
     * @brief 读取大端序的 int64
     */
    static int64_t readInt64(std::istream& stream);
    
    /**
     * @brief 读取大端序的 float
     */
    static float readFloat(std::istream& stream);
    
    /**
     * @brief 读取大端序的 double
     */
    static double readDouble(std::istream& stream);
};

} // namespace MCATool

#endif // MCATOOL_NBTPARSER_H
