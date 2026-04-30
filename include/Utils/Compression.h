#ifndef MCATOOL_COMPRESSION_H
#define MCATOOL_COMPRESSION_H

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace MCATool {

class Compression {
public:
    /**
     * @brief 解压 Zlib 压缩的数据
     * @param compressedData 压缩的数据
     * @return 解压后的数据
     */
    static std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& compressedData);
    
    /**
     * @brief 解压 GZip 压缩的数据
     * @param compressedData 压缩的数据
     * @return 解压后的数据
     */
    static std::vector<uint8_t> decompressGZip(const std::vector<uint8_t>& compressedData);
    
    /**
     * @brief 根据压缩类型解压数据
     * @param compressedData 压缩的数据
     * @param compressionType 压缩类型（1=GZip, 2=Zlib, 3=Uncompressed）
     * @return 解压后的数据
     */
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData, uint8_t compressionType);
};

} // namespace MCATool

#endif // MCATOOL_COMPRESSION_H
