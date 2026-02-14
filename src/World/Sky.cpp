#include "Sky.h"
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @file Sky.cpp
 * @brief Implementation of atmospheric day/night cycles and celestial rendering.
 */

namespace {
    /** @name Math Constants @{ */
    constexpr float TWO_PI = 2.0f * static_cast<float>(M_PI);
    constexpr float INV_TWO_PI = 1.0f / TWO_PI;
    /** @} */

    /** @name Color Palettes
     * Defined for different times of day to allow smooth interpolation.
     * @{
     */
    const glm::vec3 DAY_SKY_COLOR(0.5f, 0.75f, 1.0f);
    const glm::vec3 NIGHT_SKY_COLOR(0.02f, 0.02f, 0.05f);
    const glm::vec3 SUNSET_SKY_COLOR(0.9f, 0.5f, 0.2f);
    const glm::vec3 SUN_COLOR_BRIGHT(1.0f, 0.95f, 0.85f);
    const glm::vec3 SUN_COLOR_SUNSET(1.0f, 0.4f, 0.1f);
    /** @} */

    /** @name Lighting Thresholds @{ */
    constexpr float SUNSET_THRESHOLD = 0.3f;
    constexpr float INV_SUNSET_THRESHOLD = 1.0f / SUNSET_THRESHOLD;
    /** @} */

    /** @name Shared Geometry Data
     * A standard unit quad for billboard rendering.
     * @{
     */
    constexpr float QUAD_VERTICES[] = {
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };
    /** @} */
}

Sky::Sky() {}

