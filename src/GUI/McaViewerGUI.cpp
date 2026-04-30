#include "GUI/McaViewerGUI.h"
#include "Utils/Logger.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

namespace MCATool {

McaViewerGUI::McaViewerGUI()
    : m_showFileDialog(false)
    , m_showRegionInfo(true)
    , m_showChunkList(true)
    , m_showChunkDetails(true)
    , m_showBlockStats(true)
    , m_showSectionViewer(false)
    , m_selectedChunkIndex(-1)
    , m_window(nullptr)
    , m_isDecoding(false)
    , m_currentFileIndex(-1)
    , m_sortBySize(false)
    , m_isSearching(false)
    , m_selectedSectionIndex(-1)
    , m_sectionViewerYLayer(0)
    , m_textureLoader(nullptr)
    , m_use3DView(false)
    , m_camera3DRotationX(30.0f)
    , m_camera3DRotationY(45.0f)
    , m_camera3DDistance(40.0f)
    , m_lastMousePos(0, 0)
    , m_isDragging(false)
    , m_showOnlyNonAir(true)
{
    m_searchChunkX[0] = '\0';
    m_searchChunkZ[0] = '\0';
}

McaViewerGUI::~McaViewerGUI() {
    cleanup();
}

bool McaViewerGUI::initialize() {
    // 初始化 GLFW
    if (!glfwInit()) {
        Logger::error("Failed to initialize GLFW");
        return false;
    }
    
    // 设置 OpenGL 版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // 创建窗口
    m_window = glfwCreateWindow(1600, 900, "MCA Viewer - Minecraft Region File Viewer", nullptr, nullptr);
    if (!m_window) {
        Logger::error("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // 启用垂直同步
    
    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // 设置字体大小
    io.FontGlobalScale = 1.5f;  // 放大字体 1.5 倍
    
    // 设置 ImGui 样式
    ImGui::StyleColorsDark();
    
    // 调整样式以适应更大的字体
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(12, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 18.0f;
    
    // 初始化 ImGui 后端
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // 初始化纹理加载器
    m_textureLoader = std::make_unique<TextureLoader>();
    m_textureLoader->setTexturePath("texture");
    
    Logger::info("GUI initialized successfully");
    return true;
}

void McaViewerGUI::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        
        // 开始新的 ImGui 帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // 渲染 GUI 组件
        renderMenuBar();
        
        if (m_showFileDialog) {
            renderFileDialog();
        }
        
        if (m_currentRegion) {
            // 如果有多个文件，显示文件选择器
            if (m_mcaFiles.size() > 1) {
                renderFileSelector();
            }
            
            if (m_showRegionInfo) {
                renderRegionInfo();
            }
            
            if (m_showChunkList) {
                renderChunkList();
            }
            
            if (m_showChunkDetails && m_selectedChunkIndex >= 0) {
                renderChunkDetails();
            }
            
            if (m_showBlockStats) {
                renderBlockStats();
            }
            
            if (m_showSectionViewer && m_selectedSectionIndex >= 0) {
                renderSectionViewer();
            }
        } else {
            // 显示欢迎界面 - 占据整个窗口
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(display_w - 100, display_h - 100), ImGuiCond_Always);
            ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
            
            // 标题
            ImGui::PushFont(nullptr);  // 使用默认字体
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("Welcome to MCA Viewer!");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            ImGui::TextWrapped("This tool allows you to view and analyze Minecraft region files (.mca).");
            
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();
            
            ImGui::Text("To get started:");
            ImGui::Spacing();
            ImGui::BulletText("Click 'File -> Open Test Cases' to load test region files");
            ImGui::BulletText("Click 'File -> Open MCA Folder...' to load your own region files");
            ImGui::BulletText("Or drag and drop a .mca file into this window");
            
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();
            
            ImGui::Text("Features:");
            ImGui::Spacing();
            ImGui::BulletText("View region and chunk information");
            ImGui::BulletText("Analyze block distribution and statistics");
            ImGui::BulletText("Inspect section data and biomes");
            ImGui::BulletText("Support for both legacy and modern Minecraft formats");
            
            ImGui::End();
        }
        
        // 渲染
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(m_window);
    }
}

void McaViewerGUI::cleanup() {
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(m_window);
        glfwTerminate();
        
        m_window = nullptr;
    }
}



void McaViewerGUI::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Test Cases", "Ctrl+O")) {
                // 打开默认测试案例目录
                loadMcaFolder("test_mca_files");
            }
            if (ImGui::MenuItem("Open MCA Folder...", "Ctrl+Shift+O")) {
                openNativeFolderDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(m_window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Region Info", nullptr, &m_showRegionInfo);
            ImGui::MenuItem("Chunk List", nullptr, &m_showChunkList);
            ImGui::MenuItem("Chunk Details", nullptr, &m_showChunkDetails);
            ImGui::MenuItem("Block Statistics", nullptr, &m_showBlockStats);
            ImGui::MenuItem("Section Viewer", nullptr, &m_showSectionViewer);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ImGui::OpenPopup("About");
            }
            ImGui::EndMenu();
        }
        
        // About 弹窗
        if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("MCA Viewer v1.0.0");
            ImGui::Separator();
            ImGui::Text("A Minecraft Region File Viewer");
            ImGui::Text("Built with Dear ImGui and C++17");
            ImGui::Spacing();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void McaViewerGUI::openNativeFileDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(m_window);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "MCA Files (*.mca)\0*.mca\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn) == TRUE) {
        std::string filePath(ofn.lpstrFile);
        std::cout << "Selected file: " << filePath << std::endl;
        loadMcaFile(filePath);
    }
