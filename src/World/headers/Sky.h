/**
 * @file Sky.h
 * @brief Logic for the sky, day/night cycle, and sun/moon.
 */

#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Shader.h>

/**
 * @class Sky
 * @brief Manages the sky color and positions of the sun and moon.
 * 
 * This class keeps track of the time of day and updates the light 
 * and colors in the world to match.
 */
class Sky {
public:
    /**
     * @brief Constructor. Sets default time and cycle parameters.
     */
    Sky();
    
    /**
     * @brief Destructor. Cleans up OpenGL textures, buffers, and shaders.
     */
    ~Sky();

    /**
     * @brief Initialize celestial textures, shader programs, and quad geometry.
     */
    void init();

    /**
     * @brief Advances the internal clock and triggers a recalculation of cached lighting values.
     * @param deltaTime Time elapsed since last frame in seconds.
     */
    void update(float deltaTime);

    /**
     * @brief Renders the sun and moon billboards into the scene.
     * 
     * Uses additive blending and high-distance translation to ensure celestials appear 
     * behind all world geometry while following the camera.
     * 
     * @param view The current camera view matrix.
     * @param projection The current camera projection matrix.
     * @param cameraPos The world-space position of the camera (used for billboard anchoring).
     */
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    /** @name Environmental Lighting Parameters (Cached)
     * These values are computed once per frame during update() to minimize CPU overhead.
     * @{
     */
    [[nodiscard]] inline glm::vec3 getSunDirection() const noexcept { return cachedSunDirection; }
    [[nodiscard]] inline glm::vec3 getSunColor() const noexcept { return cachedSunColor; }
    [[nodiscard]] inline float getAmbientStrength() const noexcept { return cachedAmbientStrength; }
    [[nodiscard]] inline glm::vec3 getSkyColor() const noexcept { return cachedSkyColor; }
    /** @} */

    /**
     * @brief The current clock state as a normalized float.
     * 0.0 = Noon, 0.25 = Sunset, 0.5 = Midnight, 0.75 = Sunrise.
     */
    float timeOfDay = 0.75f;

    /// @brief Duration of a full 24-hour cycle in real-world seconds.
    float dayLengthSeconds = 600.0f;

    /** @name Time Control @{ */
    void togglePause() noexcept { paused = !paused; }
    [[nodiscard]] bool isPaused() const noexcept { return paused; }
    /** @} */

private:
    /**
     * Frequent variables are kept together to help with speed.
     */

    /** @name Cached Lighting Results @{ */
    glm::vec3 cachedSunDirection{0.3f, -1.0f, 0.0f}; ///< Direction vector for shaders.
    glm::vec3 cachedSunColor{1.0f, 0.95f, 0.85f};     ///< Sunbeam intensity/color.
    glm::vec3 cachedSkyColor{0.5f, 0.75f, 1.0f};      ///< Clear-color/Fog color.
    float cachedAmbientStrength = 0.45f;              ///< Minimum scene brightness.
    /** @} */

    /** @name Cached Trigonometric Results @{ */
    float cachedAngle = 0.0f;       ///< Current celestial orbital angle in radians.
    float cachedSunHeight = -1.0f;  ///< Normalized Y component of sun position.
    float cachedCosAngle = -1.0f;   ///< cos(cachedAngle)
    float cachedSinAngle = 0.0f;    ///< sin(cachedAngle)
    /** @} */

    bool paused = false; ///< If true, time progression is halted.

    /** @name Resource Data
     * Handles for textures and shaders.
     * @{
     */
    Shader* skyShader = nullptr;
    GLuint sunTexture = 0;
    GLuint moonTexture = 0;
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    /** @} */

    /**
     * @brief Loads a single image file into an OpenGL texture with nearest filtering.
     */
    void loadTexture(const char* path, GLuint& texID);

    /**
     * @brief Generates the shared 2D quad geometry used for both sun and moon.
     */
    void initQuad();

    /**
     * @brief Centralized logic for interpolating lighting and colors based on the sun's height.
     * This is called automatically by update().
     */
    void updateCachedValues();

    /**
     * @brief Internal helper to render a single billboard at a fixed distance.
     * Performs camera-facing rotation and applies brightness scaling.
     */
    void renderBillboard(GLuint texture, const glm::vec3& direction, float size,
                         const glm::mat4& view, const glm::mat4& projection,
                         const glm::vec3& cameraPos, float brightness);
};
