/**
 * @file Sky.h
 * @brief Manage atmospheric effects, the day/night cycle, and celestial rendering.
 */

#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Shader.h>

/**
 * @class Sky
 * @brief Handles the background sky color, sun/moon positions, and global ambient lighting.
 * 
 * The Sky class provides the "Time of Day" logic which affects:
 * 1. **Clear Color**: The background color of the OpenGL context.
 * 2. **Lighting**: Sun direction and color passed to chunk shaders.
 * 3. **Celestial Bodies**: Rendering of sun and moon billboards.
 */
class Sky {
public:
    /**
     * @brief Constructor. Sets default time and speeds.
     */
    Sky();
    
    /**
     * @brief Destructor. Cleans up OpenGL textures and VAOs.
     */
    ~Sky();

    /**
     * @brief Initialized celestial textures and geometry.
     */
    void init();

    /**
     * @brief Advances the internal clock.
     * @param deltaTime Time elapsed since last frame in seconds.
     */
    void update(float deltaTime);

    /**
     * @brief Renders the sun and moon into the scene.
     * @param view The current camera view matrix.
     * @param projection The current camera projection matrix.
     * @param cameraPos The world-space position of the camera.
     */
    void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

    /** @name Environmental Lighting Parameters
     * Calculated based on the current time of day.
     * @{
     */
    glm::vec3 getSunDirection() const;  ///< Unit vector pointing towards the sun.
    glm::vec3 getSunColor() const;      ///< Light intensity and color of sunbeams.
    float getAmbientStrength() const;   ///< Global minimum light level (higher at noon).
    glm::vec3 getSkyColor() const;      ///< Current background clear color.
    /** @} */

    /**
     * @brief The current clock state.
     * 0.0 = Noon, 0.25 = Sunset, 0.5 = Midnight, 0.75 = Sunrise.
     */
    float timeOfDay = 0.0f;

    /// @brief Duration of a full cycle in real-world seconds.
    float dayLengthSeconds = 600.0f; 

    /** @name Time Control @{ */
    void togglePause() { paused = !paused; }
    bool isPaused() const { return paused; }
    /** @} */

private:
    bool paused = false; ///< If true, timeOfDay does not advance.
    
    /**
     * @brief Loads a single image as an OpenGL texture.
     */
    void loadTexture(const char* path, GLuint& texID);

    Shader* skyShader = nullptr; ///< Shader used for billboard rendering.
    GLuint sunTexture = 0;       ///< Texture ID for the sun.
    GLuint moonTexture = 0;      ///< Texture ID for the moon.
    GLuint quadVAO = 0, quadVBO = 0; ///< shared quad geometry for billboards.

    /**
     * @brief Generates the 2D quad mesh used for sun/moon billboards.
     */
    void initQuad();

    /**
     * @brief Internal helper to draw a textured quad at a fixed distance from the camera.
     */
    void renderBillboard(GLuint texture, const glm::vec3& direction, float size,
                         const glm::mat4& view, const glm::mat4& projection, 
                         const glm::vec3& cameraPos, float brightness);
};
