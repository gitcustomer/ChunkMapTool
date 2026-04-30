#pragma once

#include <string>
#include <map>
#include <memory>

// 确保包含完整的 OpenGL 定义
#include <GLFW/glfw3.h>

// 如果 GL_CLAMP_TO_EDGE 未定义，手动定义它
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace MCATool {

/**
 * @brief 纹理数据结构
 */
struct Texture {
    GLuint textureID;
    int width;
    int height;
    
    Texture() : textureID(0), width(0), height(0) {}
    ~Texture() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
    }
};

/**
 * @brief 纹理加载器类
 */
class TextureLoader {
public:
    TextureLoader();
    ~TextureLoader();
    
    /**
     * @brief 加载纹理
     * @param blockName 方块名称（如 "stone"）
     * @return 纹理指针，如果加载失败返回 nullptr
     */
    std::shared_ptr<Texture> loadTexture(const std::string& blockName);
    
    /**
     * @brief 获取已加载的纹理
     * @param blockName 方块名称
     * @return 纹理指针，如果不存在返回 nullptr
     */
    std::shared_ptr<Texture> getTexture(const std::string& blockName);
    
    /**
     * @brief 清除所有纹理缓存
     */
    void clearCache();
    
    /**
     * @brief 设置纹理目录路径
     * @param path 纹理目录路径
     */
    void setTexturePath(const std::string& path);
    
private:
    std::string m_texturePath;
    std::map<std::string, std::shared_ptr<Texture>> m_textureCache;
    
    /**
     * @brief 从文件加载纹理到 OpenGL
     * @param filePath 文件路径
     * @return 纹理指针
     */
    std::shared_ptr<Texture> loadTextureFromFile(const std::string& filePath);
};

} // namespace MCATool
