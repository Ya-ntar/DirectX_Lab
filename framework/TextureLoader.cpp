#include "Framework.h"
#include <filesystem>

namespace gfw {

std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> LoadTexturesFromMtl(const std::wstring &mtlFilePath, Framework &framework) {
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> textureMap;

    std::ifstream mtlFile(mtlFilePath);
    if (!mtlFile.is_open()) {
        std::wcerr << L"Failed to open MTL file: " << mtlFilePath << std::endl;
        return textureMap;
    }

    std::wstring directory = std::filesystem::path(mtlFilePath).parent_path().wstring();

    std::string line;
    while (std::getline(mtlFile, line)) {
        if (line.find("map_") != std::string::npos) {
            std::istringstream iss(line);
            std::string type;
            std::wstring texturePath;
            iss >> type >> texturePath;

            std::wstring fullTexturePath = std::filesystem::path(directory) / texturePath;
            std::shared_ptr<Texture2D> texture = framework.CreateTextureFromFile(fullTexturePath);
            textureMap[texturePath] = texture;
        }
    }

    return textureMap;
}

} // namespace gfw