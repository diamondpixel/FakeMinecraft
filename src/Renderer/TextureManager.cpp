#include "headers/TextureManager.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <queue>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace fs = std::filesystem;

TextureManager& TextureManager::getInstance() {
    static TextureManager instance;
    return instance;
}

TextureManager::~TextureManager() {
    if (textureArrayID != 0) {
        glDeleteTextures(1, &textureArrayID);
    }
}

uint16_t TextureManager::getLayerIndex(const std::string& textureName) const {
    auto it = textureMap.find(textureName);
    if (it != textureMap.end()) {
        return it->second;
    }
    std::cerr << "Texture not found: " << textureName << ". Defaulting to 0." << std::endl;
    return 0;
}

TextureManager::TextureData TextureManager::loadPixels(const std::string& path, const std::string& name) {
    TextureData data;
    data.name = name;
    unsigned char* pixels = stbi_load(path.c_str(), &data.width, &data.height, &data.channels, 4);
    if (pixels) {
        // OPTIMIZATION: Use memcpy instead of assign() for raw pixel data
        const size_t size = static_cast<size_t>(data.width) * data.height * 4;
        data.pixels.resize(size);
        std::memcpy(data.pixels.data(), pixels, size);
        stbi_image_free(pixels);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        data.width = 0; data.height = 0;
    }
    return data;
}

// OPTIMIZATION: O(n²) BFS flood-fill replaces O(n⁴) brute-force algorithm
// Original: For each transparent pixel, scan ALL pixels -> O(transparent * total) = O(n⁴)
// New: Single BFS pass from opaque edges -> O(n²) where n = total pixels
void fixTextureBleeding(std::vector<unsigned char>& pixels, int width, int height) {
    if (width <= 0 || height <= 0) return;
    
    const int totalPixels = width * height;
    
    // Track which source pixel to copy color from (-1 = not yet assigned)
    std::vector<int> sourceIdx(totalPixels, -1);
    
    // BFS queue: stores pixel indices
    std::queue<int> bfsQueue;
    
    // Initialize: seed BFS from all opaque pixels
    for (int i = 0; i < totalPixels; ++i) {
        if (pixels[i * 4 + 3] > 0) {
            sourceIdx[i] = i;  // Opaque pixels are their own source
            bfsQueue.push(i);
        }
    }
    
    // If no opaque pixels, nothing to propagate
    if (bfsQueue.empty()) return;
    
    // 4-connected neighbors (left, right, up, down)
    const int dx[] = {-1, 1, 0, 0};
    const int dy[] = {0, 0, -1, 1};
    
    // BFS: propagate nearest opaque color to all transparent pixels
    while (!bfsQueue.empty()) {
        const int current = bfsQueue.front();
        bfsQueue.pop();
        
        const int cx = current % width;
        const int cy = current / width;
        
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + dx[d];
            const int ny = cy + dy[d];
            
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            
            const int neighbor = ny * width + nx;
            
            // Only visit unassigned (transparent) pixels
            if (sourceIdx[neighbor] == -1) {
                sourceIdx[neighbor] = sourceIdx[current];
                bfsQueue.push(neighbor);
            }
        }
    }
    
    // Apply colors from source pixels to transparent pixels
    for (int i = 0; i < totalPixels; ++i) {
        const int srcI = sourceIdx[i];
        if (pixels[i * 4 + 3] == 0 && srcI != -1 && srcI != i) {
            const int src = srcI * 4;
            const int dst = i * 4;
            pixels[dst + 0] = pixels[src + 0];
            pixels[dst + 1] = pixels[src + 1];
            pixels[dst + 2] = pixels[src + 2];
            // Alpha stays 0
        }
    }
}

