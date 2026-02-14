#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>

// Manages the Dynamic Texture Atlas
class TextureManager {
public:
    static TextureManager& getInstance();

    // Scans directory and builds the atlas
    void loadTextures(const std::string& directoryPath);

    // Returns the OpenGL Texture ID of the Texture Array
    GLuint getTextureArrayID() const { return textureArrayID; }

    // Returns the Layer Index for a given texture name (e.g., "dirt")
    // Returns 0 (default) if not found.
    uint16_t getLayerIndex(const std::string& textureName) const;

private:
    TextureManager() = default;
    ~TextureManager();

    std::unordered_map<std::string, uint16_t> textureMap;
    GLuint textureArrayID = 0;
    
    // Internal struct for source image data
    struct TextureData {
        std::string name;
        std::vector<unsigned char> pixels;
        int width, height, channels;
    };

    // Helper to load raw pixels
    static TextureData loadPixels(const std::string& path, const std::string& name);
    
    // Helper to resize image (Nearest Neighbor)
    std::vector<unsigned char> upscaleImage(const std::vector<unsigned char>& src, int srcW, int srcH, int dstW, int dstH);
};