#else
    // 非 Windows 平台，显示手动输入对话框
    m_showFileDialog = true;
#endif
}

void McaViewerGUI::openNativeFolderDialog() {
#ifdef _WIN32
    BROWSEINFOA bi = {0};
    bi.hwndOwner = glfwGetWin32Window(m_window);
    bi.lpszTitle = "Select folder containing MCA files";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            std::string folderPath(path);
            std::cout << "Selected folder: " << folderPath << std::endl;
            loadMcaFolder(folderPath);
        }
        CoTaskMemFree(pidl);
    }
#else
    // 非 Windows 平台，显示手动输入对话框
    m_showFileDialog = true;
#endif
}

void McaViewerGUI::renderFileDialog() {
    // 这个对话框现在作为备用方案（非 Windows 平台）
    ImGui::SetNextWindowPos(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Open MCA File", &m_showFileDialog)) {
        static char filePathBuffer[512] = "";
        
        ImGui::Text("Enter MCA file or folder path:");
        ImGui::InputText("##filepath", filePathBuffer, sizeof(filePathBuffer));
        ImGui::SameLine();
        
        if (ImGui::Button("Load File")) {
            std::string filePath(filePathBuffer);
            if (!filePath.empty()) {
                loadMcaFile(filePath);
                m_showFileDialog = false;
                filePathBuffer[0] = '\0';
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Load Folder")) {
            std::string folderPath(filePathBuffer);
            if (!folderPath.empty()) {
                loadMcaFolder(folderPath);
                m_showFileDialog = false;
                filePathBuffer[0] = '\0';
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_showFileDialog = false;
            filePathBuffer[0] = '\0';
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("Example file: test_mca_files/r.0.0.mca");
        ImGui::TextWrapped("Example folder: test_mca_files/");
        ImGui::TextWrapped("Note: Make sure the path exists.");
        
        // 显示当前工作目录
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Current directory: %s", std::filesystem::current_path().string().c_str());
    }
    ImGui::End();
}

void McaViewerGUI::renderFileSelector() {
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("File Selector")) {
        ImGui::Text("Loaded Files: %zu", m_mcaFiles.size());
        ImGui::Separator();
        
        if (ImGui::BeginCombo("Select MCA File", m_currentFileIndex >= 0 ? 
            std::filesystem::path(m_mcaFiles[m_currentFileIndex]).filename().string().c_str() : "None")) {
            for (int i = 0; i < static_cast<int>(m_mcaFiles.size()); i++) {
                bool isSelected = (m_currentFileIndex == i);
                std::string fileName = std::filesystem::path(m_mcaFiles[i]).filename().string();
                if (ImGui::Selectable(fileName.c_str(), isSelected)) {
                    if (m_currentFileIndex != i) {
                        loadFileByIndex(i);
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Previous File", ImVec2(190, 0))) {
            if (m_currentFileIndex > 0) {
                loadFileByIndex(m_currentFileIndex - 1);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Next File", ImVec2(190, 0))) {
            if (m_currentFileIndex < static_cast<int>(m_mcaFiles.size()) - 1) {
                loadFileByIndex(m_currentFileIndex + 1);
            }
        }
    }
    ImGui::End();
}

void McaViewerGUI::renderRegionInfo() {
    float yOffset = m_mcaFiles.size() > 1 ? 190.0f : 30.0f;
    ImGui::SetNextWindowPos(ImVec2(10, yOffset), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Region Information", &m_showRegionInfo)) {
        if (m_currentRegion) {
            ImGui::Text("File: %s", m_currentFilePath.c_str());
            ImGui::Separator();
            ImGui::Text("Region Coordinates: (%d, %d)", m_currentRegion->regionX, m_currentRegion->regionZ);
            ImGui::Text("Total Chunks: %zu", m_currentRegion->chunks.size());
            ImGui::Text("Cached Decoded Chunks: %zu", m_decodedChunksCache.size());
            
            ImGui::Spacing();
            
            // 显示解码状态
            if (m_isDecoding) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", m_decodingStatus.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Chunks are decoded on-demand when selected");
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Clear Cache", ImVec2(-1, 0))) {
                m_decodedChunksCache.clear();
                m_blockStats.clear();
                std::cout << "Cleared decoded chunks cache" << std::endl;
            }
        }
    }
    ImGui::End();
}

void McaViewerGUI::renderChunkList() {
    float yOffset = m_mcaFiles.size() > 1 ? 400.0f : 240.0f;
    float height = m_mcaFiles.size() > 1 ? 490.0f : 650.0f;
    ImGui::SetNextWindowPos(ImVec2(10, yOffset), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, height), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Chunk List", &m_showChunkList)) {
        if (m_currentRegion && !m_currentRegion->chunks.empty()) {
            ImGui::Text("Total: %zu chunks", m_currentRegion->chunks.size());
            
            // 搜索功能
            ImGui::Text("Search by Coordinates:");
            ImGui::PushItemWidth(80);
            ImGui::InputText("X", m_searchChunkX, sizeof(m_searchChunkX), ImGuiInputTextFlags_CharsDecimal);
            ImGui::SameLine();
            ImGui::InputText("Z", m_searchChunkZ, sizeof(m_searchChunkZ), ImGuiInputTextFlags_CharsDecimal);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            if (ImGui::Button("Search")) {
                // 执行搜索
                if (m_searchChunkX[0] != '\0' && m_searchChunkZ[0] != '\0') {
                    try {
                        int searchX = std::stoi(m_searchChunkX);
                        int searchZ = std::stoi(m_searchChunkZ);
                        
                        // 查找匹配的 chunk
                        bool found = false;
                        for (size_t i = 0; i < m_currentRegion->chunks.size(); i++) {
                            const auto& chunk = m_currentRegion->chunks[i];
                            if (chunk.chunkX == searchX && chunk.chunkZ == searchZ) {
                                m_selectedChunkIndex = static_cast<int>(i);
                                decodeChunkByIndex(static_cast<int>(i));
                                calculateBlockStats();
                                found = true;
                                std::cout << "Found chunk at index " << i << " with coordinates (" 
                                         << searchX << ", " << searchZ << ")" << std::endl;
                                break;
                            }
                        }
                        
                        if (!found) {
                            std::cout << "Chunk (" << searchX << ", " << searchZ << ") not found in this region" << std::endl;
                        }
                        
                        m_isSearching = found;
                    } catch (const std::exception& ex) {
                        std::cerr << "Invalid search coordinates: " << ex.what() << std::endl;
                    }
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_searchChunkX[0] = '\0';
                m_searchChunkZ[0] = '\0';
                m_isSearching = false;
            }
            
            // 排序选项
            if (ImGui::Checkbox("Sort by Size", &m_sortBySize)) {
                updateChunkDisplayOrder();
            }
            
            ImGui::Separator();
            
            if (ImGui::BeginTable("ChunkTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Chunk X", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Chunk Z", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();
                
                // 使用显示顺序遍历
                for (size_t displayIdx = 0; displayIdx < m_chunkDisplayOrder.size(); displayIdx++) {
                    int chunkIdx = m_chunkDisplayOrder[displayIdx];
                    const auto& chunkData = m_currentRegion->chunks[chunkIdx];
                    
                    // 如果正在搜索，只显示匹配的 chunk
                    if (m_isSearching && m_selectedChunkIndex != chunkIdx) {
                        continue;
                    }
                    
                    ImGui::TableNextRow();
                    
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(std::to_string(chunkIdx).c_str(), m_selectedChunkIndex == chunkIdx, ImGuiSelectableFlags_SpanAllColumns)) {
                        m_selectedChunkIndex = chunkIdx;
                        // 按需解码选中的 chunk
                        decodeChunkByIndex(chunkIdx);
                        calculateBlockStats();
                    }
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", chunkData.chunkX);
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", chunkData.chunkZ);
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f KB", chunkData.nbtData.size() / 1024.0);
                }
                
                ImGui::EndTable();
            }
        } else {
            ImGui::TextWrapped("No region loaded. Open a MCA file to view chunks.");
        }
    }
    ImGui::End();
}

void McaViewerGUI::renderChunkDetails() {
    ImGui::SetNextWindowPos(ImVec2(420, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(580, 500), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Chunk Details", &m_showChunkDetails)) {
        const DecodedChunk* chunk = getDecodedChunk(m_selectedChunkIndex);
        if (chunk) {
            
            ImGui::Text("Chunk (%d, %d)", chunk->chunkX, chunk->chunkZ);
            ImGui::Separator();
            
            ImGui::Text("Sections: %zu", chunk->sections.size());
            ImGui::Text("Block Entities: %zu", chunk->blockEntities.size());
            ImGui::Text("Entities: %zu", chunk->entities.size());
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::CollapsingHeader("Sections", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginTable("SectionTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
                    ImGui::TableSetupColumn("Section Y", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Blocks", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGui::TableSetupColumn("Biomes", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGui::TableHeadersRow();
                    
                    for (size_t sectionIdx = 0; sectionIdx < chunk->sections.size(); sectionIdx++) {
                        const auto& section = chunk->sections[sectionIdx];
                        ImGui::TableNextRow();
                        
                        ImGui::TableNextColumn();
                        // 可点击的 Section Y
                        char sectionLabel[32];
                        snprintf(sectionLabel, sizeof(sectionLabel), "Y = %d", section.sectionY);
                        if (ImGui::Selectable(sectionLabel, m_selectedSectionIndex == static_cast<int>(sectionIdx), ImGuiSelectableFlags_SpanAllColumns)) {
                            m_selectedSectionIndex = static_cast<int>(sectionIdx);
                            m_sectionViewerYLayer = 0;  // 重置到第一层
                            m_showSectionViewer = true;
                            std::cout << "Selected section Y = " << section.sectionY << " for detailed view" << std::endl;
                        }
                        
                        ImGui::TableNextColumn();
                        // 统计该 Section 的方块类型
                        std::map<std::string, int> blockCount;
                        for (const auto& blockName : section.blockNames) {
                            blockCount[getShortBlockName(blockName)]++;
                        }
                        ImGui::Text("%zu types", blockCount.size());
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            int shown = 0;
                            for (const auto& pair : blockCount) {
                                if (shown++ >= 5) {
                                    ImGui::Text("...");
                                    break;
                                }
                                ImGui::Text("%s: %d", pair.first.c_str(), pair.second);
                            }
                            ImGui::EndTooltip();
                        }
                        
                        ImGui::TableNextColumn();
                        // 统计该 Section 的生物群系类型
                        std::map<std::string, int> biomeCount;
                        for (const auto& biomeName : section.biomeNames) {
                            biomeCount[getShortBlockName(biomeName)]++;
                        }
                        ImGui::Text("%zu types", biomeCount.size());
                        if (ImGui::IsItemHovered() && !biomeCount.empty()) {
                            ImGui::BeginTooltip();
                            for (const auto& pair : biomeCount) {
                                ImGui::Text("%s: %d", pair.first.c_str(), pair.second);
                            }
                            ImGui::EndTooltip();
                        }
                    }
                    
                    ImGui::EndTable();
                }
            }
        } else {
            ImGui::TextWrapped("Select a chunk from the Chunk List to view details.");
        }
    }
    ImGui::End();
}

void McaViewerGUI::renderBlockStats() {
    ImGui::SetNextWindowPos(ImVec2(420, 540), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(580, 350), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Block Statistics", &m_showBlockStats)) {
        if (m_selectedChunkIndex >= 0 && !m_blockStats.empty()) {
            ImGui::Text("Top Block Types in Selected Chunk");
            ImGui::Separator();
            
            if (ImGui::BeginTable("BlockStatsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Block Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();
                
                for (size_t i = 0; i < m_blockStats.size() && i < 20; i++) {
                    const auto& stat = m_blockStats[i];
                    ImGui::TableNextRow();
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i + 1);
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", stat.name.c_str());
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", formatNumber(stat.count).c_str());
                }
                
                ImGui::EndTable();
            }
        } else {
            ImGui::TextWrapped("Select a chunk from the Chunk List to view block statistics.");
        }
    }
    ImGui::End();
}

void McaViewerGUI::loadMcaFile(const std::string& filePath) {
    try {
        Logger::info("Loading MCA file: " + filePath);
        std::cout << "Loading MCA file: " << filePath << std::endl;
        
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            std::string error = "File not found: " + filePath;
            Logger::error(error);
            std::cerr << "ERROR: " << error << std::endl;
            return;
        }
        
        clearData();
        
        // 设置为单文件模式
        m_mcaFiles.clear();
        m_mcaFiles.push_back(filePath);
        m_currentFileIndex = 0;
        m_currentFilePath = filePath;
        
        // 加载 Region（这个操作很快）
        std::cout << "Loading region data..." << std::endl;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        Region region = McaFileLoader::loadRegion(filePath);
        m_currentRegion = std::make_unique<Region>(std::move(region));
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::string msg = "Successfully loaded region with " + std::to_string(m_currentRegion->chunks.size()) + 
                         " chunks in " + std::to_string(duration.count()) + " ms";
        Logger::info(msg);
        std::cout << msg << std::endl;
        
        // 初始化显示顺序
        updateChunkDisplayOrder();
        
        std::cout << "Select a chunk from the Chunk List to decode and view it." << std::endl;
        
    } catch (const std::exception& ex) {
        std::string error = "Failed to load MCA file: " + std::string(ex.what());
        Logger::error(error);
        std::cerr << "ERROR: " << error << std::endl;
        clearData();
    }
}

void McaViewerGUI::decodeChunkByIndex(int chunkIndex) {
    if (!m_currentRegion || chunkIndex < 0 || chunkIndex >= static_cast<int>(m_currentRegion->chunks.size())) {
        return;
    }
    
    // 如果已经解码过，直接返回
    if (m_decodedChunksCache.find(chunkIndex) != m_decodedChunksCache.end()) {
        return;
    }
    
    // 如果正在解码，跳过
    if (m_isDecoding) {
        return;
    }
    
    try {
        m_isDecoding = true;
        const auto& chunkData = m_currentRegion->chunks[chunkIndex];
        m_decodingStatus = "Decoding chunk (" + std::to_string(chunkData.chunkX) + ", " + 
                          std::to_string(chunkData.chunkZ) + ")...";
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        DecodedChunk decoded = ChunkDecoder::decode(chunkData);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // 缓存解码结果
        m_decodedChunksCache[chunkIndex] = std::move(decoded);
        
        m_isDecoding = false;
        m_decodingStatus = "";
        
        std::cout << "Decoded chunk " << chunkIndex << " (" << chunkData.chunkX << ", " << chunkData.chunkZ 
                  << ") in " << duration.count() << " ms" << std::endl;
        
    } catch (const std::exception& ex) {
        m_isDecoding = false;
        m_decodingStatus = "";
        std::string error = "Failed to decode chunk " + std::to_string(chunkIndex) + ": " + std::string(ex.what());
        Logger::error(error);
        std::cerr << "ERROR: " << error << std::endl;
    }
}

const DecodedChunk* McaViewerGUI::getDecodedChunk(int chunkIndex) {
    if (chunkIndex < 0) {
        return nullptr;
    }
    
    auto it = m_decodedChunksCache.find(chunkIndex);
    if (it != m_decodedChunksCache.end()) {
        return &it->second;
    }
    
    return nullptr;
}

void McaViewerGUI::calculateBlockStats() {
    m_blockStats.clear();
    
    const DecodedChunk* chunk = getDecodedChunk(m_selectedChunkIndex);
    if (!chunk) {
        return;
    }
    
    std::map<std::string, int> blockCount;
    
    // 统计所有方块
    for (const auto& section : chunk->sections) {
        for (const auto& blockName : section.blockNames) {
            std::string shortName = getShortBlockName(blockName);
            blockCount[shortName]++;
        }
    }
    
    // 转换为 vector 并排序
    for (const auto& pair : blockCount) {
        m_blockStats.push_back({pair.first, pair.second});
    }
    
    std::sort(m_blockStats.begin(), m_blockStats.end(), [](const BlockStat& a, const BlockStat& b) {
        return a.count > b.count;
    });
}

void McaViewerGUI::clearData() {
    m_currentRegion.reset();
    m_decodedChunksCache.clear();
    m_blockStats.clear();
    m_selectedChunkIndex = -1;
    m_currentFilePath.clear();
    m_isDecoding = false;
    m_decodingStatus.clear();
    m_mcaFiles.clear();
    m_currentFileIndex = -1;
    m_chunkDisplayOrder.clear();
    m_sortBySize = false;
    m_searchChunkX[0] = '\0';
    m_searchChunkZ[0] = '\0';
    m_isSearching = false;
}

void McaViewerGUI::updateChunkDisplayOrder() {
    if (!m_currentRegion) {
        m_chunkDisplayOrder.clear();
        return;
    }
    
    // 初始化索引列表
    m_chunkDisplayOrder.clear();
    m_chunkDisplayOrder.reserve(m_currentRegion->chunks.size());
    for (size_t i = 0; i < m_currentRegion->chunks.size(); i++) {
        m_chunkDisplayOrder.push_back(static_cast<int>(i));
    }
    
    // 如果需要按大小排序
    if (m_sortBySize) {
        std::sort(m_chunkDisplayOrder.begin(), m_chunkDisplayOrder.end(), 
            [this](int a, int b) {
                return m_currentRegion->chunks[a].nbtData.size() > m_currentRegion->chunks[b].nbtData.size();
            });
        std::cout << "Chunks sorted by size (descending)" << std::endl;
    } else {
        std::cout << "Chunks sorted by index (default order)" << std::endl;
    }
}

std::string McaViewerGUI::formatNumber(int number) {
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << number;
    return ss.str();
}

void McaViewerGUI::loadMcaFolder(const std::string& folderPath) {
    try {
        Logger::info("Loading MCA files from folder: " + folderPath);
        std::cout << "Scanning folder for MCA files: " << folderPath << std::endl;
        
        // 检查文件夹是否存在
        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
            std::string error = "Folder not found or not a directory: " + folderPath;
            Logger::error(error);
            std::cerr << "ERROR: " << error << std::endl;
            return;
        }
        
        // 查找所有 .mca 文件并排序
        std::vector<std::string> mcaFiles;
        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mca") {
                mcaFiles.push_back(entry.path().string());
            }
        }
        
        if (mcaFiles.empty()) {
            std::string error = "No MCA files found in folder: " + folderPath;
            Logger::error(error);
            std::cerr << "ERROR: " << error << std::endl;
            return;
        }
        
        // 按文件名排序，确保 r.0.0.mca, r.0.1.mca 等按顺序排列
        std::sort(mcaFiles.begin(), mcaFiles.end());
        
        std::cout << "Found " << mcaFiles.size() << " MCA file(s)" << std::endl;
        
        clearData();
        
        // 保存所有文件路径
        m_mcaFiles = mcaFiles;
        
        // 加载第一个文件
        if (!m_mcaFiles.empty()) {
            std::cout << "Loading first file: " << m_mcaFiles[0] << std::endl;
            if (m_mcaFiles.size() > 1) {
                std::cout << "Use the File Selector to switch between " << m_mcaFiles.size() << " files." << std::endl;
            }
            loadFileByIndex(0);
        }
        
    } catch (const std::exception& ex) {
        std::string error = "Failed to load MCA folder: " + std::string(ex.what());
        Logger::error(error);
        std::cerr << "ERROR: " << error << std::endl;
    }
}

void McaViewerGUI::loadFileByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_mcaFiles.size())) {
        Logger::error("Invalid file index: " + std::to_string(index));
        return;
    }
    
    try {
        const std::string& filePath = m_mcaFiles[index];
        Logger::info("Switching to file: " + filePath);
        std::cout << "Loading file " << (index + 1) << " of " << m_mcaFiles.size() << ": " << filePath << std::endl;
        
        // 清除当前数据（但保留文件列表）
        m_currentRegion.reset();
        m_decodedChunksCache.clear();
        m_blockStats.clear();
        m_selectedChunkIndex = -1;
        m_isDecoding = false;
        m_decodingStatus.clear();
        
        m_currentFileIndex = index;
        m_currentFilePath = filePath;
        
        // 加载新的 Region
        auto startTime = std::chrono::high_resolution_clock::now();
        
        Region region = McaFileLoader::loadRegion(filePath);
        m_currentRegion = std::make_unique<Region>(std::move(region));
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::string msg = "Successfully loaded region with " + std::to_string(m_currentRegion->chunks.size()) + 
                         " chunks in " + std::to_string(duration.count()) + " ms";
        Logger::info(msg);
        std::cout << msg << std::endl;
        
        // 初始化显示顺序
        updateChunkDisplayOrder();
        
        std::cout << "Select a chunk from the Chunk List to decode and view it." << std::endl;
        
    } catch (const std::exception& ex) {
        std::string error = "Failed to load file at index " + std::to_string(index) + ": " + std::string(ex.what());
        Logger::error(error);
        std::cerr << "ERROR: " << error << std::endl;
    }
}

