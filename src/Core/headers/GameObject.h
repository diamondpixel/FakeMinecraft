#pragma once

#include <Graphics.h>
#include "Camera.h"
#include "Planet.h"
#include <Shader.h>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include "CheckBox.h"
#include "Physics.h"
#include "Slider.h"
#include "TextureManager.h"
#include "TypeBox.h"
#include "Sky.h"

// Forward declarations
class Chunk;

// Utility function moved to implementation
int convertToASCIIsum(const std::string &input);

// Game state enum
enum GameStateEnum {
    PLAYING,
    PAUSED
};

struct GameState {
    GameStateEnum state = PAUSED;
    uint16_t selectedBlock = 1;
};

// ============================================================================
// OPTIMIZED GAME OBJECT CLASS
// ============================================================================
class GameObject {
public:
    // Singleton access
    static GameObject &getInstance(float x, float y, const std::string &windowName);

    // Main initialization
    void init();

    // Prevent copying
    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;

private:
    // Constructor/Destructor
    GameObject(float x, float y, std::string windowName);
    ~GameObject();

    // Initialization methods
    void initializeGraphics();
    void initializeShaders();
    void initializeUIElements();
    void initializeOutlineVAO();

    // Update methods
    void updateShaders();
    void setupRenderingState();
    static void updateWindowTitle();

    // Input handling
    void mouseCallBack();
    void keyboardCallBack(float deltaTime);
    void handlePlayingMouseInput(graphics::MouseState &ms);
    void handlePausedMouseInput(graphics::MouseState &ms);

    // Rendering
    void renderUI() const;
    void renderBlockOutline(unsigned int outlineVAO);
    void renderFluidOverlay() const;
    void renderCrosshair() const;
    void renderDebugInfo() const;
    void renderPauseMenu() const;

    // Block operations
    void handleBlockBreak(const Physics::RaycastResult &result);
    void handleBlockPlace(const Physics::RaycastResult &result);
    void handleBlockPick(const Physics::RaycastResult &result);

    // Utility
    static void playSound(uint16_t block_id);
    void updateFPSSettings() const;

    // Member variables - grouped by usage
    Camera camera;
    GameState gameState;

    // Shaders
    // Shaders
    Shader* worldShader = nullptr;
    Shader* billboardShader = nullptr;
    Shader* fluidShader = nullptr;
    Shader* outlineShader = nullptr;
    Shader* bboxShader = nullptr;

    // Sky system
    Sky sky;

    // UI Elements
    Checkbox fullscreenCheckBox;
    Slider fpsSlider;
    Slider renderDistanceSlider;
    TypeBox seedBox;

    // Window state
    float windowX, windowY;
    float lastX, lastY;
    std::string window_name;

    // Settings
    int fpsCap = 55;
    
    // Freecam mode
    bool freecamActive = false;
    glm::vec3 savedPlayerPosition;
    glm::mat4 savedViewProjection; // Frozen frustum during freecam

    // Resources
    graphics::TextureManager *textureManager = nullptr;

    unsigned int outlineVAO = 0;
    unsigned int outlineVBO = 0;

    // Cached values for performance
    struct CachedMatrices {
        glm::mat4 view;
        glm::mat4 projection;
        bool dirty = true;
    } cachedMatrices;
};

// ============================================================================
// CONSTANTS
// ============================================================================
namespace GameConstants {
    constexpr float CROSSHAIR_SIZE = 5.0f;
    constexpr float CROSSHAIR_THICKNESS = 25.0f;
    constexpr float MOVEMENT_SPEED_MULTIPLIER = 0.02f;
    constexpr float RAYCAST_DISTANCE = 5.0f;
    constexpr float MUSIC_VOLUME = 0.8f;
    constexpr int FPS_VSYNC_THRESHOLD = 55;
    constexpr int FPS_MAX_CAPPED = 360;

    // Outline vertices as constexpr array
    constexpr float OUTLINE_VERTICES[] = {
        -.001f, -.001f, -.001f, 1.001f, -.001f, -.001f,
        1.001f, -.001f, -.001f, 1.001f, 1.001f, -.001f,
        1.001f, 1.001f, -.001f, -.001f, 1.001f, -.001f,
        -.001f, 1.001f, -.001f, -.001f, -.001f, -.001f,

        -.001f, -.001f, -.001f, -.001f, -.001f, 1.001f,
        -.001f, -.001f, 1.001f, -.001f, 1.001f, 1.001f,
        -.001f, 1.001f, 1.001f, -.001f, 1.001f, -.001f,

        1.001f, -.001f, -.001f, 1.001f, -.001f, 1.001f,
        1.001f, -.001f, 1.001f, 1.001f, 1.001f, 1.001f,
        1.001f, 1.001f, 1.001f, 1.001f, 1.001f, -.001f,

        -.001f, -.001f, 1.001f, 1.001f, -.001f, 1.001f,
        -.001f, 1.001f, 1.001f, 1.001f, 1.001f, 1.001f,
    };
}