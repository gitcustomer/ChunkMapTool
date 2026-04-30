#include "Utils/Compression.h"
#include "Utils/Logger.h"
#include <zlib.h>
#include <cstring>

namespace MCATool {

std::vector<uint8_t> Compression::decompressZlib(const std::vector<uint8_t>& compressedData) {
    if (compressedData.empty()) {
        throw std::runtime_error("Empty compressed data");
    }
    
    // 初始化 zlib 流
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    
    // 初始化解压缩器（Zlib 格式）
    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib decompression");
    }
    
    stream.avail_in = static_cast<uInt>(compressedData.size());
    stream.next_in = const_cast<Bytef*>(compressedData.data());
    
    // 预分配输出缓冲区（假设解压后大小为压缩数据的 10 倍）
    std::vector<uint8_t> decompressedData;
    decompressedData.reserve(compressedData.size() * 10);
    
    const size_t CHUNK_SIZE = 16384; // 16KB
    std::vector<uint8_t> tempBuffer(CHUNK_SIZE);
    
    int ret;
    do {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = tempBuffer.data();
        
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("Zlib decompression failed: " + std::string(stream.msg ? stream.msg : "Unknown error"));
        }
        
        size_t have = CHUNK_SIZE - stream.avail_out;
        decompressedData.insert(decompressedData.end(), tempBuffer.begin(), tempBuffer.begin() + have);
        
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&stream);
    
    Logger::debug("Zlib decompression: " + std::to_string(compressedData.size()) + 
                  " bytes -> " + std::to_string(decompressedData.size()) + " bytes");
    
    return decompressedData;
}

std::vector<uint8_t> Compression::decompressGZip(const std::vector<uint8_t>& compressedData) {
    if (compressedData.empty()) {
        throw std::runtime_error("Empty compressed data");
    }
    
    // 初始化 zlib 流
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    
    // 初始化解压缩器（GZip 格式，使用 windowBits + 16）
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        throw std::runtime_error("Failed to initialize gzip decompression");
    }
    
    stream.avail_in = static_cast<uInt>(compressedData.size());
    stream.next_in = const_cast<Bytef*>(compressedData.data());
    
    // 预分配输出缓冲区
    std::vector<uint8_t> decompressedData;
    decompressedData.reserve(compressedData.size() * 10);
    
    const size_t CHUNK_SIZE = 16384; // 16KB
    std::vector<uint8_t> tempBuffer(CHUNK_SIZE);
    
    int ret;
    do {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = tempBuffer.data();
        
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("GZip decompression failed: " + std::string(stream.msg ? stream.msg : "Unknown error"));
        }
        
        size_t have = CHUNK_SIZE - stream.avail_out;
        decompressedData.insert(decompressedData.end(), tempBuffer.begin(), tempBuffer.begin() + have);
        
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&stream);
    
    Logger::debug("GZip decompression: " + std::to_string(compressedData.size()) + 
                  " bytes -> " + std::to_string(decompressedData.size()) + " bytes");
    
    return decompressedData;
}

std::vector<uint8_t> Compression::decompress(const std::vector<uint8_t>& compressedData, uint8_t compressionType) {
    switch (compressionType) {
        case 1: // GZip
            return decompressGZip(compressedData);
        case 2: // Zlib
            return decompressZlib(compressedData);
        case 3: // Uncompressed
            return compressedData;
        default:
            throw std::runtime_error("Unknown compression type: " + std::to_string(compressionType));
    }
}

} // namespace MCATool