Sky::~Sky() {
    delete skyShader;

    if (sunTexture) glDeleteTextures(1, &sunTexture);
    if (moonTexture) glDeleteTextures(1, &moonTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void Sky::init() {
    skyShader = new Shader("../assets/shaders/sky_vertex_shader.glsl",
                          "../assets/shaders/sky_fragment_shader.glsl");

    if (skyShader->program == 0) [[unlikely]] {
        std::cerr << "[Sky] Critical: Shader failed to compile!" << std::endl;
    }

    loadTexture("../assets/sprites/blocks/sun.png", sunTexture);
    loadTexture("../assets/sprites/blocks/moon.png", moonTexture);
    initQuad();

    // Initial calculation of the world's lighting state.
    updateCachedValues();
}

void Sky::loadTexture(const char* path, GLuint& texID) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);

    if (!data) [[unlikely]] {
        std::cerr << "[Sky] Failed to load texture: " << path << std::endl;
        return;
    }

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // use nearest filtering for that crisp voxel aesthetic.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

void Sky::initQuad() {
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

    // Layout: [Pos: 3f][UV: 2f]
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Sky::update(float deltaTime) {
    if (paused) [[unlikely]] return;

    // Advance time and loop at 1.0 (Midnight/Sunrise cycle).
    timeOfDay += deltaTime / dayLengthSeconds;
    if (timeOfDay >= 1.0f) [[unlikely]] {
        timeOfDay -= 1.0f;
    }

    // Single-pass lighting update.
    updateCachedValues();
}

void Sky::updateCachedValues() {
    // 1. Angular state
    cachedAngle = timeOfDay * TWO_PI;
    cachedCosAngle = std::cos(cachedAngle);
    cachedSinAngle = std::sin(cachedAngle);
    cachedSunHeight = -cachedCosAngle; // Maps cos range [-1, 1] to height range [1, -1] (noon=1)

    // 2. Global Light Direction (Sun -> Earth)
    cachedSunDirection = glm::normalize(glm::vec3(0.3f, cachedSunHeight, cachedSinAngle));

    // 3. Sun Color Interpolation
    if (cachedSunHeight > 0.0f) [[unlikely]] {
        // Night phase
        cachedSunColor = glm::vec3(0.0f);
    } else {
        const float intensity = -cachedSunHeight; // 0 at horizon, 1 at noon zenith

        if (intensity < SUNSET_THRESHOLD) [[unlikely]] {
            // Smoothly lerp towards a warm orange as the sun approaches the horizon.
            const float t = intensity * INV_SUNSET_THRESHOLD;
            cachedSunColor = glm::mix(SUN_COLOR_SUNSET, SUN_COLOR_BRIGHT, t);
        } else {
            cachedSunColor = SUN_COLOR_BRIGHT * intensity;
        }
    }

    // 4. Ambient Strength (Moonlight vs Sunlight)
    if (cachedSunHeight > 0.0f) [[unlikely]] {
        // Night: slight ambient boost when moon is high
        cachedAmbientStrength = 0.15f + 0.05f * (1.0f - cachedSunHeight);
    } else {
        // Day: peak ambient at noon
        cachedAmbientStrength = 0.3f + 0.15f * (-cachedSunHeight);
    }

    // 5. Sky Color Selection (Fog/ClearColor)
    if (cachedSunHeight > 0.1f) [[unlikely]] {
        cachedSkyColor = NIGHT_SKY_COLOR;
    } else if (cachedSunHeight > -0.1f) [[unlikely]] {
        // Night -> Sunset transition
        const float t = (cachedSunHeight + 0.1f) * 5.0f;
        cachedSkyColor = glm::mix(SUNSET_SKY_COLOR, NIGHT_SKY_COLOR, t);
    } else if (cachedSunHeight > -0.3f) [[unlikely]] {
        // Sunset -> Day transition
        const float t = (cachedSunHeight + 0.3f) * 5.0f;
        cachedSkyColor = glm::mix(DAY_SKY_COLOR, SUNSET_SKY_COLOR, t);
    } else {
        cachedSkyColor = DAY_SKY_COLOR;
    }
}

void Sky::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    if (!sunTexture || !moonTexture || !quadVAO || !skyShader || skyShader->program == 0) [[unlikely]] {
        return;
    }

    // Optimization: Use pre-cached direction vector.
    const glm::vec3& sunDir = cachedSunDirection;
    const glm::vec3 moonDir = -sunDir;

    // Calculate billboard Opacity (fade celestials near horizons)
    const float sunBrightness = (cachedSunHeight < 0.0f)
        ? glm::clamp(-cachedSunHeight * 2.0f, 0.0f, 1.0f) : 0.0f;
    const float moonBrightness = (cachedSunHeight > 0.0f)
        ? glm::clamp(cachedSunHeight * 2.0f, 0.0f, 0.8f) : 0.0f;

    if (sunBrightness <= 0.01f && moonBrightness <= 0.01f) [[unlikely]] {
        return;
    }

    // Batch GL state changes for celestial rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive blending for glow effect
    glDisable(GL_CULL_FACE);

    skyShader->use();
    (*skyShader)["view"] = view;
    (*skyShader)["projection"] = projection;

    // Drawcelestials opposite each other.
    if (sunBrightness > 0.01f) [[likely]] {
        renderBillboard(sunTexture, -sunDir, 80.0f, view, projection, cameraPos, sunBrightness);
    }
    if (moonBrightness > 0.01f) [[likely]] {
        renderBillboard(moonTexture, -moonDir, 60.0f, view, projection, cameraPos, moonBrightness);
    }

    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Sky::renderBillboard(GLuint texture, const glm::vec3& direction, float size,
                          const glm::mat4& view, const glm::mat4& projection,
                          const glm::vec3& cameraPos, float brightness) {
    // Project the billboard far into the distance.
    const glm::vec3 billboardPos = cameraPos + direction * 500.0f;

    // Extract basis vectors from the view matrix to create a camera-facing billboard.
    const glm::vec3 right(view[0][0], view[1][0], view[2][0]);
    const glm::vec3 up(view[0][1], view[1][1], view[2][1]);
    const glm::vec3 forward = glm::normalize(cameraPos - billboardPos);

    // Build the model matrix manually (Column-major logic).
    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right * size, 0.0f);
    model[1] = glm::vec4(up * size, 0.0f);
    model[2] = glm::vec4(forward * size, 0.0f);
    model[3] = glm::vec4(billboardPos, 1.0f);

    (*skyShader)["model"] = model;
    (*skyShader)["brightness"] = brightness;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture);
    (*skyShader)["skyTex"] = 1;

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}
