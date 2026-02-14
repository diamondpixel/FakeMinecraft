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
// GAME OBJECT CLASS
// ============================================================================
/**
 * @class GameObject
 * @brief The central controller class for the game engine, following the Singleton pattern.
 * 
 * GameObject manages the main game loop, initialization of all subsystems (Graphics, Shaders, UI, Planet),
 * and high-level logic like input handling, rendering orchestration, and block interaction.
 * 
 * The class layout is optimized for cache efficiency by grouping frequently accessed data 
 * ("hot" data) like the Camera and GameState at the top, while keeping "cold" data 
 * like resource paths at the bottom.
 */
class GameObject {
public:
    /**
     * @brief Gets the singleton instance of the GameObject.
     * @param x Initial window width.
     * @param y Initial window height.
     * @param windowName Title of the game window.
     * @return Reference to the GameObject instance.
     */
    static GameObject &getInstance(float x, float y, const std::string &windowName);

    /**
     * @brief Initializes the entire game world and enters the main loop.
     * 
     * Sets up graphics, shaders, UI elements, the planet, and binds all necessary 
     * callbacks for rendering and logic updates.
     */
    void init();

    // Prevent copying to maintain Singleton integrity
    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;

private:
    /**
     * @brief Constructor for GameObject.
     * @param x Initial window width.
     * @param y Initial window height.
     * @param windowName Name of the window.
     */
    GameObject(float x, float y, std::string windowName);
    
    /**
     * @brief Destructor for GameObject. Handles resource cleanup including VAOs/VBOs and Shaders.
     */
    ~GameObject();

    /** @name Initialization Helpers @{ */
    void initializeGraphics();
    void initializeShaders();
    void initializeUIElements();
    void initializeOutlineVAO();
    /** @} */

    /** @name Live Update Systems @{ */
    /// Updates all active shaders with current frame matrices and lighting data.
    void updateShaders();
    /// Configures global OpenGL state (Depth, Blending, Culling) for the current frame.
    void setupRenderingState() const;
    /// Updates the window title with performance metrics (FPS, Chunk Count).
    static void updateWindowTitle();
    /// Synchronizes the graphics engine with the user-selected FPS cap or VSync.
    void updateFPSSettings() const;
    /** @} */

    /** @name Input Callbacks @{ */
    void mouseCallBack();
    void keyboardCallBack(float deltaTime);
    void handlePlayingMouseInput(const graphics::MouseState &ms);
    void handlePausedMouseInput(const graphics::MouseState &ms);
    /** @} */

    /** @name Rendering Layers @{ */
    void renderUI() const;
    void renderBlockOutline(unsigned int outlineVAO);
    void renderFluidOverlay() const;
    void renderCrosshair() const;
    void renderDebugInfo() const;
    void renderPauseMenu() const;
    /** @} */

    /** @name Block Interaction Logic @{ */
    static void handleBlockBreak(const Physics::RaycastResult &result);
    void handleBlockPlace(const Physics::RaycastResult &result) const;
    void handleBlockPick(const Physics::RaycastResult &result);
    /** @} */

    /** @name Audio Utilities @{ */
    static void playSound(uint16_t block_id);
    /** @} */

    // ========================================================================
    // MEMBER VARIABLES - Optimized layout for cache efficiency
    // ========================================================================

    /// @brief Primary camera instance. Most frequently accessed during matrix updates.
    Camera camera;
    
    /// @brief Holds the current play state (Paused/Playing) and UI selection.
    GameState gameState;

    /** @brief Matrix caching system to avoid redundant perspective/view calculations. */
    struct CachedMatrices {
        glm::mat4 view;
        glm::mat4 projection;
        bool dirty = true; ///< Set to true whenever the camera moves or window resizes.
    } cachedMatrices;

    /** @brief Cached UI center points to avoid repeated division in rendering code. */
    struct {
        float x = 0.0f;
        float y = 0.0f;
    } cachedWindowCenter;

    /** @name Screen Dimensions @{ */
    float windowX, windowY;
    float lastX, lastY;
    /** @} */

    /// @brief Target frame rate (55 = VSync, 361 = Uncapped).
    int fpsCap = 55;

    /** @name Debug/Freecam State @{ */
    bool freecamActive = false;
    glm::vec3 savedPlayerPosition;
    glm::mat4 savedViewProjection;
    /** @} */

    /** @name Shader Handles @{ */
    Shader* worldShader = nullptr;
    Shader* billboardShader = nullptr;
    Shader* fluidShader = nullptr;
    Shader* outlineShader = nullptr;
    Shader* bboxShader = nullptr;
    /** @} */

    /// @brief Manages the day/night cycle and atmospheric rendering.
    Sky sky;

    /** @name UI Components @{ */
    Checkbox fullscreenCheckBox;
    Slider fpsSlider;
    Slider renderDistanceSlider;
    TypeBox seedBox;
    /** @} */

    /** @name Resource Identifiers @{ */
    std::string window_name;
    unsigned int outlineVAO = 0;
    unsigned int outlineVBO = 0;
    /** @} */
};

// ============================================================================
// CONSTANTS - constexpr for compile-time evaluation
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