#include "Utils/TextureLoader.h"
#include "Utils/Logger.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <zlib.h>

namespace MCATool {

// 简单的 PNG 解码器（仅支持基本的 PNG 格式）
namespace {
    // PNG 文件头
    const unsigned char PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    
    // 读取大端序 32 位整数
    uint32_t readBigEndian32(const unsigned char* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }
    
    // Paeth 滤波器预测函数
    unsigned char paethPredictor(unsigned char a, unsigned char b, unsigned char c) {
        int p = a + b - c;
        int pa = abs(p - a);
        int pb = abs(p - b);
        int pc = abs(p - c);
        
        if (pa <= pb && pa <= pc) return a;
        if (pb <= pc) return b;
        return c;
    }
    
    // 完整的 PNG 解码器
    bool decodePNG(const std::vector<unsigned char>& fileData, 
                   std::vector<unsigned char>& imageData,
                   int& width, int& height) {
        // 检查 PNG 签名
        if (fileData.size() < 8 || memcmp(fileData.data(), PNG_SIGNATURE, 8) != 0) {
            return false;
        }
        
        size_t pos = 8;
        bool foundIHDR = false;
        unsigned char bitDepth = 0;
        unsigned char colorType = 0;
        std::vector<unsigned char> compressedData;
        std::vector<unsigned char> palette; // 调色板数据（RGB）
        
        // 读取 PNG 块
        while (pos + 12 <= fileData.size()) {
            uint32_t chunkLength = readBigEndian32(&fileData[pos]);
            pos += 4;
            
            if (pos + 4 > fileData.size()) break;
            
            // 读取块类型
            char chunkType[5] = {0};
            memcpy(chunkType, &fileData[pos], 4);
            pos += 4;
            
            if (pos + chunkLength + 4 > fileData.size()) break;
            
            // IHDR 块 - 图像头
            if (strcmp(chunkType, "IHDR") == 0 && chunkLength >= 13) {
                width = readBigEndian32(&fileData[pos]);
                height = readBigEndian32(&fileData[pos + 4]);
                bitDepth = fileData[pos + 8];
                colorType = fileData[pos + 9];
                foundIHDR = true;
            }
            // PLTE 块 - 调色板
            else if (strcmp(chunkType, "PLTE") == 0) {
                palette.insert(palette.end(), 
                              &fileData[pos], 
                              &fileData[pos + chunkLength]);
            }
            // IDAT 块 - 图像数据
            else if (strcmp(chunkType, "IDAT") == 0) {
                compressedData.insert(compressedData.end(), 
                                     &fileData[pos], 
                                     &fileData[pos + chunkLength]);
            }
            // IEND 块 - 结束
            else if (strcmp(chunkType, "IEND") == 0) {
                break;
            }
            
            pos += chunkLength + 4; // 跳过数据和 CRC
        }
        
        if (!foundIHDR || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
            return false;
        }
        
        // 检查索引色是否有调色板
        if (colorType == 3 && palette.empty()) {
            return false;
        }
        
        // 支持 1, 2, 4, 8 位深度
        if (bitDepth != 1 && bitDepth != 2 && bitDepth != 4 && bitDepth != 8) {
            return false;
        }
        
        // 计算解压后的大小
        int bitsPerPixel = 0;
        switch (colorType) {
            case 0: bitsPerPixel = bitDepth; break;           // 灰度
            case 2: bitsPerPixel = bitDepth * 3; break;       // RGB
            case 3: bitsPerPixel = bitDepth; break;           // 索引色
            case 4: bitsPerPixel = bitDepth * 2; break;       // 灰度 + Alpha
            case 6: bitsPerPixel = bitDepth * 4; break;       // RGBA
            default: return false;
        }
        
        int bytesPerRow = ((width * bitsPerPixel + 7) / 8) + 1; // +1 for filter byte
        
        // 解压缩数据
        std::vector<unsigned char> decompressedData;
        uLongf decompressedSize = bytesPerRow * height;
        decompressedData.resize(decompressedSize);
        
        int result = uncompress(decompressedData.data(), &decompressedSize,
                               compressedData.data(), compressedData.size());
        
        if (result != Z_OK) {
            return false;
        }
        
        decompressedData.resize(decompressedSize);
        
        // 用于滤波器的字节数（8位时使用）
        int filterBytesPerPixel = (bitsPerPixel + 7) / 8;
        
        // 分配输出图像数据（RGBA 格式）
        imageData.resize(width * height * 4);
        
        // 解码扫描线
        std::vector<unsigned char> prevRow(bytesPerRow - 1, 0);
        
        for (int y = 0; y < height; y++) {
            size_t rowStart = y * bytesPerRow;
            if (rowStart >= decompressedData.size()) break;
            
            unsigned char filterType = decompressedData[rowStart];
            
            // 应用滤波器
            for (int x = 0; x < bytesPerRow - 1; x++) {
                unsigned char raw = decompressedData[rowStart + 1 + x];
                unsigned char a = (x >= filterBytesPerPixel) ? decompressedData[rowStart + 1 + x - filterBytesPerPixel] : 0;
                unsigned char b = prevRow[x];
                unsigned char c = (x >= filterBytesPerPixel) ? prevRow[x - filterBytesPerPixel] : 0;
                
                unsigned char filtered = 0;
                switch (filterType) {
                    case 0: // None
                        filtered = raw;
                        break;
                    case 1: // Sub
                        filtered = raw + a;
                        break;
                    case 2: // Up
                        filtered = raw + b;
                        break;
                    case 3: // Average
                        filtered = raw + ((a + b) / 2);
                        break;
                    case 4: // Paeth
                        filtered = raw + paethPredictor(a, b, c);
                        break;
                    default:
                        filtered = raw;
                        break;
                }
                
                decompressedData[rowStart + 1 + x] = filtered;
                prevRow[x] = filtered;
            }
            
            // 转换为 RGBA
            for (int x = 0; x < width; x++) {
                int dstIdx = (y * width + x) * 4;
                
                if (colorType == 3) {
                    // 索引色 - 需要从调色板中查找
                    int paletteIndex = 0;
                    
                    if (bitDepth == 8) {
                        paletteIndex = decompressedData[rowStart + 1 + x];
                    } else if (bitDepth == 4) {
                        int byteIdx = x / 2;
                        int shift = (1 - (x % 2)) * 4;
                        paletteIndex = (decompressedData[rowStart + 1 + byteIdx] >> shift) & 0x0F;
                    } else if (bitDepth == 2) {
                        int byteIdx = x / 4;
                        int shift = (3 - (x % 4)) * 2;
                        paletteIndex = (decompressedData[rowStart + 1 + byteIdx] >> shift) & 0x03;
                    } else if (bitDepth == 1) {
                        int byteIdx = x / 8;
                        int shift = 7 - (x % 8);
                        paletteIndex = (decompressedData[rowStart + 1 + byteIdx] >> shift) & 0x01;
                    }
                    
                    // 从调色板获取颜色
                    if (paletteIndex * 3 + 2 < (int)palette.size()) {
                        imageData[dstIdx + 0] = palette[paletteIndex * 3 + 0];
                        imageData[dstIdx + 1] = palette[paletteIndex * 3 + 1];
                        imageData[dstIdx + 2] = palette[paletteIndex * 3 + 2];
                        imageData[dstIdx + 3] = 255;
                    } else {
                        imageData[dstIdx + 0] = 255;
                        imageData[dstIdx + 1] = 0;
                        imageData[dstIdx + 2] = 255;
                        imageData[dstIdx + 3] = 255;
                    }
                } else if (bitDepth == 8) {
                    // 8位深度的直接颜色
                    int bytesPerPixel = bitsPerPixel / 8;
                    int srcIdx = rowStart + 1 + x * bytesPerPixel;
                    
                    switch (colorType) {
                        case 0: // 灰度
                            imageData[dstIdx + 0] = decompressedData[srcIdx];
                            imageData[dstIdx + 1] = decompressedData[srcIdx];
                            imageData[dstIdx + 2] = decompressedData[srcIdx];
                            imageData[dstIdx + 3] = 255;
                            break;
                        case 2: // RGB
                            imageData[dstIdx + 0] = decompressedData[srcIdx + 0];
                            imageData[dstIdx + 1] = decompressedData[srcIdx + 1];
                            imageData[dstIdx + 2] = decompressedData[srcIdx + 2];
                            imageData[dstIdx + 3] = 255;
                            break;
                        case 4: // 灰度 + Alpha
                            imageData[dstIdx + 0] = decompressedData[srcIdx];
                            imageData[dstIdx + 1] = decompressedData[srcIdx];
                            imageData[dstIdx + 2] = decompressedData[srcIdx];
                            imageData[dstIdx + 3] = decompressedData[srcIdx + 1];
                            break;
                        case 6: // RGBA
                            imageData[dstIdx + 0] = decompressedData[srcIdx + 0];
                            imageData[dstIdx + 1] = decompressedData[srcIdx + 1];
                            imageData[dstIdx + 2] = decompressedData[srcIdx + 2];
                            imageData[dstIdx + 3] = decompressedData[srcIdx + 3];
                            break;
                        default:
                            imageData[dstIdx + 0] = 255;
                            imageData[dstIdx + 1] = 0;
                            imageData[dstIdx + 2] = 255;
                            imageData[dstIdx + 3] = 255;
                            break;
                    }
                } else {
                    // 其他位深度暂不支持（除了索引色）
                    imageData[dstIdx + 0] = 128;
                    imageData[dstIdx + 1] = 128;
                    imageData[dstIdx + 2] = 128;
                    imageData[dstIdx + 3] = 255;
                }
            }
        }
        
        return true;
    }
}

TextureLoader::TextureLoader() : m_texturePath("texture") {
}

TextureLoader::~TextureLoader() {
    clearCache();
}

void TextureLoader::setTexturePath(const std::string& path) {
    m_texturePath = path;
}

std::shared_ptr<Texture> TextureLoader::loadTexture(const std::string& blockName) {
    // 检查缓存
    auto it = m_textureCache.find(blockName);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    
    // 构建文件路径
    std::string filePath = m_texturePath + "/" + blockName + ".png";
    
    // 检查文件是否存在
    if (!std::filesystem::exists(filePath)) {
        // Logger::warn("Texture file not found: " + filePath);
        return nullptr;
    }
    
    // 加载纹理
    auto texture = loadTextureFromFile(filePath);
    if (texture) {
        m_textureCache[blockName] = texture;
        Logger::info("Loaded texture: " + blockName);
    }
    
    return texture;
}

std::shared_ptr<Texture> TextureLoader::getTexture(const std::string& blockName) {
    auto it = m_textureCache.find(blockName);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    return nullptr;
}

void TextureLoader::clearCache() {
    m_textureCache.clear();
}

std::shared_ptr<Texture> TextureLoader::loadTextureFromFile(const std::string& filePath) {
    // 读取文件
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        Logger::error("Failed to open file: " + filePath);
        return nullptr;
    }
    
    // 读取文件内容
    std::vector<unsigned char> fileData((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
    file.close();
    
    if (fileData.empty()) {
        Logger::error("File is empty: " + filePath);
        return nullptr;
    }
    
    // 解码 PNG
    std::vector<unsigned char> imageData;
    int width, height;
    if (!decodePNG(fileData, imageData, width, height)) {
        Logger::error("Failed to decode PNG: " + filePath);
        return nullptr;
    }
    
    // 创建纹理对象
    auto texture = std::make_shared<Texture>();
    texture->width = width;
    texture->height = height;
    
    // 生成 OpenGL 纹理
    glGenTextures(1, &texture->textureID);
    glBindTexture(GL_TEXTURE_2D, texture->textureID);
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
    
    // 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}

} // namespace MCATool