void McaViewerGUI::renderSectionViewer() {
    ImGui::SetNextWindowPos(ImVec2(1020, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 860), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Section Viewer", &m_showSectionViewer)) {
        const DecodedChunk* chunk = getDecodedChunk(m_selectedChunkIndex);
        if (!chunk || m_selectedSectionIndex < 0 || m_selectedSectionIndex >= static_cast<int>(chunk->sections.size())) {
            ImGui::TextWrapped("No section selected. Click on a section in the Chunk Details panel to view it.");
            ImGui::End();
            return;
        }
        
        const auto& section = chunk->sections[m_selectedSectionIndex];
        
        // 视图模式切换
        ImGui::Text("View Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("2D Grid", !m_use3DView)) {
            m_use3DView = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("3D Cube", m_use3DView)) {
            m_use3DView = true;
        }
        
        ImGui::Separator();
        
        // 根据模式渲染不同的视图
        if (m_use3DView) {
            renderSection3DView(chunk, section);
        } else {
            // 原有的 2D 网格视图代码
            renderSection2DView(chunk, section);
        }
    }
    ImGui::End();
}

void McaViewerGUI::renderSection2DView(const DecodedChunk* chunk, const DecodedSection& section) {
        
    // 标题信息
    ImGui::Text("Chunk (%d, %d) - Section Y = %d", chunk->chunkX, chunk->chunkZ, section.sectionY);
    ImGui::Separator();
        
        // Y 层选择器
        ImGui::Text("Y Layer (0-15):");
        ImGui::SliderInt("##YLayer", &m_sectionViewerYLayer, 0, 15);
        ImGui::SameLine();
        ImGui::Text("Absolute Y: %d", section.sectionY * 16 + m_sectionViewerYLayer);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // 统计当前层的方块类型
        std::map<std::string, int> layerBlockCount;
        int layerStartIdx = m_sectionViewerYLayer * 256;  // 16x16 = 256 blocks per layer
        for (int i = 0; i < 256; i++) {
            int blockIdx = layerStartIdx + i;
            if (blockIdx < 4096) {
                std::string shortName = getShortBlockName(section.blockNames[blockIdx]);
                layerBlockCount[shortName]++;
            }
        }
        
        ImGui::Text("Block types in this layer: %zu", layerBlockCount.size());
        
        // 显示 16x16 网格
        ImGui::Spacing();
        ImGui::Text("16x16 Block Grid (hover for details):");
        ImGui::Spacing();
        
        const float cellSize = 30.0f;
        const float gridSize = cellSize * 16;
        
        ImVec2 gridStart = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // 为不同方块类型分配颜色（作为纹理加载失败时的后备方案）
        std::map<std::string, ImU32> blockColors;
        int colorIndex = 0;
        ImU32 predefinedColors[] = {
            IM_COL32(100, 100, 100, 255),  // 灰色 - air
            IM_COL32(139, 90, 43, 255),    // 棕色 - 泥土/石头
            IM_COL32(34, 139, 34, 255),    // 绿色 - 草
            IM_COL32(0, 100, 200, 255),    // 蓝色 - 水
            IM_COL32(200, 200, 0, 255),    // 黄色 - 沙子
            IM_COL32(150, 150, 150, 255),  // 浅灰 - 石头
            IM_COL32(139, 69, 19, 255),    // 深棕 - 木头
            IM_COL32(255, 0, 0, 255),      // 红色 - 其他
        };
        
        for (const auto& pair : layerBlockCount) {
            if (pair.first == "air") {
                blockColors[pair.first] = IM_COL32(200, 200, 200, 100);  // 半透明灰色
            } else {
                blockColors[pair.first] = predefinedColors[colorIndex % 8];
                colorIndex++;
            }
        }
        
        // 绘制网格
        for (int z = 0; z < 16; z++) {
            for (int x = 0; x < 16; x++) {
                int blockIdx = layerStartIdx + z * 16 + x;
                std::string blockName = getShortBlockName(section.blockNames[blockIdx]);
                
                ImVec2 cellMin(gridStart.x + x * cellSize, gridStart.y + z * cellSize);
                ImVec2 cellMax(cellMin.x + cellSize, cellMin.y + cellSize);
                
                // 尝试加载并使用纹理
                auto texture = m_textureLoader->loadTexture(blockName);
                if (texture && texture->textureID != 0) {
                    // 使用纹理渲染
                    ImVec2 uv0(0.0f, 0.0f);
                    ImVec2 uv1(1.0f, 1.0f);
                    drawList->AddImage((ImTextureID)(intptr_t)texture->textureID, cellMin, cellMax, uv0, uv1);
                } else {
                    // 纹理加载失败，使用纯色渲染
                    ImU32 color = blockColors[blockName];
                    drawList->AddRectFilled(cellMin, cellMax, color);
                }
                
                // 绘制边框
                drawList->AddRect(cellMin, cellMax, IM_COL32(50, 50, 50, 255), 0.0f, 0, 1.0f);
                
                // 鼠标悬停显示信息
                if (ImGui::IsMouseHoveringRect(cellMin, cellMax)) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Local Pos: (%d, %d, %d)", x, m_sectionViewerYLayer, z);
                    ImGui::Text("World Pos: (%d, %d, %d)", 
                               chunk->chunkX * 16 + x, 
                               section.sectionY * 16 + m_sectionViewerYLayer, 
                               chunk->chunkZ * 16 + z);
                    ImGui::Separator();
                    ImGui::Text("Block: %s", blockName.c_str());
                    ImGui::Text("Full: %s", section.blockNames[blockIdx].c_str());
                    ImGui::EndTooltip();
                }
            }
        }
        
        // 占位符，确保网格有足够空间
        ImGui::Dummy(ImVec2(gridSize, gridSize));
        
        // 图例
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Legend (Top 10 blocks in this layer):");
        
        // 按数量排序
        std::vector<std::pair<std::string, int>> sortedBlocks(layerBlockCount.begin(), layerBlockCount.end());
        std::sort(sortedBlocks.begin(), sortedBlocks.end(), 
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        size_t maxBlocks = sortedBlocks.size() < 10 ? sortedBlocks.size() : 10;
        for (size_t i = 0; i < maxBlocks; i++) {
            const auto& pair = sortedBlocks[i];
            
            // 绘制图例方块（纹理或颜色）
            ImVec2 legendPos = ImGui::GetCursorScreenPos();
            auto texture = m_textureLoader->getTexture(pair.first);
            if (texture && texture->textureID != 0) {
                // 使用纹理
                ImVec2 uv0(0.0f, 0.0f);
                ImVec2 uv1(1.0f, 1.0f);
                drawList->AddImage((ImTextureID)(intptr_t)texture->textureID, 
                                  legendPos, ImVec2(legendPos.x + 20, legendPos.y + 20), uv0, uv1);
            } else {
                // 使用颜色
                ImU32 color = blockColors[pair.first];
                drawList->AddRectFilled(legendPos, ImVec2(legendPos.x + 20, legendPos.y + 20), color);
            }
            drawList->AddRect(legendPos, ImVec2(legendPos.x + 20, legendPos.y + 20), IM_COL32(0, 0, 0, 255));
            
            ImGui::Dummy(ImVec2(25, 20));
            ImGui::SameLine();
            ImGui::Text("%s: %d blocks", pair.first.c_str(), pair.second);
        }
}

