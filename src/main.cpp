#include "GUI/McaViewerGUI.h"
#include "Utils/Logger.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "=== MCA Viewer GUI ===" << std::endl;
    std::cout << "Minecraft Region File Viewer" << std::endl;
    std::cout << "Version 1.0.0" << std::endl;
    std::cout << std::endl;
    
    try {
        std::cout << "Creating GUI instance..." << std::endl;
        MCATool::McaViewerGUI gui;
        
        std::cout << "Initializing GUI..." << std::endl;
        if (!gui.initialize()) {
            std::cerr << "ERROR: Failed to initialize GUI" << std::endl;
            std::cerr << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        
        std::cout << "GUI initialized successfully" << std::endl;
        std::cout << "Starting main loop..." << std::endl;
        MCATool::Logger::info("Starting GUI...");
        gui.run();
        
        std::cout << "GUI closed normally" << std::endl;
        MCATool::Logger::info("GUI closed");
        return 0;
        
    } catch (const std::exception& ex) {
        std::cerr << "FATAL ERROR: " << ex.what() << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        MCATool::Logger::error("Fatal error: " + std::string(ex.what()));
        return 1;
    }
}