// OPTIMIZATION: Precompute X-coordinate lookup table + use memcpy
std::vector<unsigned char> TextureManager::upscaleImage(const std::vector<unsigned char>& src, int srcW, int srcH, int dstW, int dstH) {
    if (srcW == dstW && srcH == dstH) return src;

    std::vector<unsigned char> dst(static_cast<size_t>(dstW) * dstH * 4);
    
    // Precompute source X for each destination X (avoid repeated float math)
    std::vector<int> srcXLookup(dstW);
    const float xRatio = static_cast<float>(srcW) / dstW;
    for (int x = 0; x < dstW; ++x) {
        srcXLookup[x] = static_cast<int>(x * xRatio);
    }
    
    const float yRatio = static_cast<float>(srcH) / dstH;
    const int srcStride = srcW * 4;
    
    for (int y = 0; y < dstH; ++y) {
        const int srcY = static_cast<int>(y * yRatio);
        const unsigned char* srcRow = src.data() + srcY * srcStride;
        unsigned char* dstRow = dst.data() + y * dstW * 4;
        
        for (int x = 0; x < dstW; ++x) {
            // Use memcpy for 4-byte copy (compiler can optimize to single 32-bit mov)
            std::memcpy(dstRow + x * 4, srcRow + srcXLookup[x] * 4, 4);
        }
    }
    return dst;
}

// OPTIMIZATION: Fixed-point integer tinting (avoid per-pixel float multiplication)
namespace {
    struct TintParams {
        uint32_t rMul, gMul, bMul;  // 8.8 fixed-point multipliers (0-256)
    };
    
    inline void applyTint(std::vector<unsigned char>& pixels, const TintParams& tint) {
        const size_t size = pixels.size();
        for (size_t i = 0; i < size; i += 4) {
            if (pixels[i + 3] > 0) {
                pixels[i + 0] = static_cast<unsigned char>((pixels[i + 0] * tint.rMul) >> 8);
                pixels[i + 1] = static_cast<unsigned char>((pixels[i + 1] * tint.gMul) >> 8);
                pixels[i + 2] = static_cast<unsigned char>((pixels[i + 2] * tint.bMul) >> 8);
            }
        }
    }
    
    inline void applyWaterTint(std::vector<unsigned char>& pixels, const TintParams& tint) {
        const size_t size = pixels.size();
        for (size_t i = 0; i < size; i += 4) {
            if (pixels[i + 3] > 0) {
                pixels[i + 0] = static_cast<unsigned char>((pixels[i + 0] * tint.rMul) >> 8);
                pixels[i + 1] = static_cast<unsigned char>((pixels[i + 1] * tint.gMul) >> 8);
                pixels[i + 2] = static_cast<unsigned char>((pixels[i + 2] * tint.bMul) >> 8);
                if (pixels[i + 3] < 180) {
                    pixels[i + 3] = 180;
                }
            }
        }
    }
    
    // Pre-computed tint values as fixed-point (avoid runtime float->int conversion)
    constexpr TintParams GREEN_TINT = {
        (145 * 256) / 255,  // ~145
        (189 * 256) / 255,  // ~189
        (89 * 256) / 255    // ~89
    };
    
    constexpr TintParams WATER_TINT = {
        (63 * 256) / 255,
        (118 * 256) / 255,
        (228 * 256) / 255
    };
    
    inline bool needsGreenTint(const std::string& name) {
        return name == "grass_block_top" || 
               name == "short_grass" || 
               name == "tall_grass_bottom" || 
               name == "tall_grass_top" || 
               name == "oak_leaves";
    }
}