void McaViewerGUI::renderSection3DView(const DecodedChunk* chunk, const DecodedSection& section) {
    // 控制选项
    ImGui::Checkbox("Show only non-air blocks", &m_showOnlyNonAir);
    ImGui::SameLine();
    ImGui::Text("| Camera: Drag=Rotate, Scroll=Zoom");
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // 获取可用的渲染区域
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = canvasSize.y > 600 ? 600 : canvasSize.y;
    
    // 创建一个画布用于 3D 渲染
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // 绘制背景
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                           IM_COL32(30, 30, 30, 255));
    
    // 处理鼠标输入
    ImGui::InvisibleButton("3DCanvas", canvasSize);
    bool isHovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    
    if (isHovered) {
        // 鼠标滚轮缩放
        if (io.MouseWheel != 0.0f) {
            m_camera3DDistance -= io.MouseWheel * 2.0f;
            if (m_camera3DDistance < 10.0f) m_camera3DDistance = 10.0f;
            if (m_camera3DDistance > 100.0f) m_camera3DDistance = 100.0f;
        }
        
        // 鼠标拖拽旋转
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (!m_isDragging) {
                m_isDragging = true;
                m_lastMousePos = io.MousePos;
            } else {
                ImVec2 delta;
                delta.x = io.MousePos.x - m_lastMousePos.x;
                delta.y = io.MousePos.y - m_lastMousePos.y;
                
                m_camera3DRotationY += delta.x * 0.5f;
                m_camera3DRotationX += delta.y * 0.5f;
                
                // 限制 X 轴旋转角度
                if (m_camera3DRotationX > 89.0f) m_camera3DRotationX = 89.0f;
                if (m_camera3DRotationX < -89.0f) m_camera3DRotationX = -89.0f;
                
                m_lastMousePos = io.MousePos;
            }
        } else {
            m_isDragging = false;
        }
    }
    
    // 简单的 3D 到 2D 投影函数
    auto project3D = [&](float x, float y, float z) -> ImVec2 {
        // 将坐标中心化
        x -= 8.0f;
        y -= 8.0f;
        z -= 8.0f;
        
        // 旋转矩阵
        float radX = m_camera3DRotationX * 3.14159f / 180.0f;
        float radY = m_camera3DRotationY * 3.14159f / 180.0f;
        
        float cosX = cosf(radX);
        float sinX = sinf(radX);
        float cosY = cosf(radY);
        float sinY = sinf(radY);
        
        // 绕 X 轴旋转
        float y1 = y * cosX - z * sinX;
        float z1 = y * sinX + z * cosX;
        
        // 绕 Y 轴旋转
        float x2 = x * cosY + z1 * sinY;
        float z2 = -x * sinY + z1 * cosY;
        float y2 = y1;
        
        // 透视投影
        float scale = m_camera3DDistance / (m_camera3DDistance + z2);
        float screenX = x2 * scale * 20.0f + canvasSize.x * 0.5f;
        float screenY = -y2 * scale * 20.0f + canvasSize.y * 0.5f;
        
        return ImVec2(canvasPos.x + screenX, canvasPos.y + screenY);
    };
    
    // 收集并排序方块（按深度排序以正确渲染）
    struct BlockInfo {
        int x, y, z;
        float depth;
        std::string name;
    };
    std::vector<BlockInfo> blocks;
    
    for (int y = 0; y < 16; y++) {
        for (int z = 0; z < 16; z++) {
            for (int x = 0; x < 16; x++) {
                int blockIdx = y * 256 + z * 16 + x;
                std::string blockName = getShortBlockName(section.blockNames[blockIdx]);
                
                // 跳过空气方块
                if (m_showOnlyNonAir && blockName == "air") {
                    continue;
                }
                
                // 检查是否需要渲染（完全包裹剔除优化）
                // 如果方块被其他非空气方块完全包裹，则不渲染
                bool renderBlock = false;
                
                // 检查相邻方块是否为非空气方块
                auto isNeighborSolid = [&](int dx, int dy, int dz) -> bool {
                    int nx = x + dx, ny = y + dy, nz = z + dz;
                    // 边界外视为空气（需要渲染）
                    if (nx < 0 || nx >= 16 || ny < 0 || ny >= 16 || nz < 0 || nz >= 16) {
                        return false;
                    }
                    int neighborIdx = ny * 256 + nz * 16 + nx;
                    std::string neighborName = getShortBlockName(section.blockNames[neighborIdx]);
                    // 如果相邻方块是空气，则当前方块需要渲染
                    return neighborName != "air";
                };
                
                // 检查六个面是否都被非空气方块包裹
                bool allSidesCovered = 
                    isNeighborSolid(-1, 0, 0) &&  // 左面
                    isNeighborSolid(1, 0, 0) &&   // 右面
                    isNeighborSolid(0, -1, 0) &&  // 底面
                    isNeighborSolid(0, 1, 0) &&   // 顶面
                    isNeighborSolid(0, 0, -1) &&  // 后面
                    isNeighborSolid(0, 0, 1);     // 前面
                
                // 如果不是所有面都被包裹，则需要渲染
                renderBlock = !allSidesCovered;
                
                if (renderBlock) {
                    // 计算深度（用于排序）
                    float cx = x - 8.0f;
                    float cy = y - 8.0f;
                    float cz = z - 8.0f;
                    float depth = cx * cx + cy * cy + cz * cz;
                    
                    blocks.push_back({x, y, z, depth, blockName});
                }
            }
        }
    }
    
    // 按深度排序（从远到近）
    std::sort(blocks.begin(), blocks.end(), [](const BlockInfo& a, const BlockInfo& b) {
        return a.depth > b.depth;
    });
    

    // 渲染方块
    for (const auto& block : blocks) {
        // 计算立方体的 8 个顶点
        // 顶点索引：
        // 0: (0,0,0)  1: (1,0,0)  2: (0,1,0)  3: (1,1,0)
        // 4: (0,0,1)  5: (1,0,1)  6: (0,1,1)  7: (1,1,1)
        ImVec2 v[8];
        for (int i = 0; i < 8; i++) {
            float dx = (i & 1) ? 1.0f : 0.0f;
            float dy = (i & 2) ? 1.0f : 0.0f;
            float dz = (i & 4) ? 1.0f : 0.0f;
            v[i] = project3D(block.x + dx, block.y + dy, block.z + dz);
        }
        
        // 加载方块纹理（如果尚未加载）
        auto texture = m_textureLoader->loadTexture(block.name);
        
        // 如果没有纹理，生成备用颜色
        ImU32 fallbackColor;
        unsigned char r = ((block.x * 17) % 200) + 55;
        unsigned char g = ((block.y * 23) % 200) + 55;
        unsigned char b = ((block.z * 31) % 200) + 55;
        fallbackColor = IM_COL32(r, g, b, 255);
        
        // 定义六个面及其法向量
        // 顶点顺序必须是逆时针（从外部看）
        struct Face {
            int v0, v1, v2, v3;  // 顶点索引（逆时针顺序）
            float nx, ny, nz;     // 法向量
            float brightness;     // 亮度系数
        };
        
        Face faces[6] = {
            {2, 6, 7, 3,  0,  1,  0, 1.0f},   // 顶面 (+Y): 左下、左上、右上、右下
            {0, 1, 5, 4,  0, -1,  0, 0.5f},   // 底面 (-Y): 左下、右下、右上、左上
            {4, 5, 7, 6,  0,  0,  1, 0.8f},   // 前面 (+Z): 左下、右下、右上、左上
            {1, 0, 2, 3,  0,  0, -1, 0.8f},   // 后面 (-Z): 左下、右下、右上、左上
            {5, 1, 3, 7,  1,  0,  0, 0.7f},   // 右面 (+X): 左下、右下、右上、左上
            {0, 4, 6, 2, -1,  0,  0, 0.7f}    // 左面 (-X): 左下、右下、右上、左上
        };
        
        // 渲染所有六个面（不进行背面剔除）
        for (const auto& face : faces) {
            // 绘制面
            if (texture && texture->textureID != 0) {
                // 使用纹理渲染，并应用亮度调整
                ImVec4 tintColor(face.brightness, face.brightness, face.brightness, 1.0f);
                drawList->AddImageQuad(
                    (ImTextureID)(intptr_t)texture->textureID,
                    v[face.v0], v[face.v1], v[face.v2], v[face.v3],
                    ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1),
                    IM_COL32(
                        static_cast<unsigned char>(255 * face.brightness),
                        static_cast<unsigned char>(255 * face.brightness),
                        static_cast<unsigned char>(255 * face.brightness),
                        255
                    )
                );
            } else {
                // 使用备用颜色渲染（应用亮度）
                unsigned char r = ((fallbackColor >> IM_COL32_R_SHIFT) & 0xFF) * face.brightness;
                unsigned char g = ((fallbackColor >> IM_COL32_G_SHIFT) & 0xFF) * face.brightness;
                unsigned char b = ((fallbackColor >> IM_COL32_B_SHIFT) & 0xFF) * face.brightness;
                ImU32 faceColor = IM_COL32(r, g, b, 255);
                
                drawList->AddQuadFilled(
                    v[face.v0], v[face.v1], v[face.v2], v[face.v3],
                    faceColor
                );
            }
            
            // 绘制边框（增强立体感）
            drawList->AddQuad(
                v[face.v0], v[face.v1], v[face.v2], v[face.v3],
                IM_COL32(0, 0, 0, 60), 1.0f
            );
        }
    }
    
    // 显示统计信息
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Rendered blocks: %zu", blocks.size());
    ImGui::Text("Camera Distance: %.1f", m_camera3DDistance);
    ImGui::Text("Rotation: X=%.1f°, Y=%.1f°", m_camera3DRotationX, m_camera3DRotationY);
    
    if (ImGui::Button("Reset Camera")) {
        m_camera3DRotationX = 30.0f;
        m_camera3DRotationY = 45.0f;
        m_camera3DDistance = 40.0f;
    }
}

std::string McaViewerGUI::getShortBlockName(const std::string& fullName) {
    // 移除 "minecraft:" 前缀
    std::string result = fullName;
    size_t colonPos = result.find(':');
    if (colonPos != std::string::npos) {
        result = result.substr(colonPos + 1);
    }
    
    // 移除方块状态 [...]
    size_t bracketPos = result.find('[');
    if (bracketPos != std::string::npos) {
        result = result.substr(0, bracketPos);
    }
    
    return result;
}

} // namespace MCATool
