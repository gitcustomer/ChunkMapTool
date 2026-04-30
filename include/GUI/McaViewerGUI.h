#pragma once

#include "Core/DataStructures.h"
#include "Core/McaFileLoader.h"
#include "Core/ChunkDecoder.h"
#include "Utils/TextureLoader.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace MCATool {

/**
 * @brief MCA 查看器 GUI 主类
 */
class McaViewerGUI {
public:
    McaViewerGUI();
    ~McaViewerGUI();
    
    /**
     * @brief 初始化 GUI
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * @brief 运行 GUI 主循环
     */
    void run();
    
    /**
     * @brief 清理资源
     */
    void cleanup();

private:
    // GUI 状态
    bool m_showFileDialog;
    bool m_showRegionInfo;
    bool m_showChunkList;
    bool m_showChunkDetails;
    bool m_showBlockStats;
    bool m_showSectionViewer;
    
    // 数据
    std::string m_currentFilePath;
    std::unique_ptr<Region> m_currentRegion;
    std::map<int, DecodedChunk> m_decodedChunksCache;  // 缓存已解析的 chunk，key 是 chunk 在 region 中的索引
    int m_selectedChunkIndex;
    
    // 多文件支持
    std::vector<std::string> m_mcaFiles;
    int m_currentFileIndex;
    
    // Chunk 列表排序和搜索
    std::vector<int> m_chunkDisplayOrder;  // chunk 的显示顺序（索引列表）
    bool m_sortBySize;  // 是否按大小排序
    char m_searchChunkX[32];  // 搜索的 Chunk X 坐标
    char m_searchChunkZ[32];  // 搜索的 Chunk Z 坐标
    bool m_isSearching;  // 是否正在搜索
    
    // Section 查看器
    int m_selectedSectionIndex;  // 选中的 Section 索引
    int m_sectionViewerYLayer;   // 当前查看的 Y 层（0-15）
    
    // 3D 视图相关
    bool m_use3DView;            // 是否使用 3D 视图
    float m_camera3DRotationX;   // 相机 X 轴旋转角度
    float m_camera3DRotationY;   // 相机 Y 轴旋转角度
    float m_camera3DDistance;    // 相机距离
    ImVec2 m_lastMousePos;       // 上次鼠标位置
    bool m_isDragging;           // 是否正在拖拽
    bool m_showOnlyNonAir;       // 是否只显示非空气方块
    
    // 解码状态
    bool m_isDecoding;
    std::string m_decodingStatus;
    
    // 统计数据
    struct BlockStat {
        std::string name;
        int count;
    };
    std::vector<BlockStat> m_blockStats;
    
    // GLFW 和 ImGui 相关
    GLFWwindow* m_window;
    
    // 纹理加载器
    std::unique_ptr<TextureLoader> m_textureLoader;
    
    // GUI 渲染方法
    void renderMenuBar();
    void renderFileDialog();
    void renderRegionInfo();
    void renderChunkList();
    void renderChunkDetails();
    void renderBlockStats();
    void renderFileSelector();
    void renderSectionViewer();
    void renderSection2DView(const DecodedChunk* chunk, const DecodedSection& section);
    void renderSection3DView(const DecodedChunk* chunk, const DecodedSection& section);
    
    // 文件选择方法
    void openNativeFileDialog();
    void openNativeFolderDialog();
    
    // 数据处理方法
    void loadMcaFile(const std::string& filePath);
    void loadMcaFolder(const std::string& folderPath);
    void loadFileByIndex(int index);
    void decodeChunkByIndex(int chunkIndex);
    const DecodedChunk* getDecodedChunk(int chunkIndex);
    void calculateBlockStats();
    void clearData();
    void updateChunkDisplayOrder();
    
    // 辅助方法
    std::string formatNumber(int number);
    std::string getShortBlockName(const std::string& fullName);
};

} // namespace MCATool