void TextureManager::loadTextures(const std::string& directoryPath) {
    std::vector<TextureData> rawTextures;
    int maxWidth = 0;
    int maxHeight = 0;

    stbi_set_flip_vertically_on_load(true);

    if (!fs::exists(directoryPath)) {
        std::cerr << "Texture directory does not exist: " << directoryPath << std::endl;
        return; 
    }

    // OPTIMIZATION: Pre-allocate vector capacity
    rawTextures.reserve(64);

    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) {
            // OPTIMIZATION: Avoid string copy by using const ref to path extension
            const auto& ext = entry.path().extension();
            if (ext == ".png" || ext == ".jpg" || ext == ".tga") {
                std::string name = entry.path().stem().string();
                
                TextureData data = loadPixels(entry.path().string(), name);

                if (data.width > 0) {
                    maxWidth = std::max(maxWidth, data.width);
                    // Determine frame height (assume square frames if strip)
                    int frameHeight = (data.height > data.width) ? data.width : data.height;
                    maxHeight = std::max(maxHeight, frameHeight);
                    // OPTIMIZATION: Use emplace_back with move
                    rawTextures.emplace_back(std::move(data));
                }
            }
        }
    }

    if (rawTextures.empty()) {
        std::cerr << "No textures found in " << directoryPath << std::endl;
        return;
    }

    const int arrayWidth = std::max(maxWidth, maxHeight);
    const int arrayHeight = arrayWidth; 

    // Calculate total layers (including frames)
    size_t layerCount = 0;
    for (const auto& tex : rawTextures) {
        if (tex.height > tex.width && tex.height % tex.width == 0) {
            layerCount += (tex.height / tex.width);
        } else {
            layerCount++;
        }
    }

    const size_t layerSize = static_cast<size_t>(arrayWidth) * arrayHeight * 4;
    
    std::vector<unsigned char> textureArrayBuffer;
    // OPTIMIZATION: Reserve exact needed capacity upfront
    textureArrayBuffer.reserve(layerSize * layerCount);

    // Sort for deterministic layer ordering
    std::sort(rawTextures.begin(), rawTextures.end(), [](const TextureData& a, const TextureData& b) {
        return a.name < b.name;
    });

    uint16_t currentLayer = 0;
    for (auto& tex : rawTextures) {
        // OPTIMIZATION: Move pixels instead of copy (we're done with rawTextures after this)
        std::vector<unsigned char> currentPixels = std::move(tex.pixels);

        // Apply tinting using integer fixed-point math
        if (needsGreenTint(tex.name)) {
            applyTint(currentPixels, GREEN_TINT);
            std::cout << "Applied Green Tint to: " << tex.name << std::endl;
        } 
        else if (tex.name == "water_still") {
            applyWaterTint(currentPixels, WATER_TINT);
            std::cout << "Applied Blue Tint to: " << tex.name << std::endl;
        }

        // Fix texture bleeding with optimized O(n²) BFS
        fixTextureBleeding(currentPixels, tex.width, tex.height);

        // Handle frames
        int sourceFrames = 1;
        if (tex.height > tex.width && tex.height % tex.width == 0) {
            sourceFrames = tex.height / tex.width;
        }

        int targetFrames = sourceFrames;
        // PAD FLUIDS TO 32 FRAMES for shader compatibility
        // The shader expects 32 frames for animation. accessible via aLayerIndex + frame
        if (tex.name == "water_still" || tex.name == "lava_still") {
             targetFrames = 32;
        }

        int srcFrameHeight = tex.width; // Assume square frames
        size_t srcFrameSize = static_cast<size_t>(tex.width) * srcFrameHeight * 4;

        for (int f = 0; f < targetFrames; ++f) {
             int sourceFrameIndex = f % sourceFrames; // Wrap around for padding

             std::vector<unsigned char> framePixels;
             if (sourceFrames > 1) {
                 // Extract frame
                 auto start = currentPixels.begin() + (sourceFrameIndex * srcFrameSize);
                 auto end = start + srcFrameSize;
                 framePixels.assign(start, end);
             } else {
                 // Even if source is 1 frame, if we are padding to 32, we must COPY
                 if (targetFrames > 1) {
                     framePixels = currentPixels;
                 } else {
                     framePixels = std::move(currentPixels);
                 }
             }

             // Upscale if necessary
             std::vector<unsigned char> processedPixels = upscaleImage(framePixels, tex.width, srcFrameHeight, arrayWidth, arrayHeight);
             
             // Append to master buffer
             textureArrayBuffer.insert(textureArrayBuffer.end(), processedPixels.begin(), processedPixels.end());
        }

        textureMap[tex.name] = currentLayer;
        std::cout << "Loaded Texture: " << tex.name << " -> Layer " << currentLayer << " (Source: " << sourceFrames << ", Target: " << targetFrames << " frames)" << std::endl;
        currentLayer += targetFrames;
    }

    // Upload to OpenGL
    if (textureArrayID != 0) {
        glDeleteTextures(1, &textureArrayID);
    }
    
    glGenTextures(1, &textureArrayID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayID);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, arrayWidth, arrayHeight, static_cast<GLsizei>(layerCount),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, textureArrayBuffer.data());
    
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    std::cout << "Texture Array Generated. Size: " << arrayWidth << "x" << arrayHeight 
              << " Layers: " << layerCount << std::endl;
}
