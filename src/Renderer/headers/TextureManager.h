#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>

/**
 * @brief Manages the Dynamic Texture Atlas using OpenGL 2D Texture Arrays.
 * 
 * TextureManager is responsible for loading textures from the disk, processing them 
 * (tinting, upscaling, bleeding correction), and uploading them to an OpenGL 
 * GL_TEXTURE_2D_ARRAY. It follows the Singleton pattern.
 */
class TextureManager {
public:
    /**
     * @brief Gets the singleton instance of the TextureManager.
     * @return Reference to the TextureManager instance.
     */
    static TextureManager& getInstance();

    /**
     * @brief Scans a directory for textures, processes them, and builds the 2D Texture Array.
     * 
     * This method:
     * - Discovers valid texture files (.png, .jpg, .tga).
     * - Tints specific textures (e.g., foliage, water).
     * - Fixes texture bleeding at alpha boundaries.
     * - Upscales all textures to the maximum found dimension.
     * - Handles animated textures by treating vertical strips as frames.
     * - Uploads the final result to OpenGL.
     * 
     * @param directoryPath Path to the directory containing texture assets.
     */
    void loadTextures(const std::string& directoryPath);

    /**
     * @brief Returns the OpenGL Texture ID for the generated Texture Array.
     * @return GLuint representing the OpenGL texture object.
     */
    GLuint getTextureArrayID() const { return textureArrayID; }

    /**
     * @brief Returns the layer index for a given texture name.
     * 
     * @param textureName The name of the texture (filename without extension).
     * @return uint16_t The index of the first frame of the texture in the array. 
     *         Returns 0 if the texture is not found.
     */
    uint16_t getLayerIndex(const std::string& textureName) const;

private:
    TextureManager() = default;
    ~TextureManager();

    /// Map from texture name to its starting layer index in the OpenGL texture array.
    std::unordered_map<std::string, uint16_t> textureMap;
    
    /// OpenGL handle for the Texture Array.
    GLuint textureArrayID = 0;
    
    /**
     * @brief Internal struct to hold raw source image data before processing.
     */
    struct TextureData {
        std::string name;
        std::vector<unsigned char> pixels;
        int width, height, channels;
    };

    /**
     * @brief Helper to load raw pixel data from disk using stb_image.
     * @param path Full file path.
     * @param name Name identifier for the texture.
     * @return TextureData structure containing the loaded pixels and metadata.
     */
    static TextureData loadPixels(const std::string& path, const std::string& name);
    
    /**
     * @brief Helper to upscale image data using Nearest Neighbor filtering.
     * 
     * This is used to ensure all textures in the array have identical dimensions.
     * 
     * @param src Source pixel vector.
     * @param srcW Source width.
     * @param srcH Source height.
     * @param dstW Target width.
     * @param dstH Target height.
     * @return std::vector<unsigned char> The rescaled pixel data.
     */
    static std::vector<unsigned char> upscaleImage(const std::vector<unsigned char>& src, int srcW, int srcH, int dstW, int dstH);
};
