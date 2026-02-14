#include "Sky.h"
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Sky::Sky() {}

Sky::~Sky() {
    if (skyShader) delete skyShader;
    if (sunTexture) glDeleteTextures(1, &sunTexture);
    if (moonTexture) glDeleteTextures(1, &moonTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void Sky::init() {
    skyShader = new Shader("../assets/shaders/sky_vertex_shader.glsl",
                       "../assets/shaders/sky_fragment_shader.glsl");
    
    if (skyShader->program == 0) {
        std::cerr << "[Sky] Critical: Shader failed to compile!" << std::endl;
    }

    loadTexture("../assets/sprites/blocks/sun.png", sunTexture);
    loadTexture("../assets/sprites/blocks/moon.png", moonTexture);
    initQuad();
}

void Sky::loadTexture(const char* path, GLuint& texID) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "[Sky] Failed to load texture: " << path << std::endl;
        return;
    }

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
}

void Sky::initQuad() {
    float vertices[] = {
        // positions       // texcoords
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,

         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Sky::update(float deltaTime) {
    if (paused) return;

    // Advance time of day
    timeOfDay += deltaTime / dayLengthSeconds;
    if (timeOfDay >= 1.0f) timeOfDay -= 1.0f;
}

glm::vec3 Sky::getSunDirection() const {
    // Sun revolves around the X-axis going overhead
    // 0.0 = Noon (straight down from above) -> direction = (0.8, -1, 0.7) normalized
    // 0.25 = Sunset
    // 0.5 = Midnight (sun below horizon)
    // 0.75 = Sunrise
    float angle = timeOfDay * 2.0f * static_cast<float>(M_PI);

    // Sun direction: rotates in the Y-Z plane, offset in X for more interesting lighting
    float sunY = -cos(angle);  // -1 at noon (pointing down), +1 at midnight
    float sunZ = sin(angle);
    return glm::normalize(glm::vec3(0.3f, sunY, sunZ));
}

glm::vec3 Sky::getSunColor() const {
    // Brightest at noon, reddish at sunrise/sunset, off at night
    float angle = timeOfDay * 2.0f * static_cast<float>(M_PI);
    float sunHeight = -cos(angle); // -1 at noon (sun overhead), +1 at midnight

    if (sunHeight > 0.0f) {
        // Night - no sun
        return glm::vec3(0.0f);
    }

    float intensity = -sunHeight; // 0 at horizon, 1 at noon

    // Sunset/sunrise coloring
    if (intensity < 0.3f) {
        float t = intensity / 0.3f;
        return glm::mix(glm::vec3(1.0f, 0.4f, 0.1f), glm::vec3(1.0f, 0.95f, 0.85f), t);
    }

    return glm::vec3(1.0f, 0.95f, 0.85f) * intensity;
}

float Sky::getAmbientStrength() const {
    float angle = timeOfDay * 2.0f * static_cast<float>(M_PI);
    float sunHeight = -cos(angle);

    // Ambient ranges from 0.15 (night) to 0.45 (noon)
    if (sunHeight > 0.0f) {
        // Night
        return 0.15f + 0.05f * (1.0f - sunHeight); // moonlight
    }
    // Day
    return 0.3f + 0.15f * (-sunHeight);
}

glm::vec3 Sky::getSkyColor() const {
    float angle = timeOfDay * 2.0f * static_cast<float>(M_PI);
    float sunHeight = -cos(angle);

    // Day: blue sky, Night: dark blue/black, Sunset: orange
    glm::vec3 dayColor(0.5f, 0.75f, 1.0f);
    glm::vec3 nightColor(0.02f, 0.02f, 0.05f);
    glm::vec3 sunsetColor(0.9f, 0.5f, 0.2f);

    if (sunHeight > 0.1f) {
        // Night
        return nightColor;
    } else if (sunHeight > -0.1f) {
        // Sunset/sunrise transition
        float t = (sunHeight - (-0.1f)) / 0.2f; // 0 at day, 1 at night
        return glm::mix(sunsetColor, nightColor, t);
    } else if (sunHeight > -0.3f) {
        float t = (sunHeight - (-0.3f)) / 0.2f; // 0 at full day, 1 at sunset
        return glm::mix(dayColor, sunsetColor, t);
    }

    return dayColor;
}

void Sky::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) {
    if (!sunTexture || !moonTexture || !quadVAO || !skyShader || skyShader->program == 0) return;

    // Get sun/moon direction for positioning
    float angle = timeOfDay * 2.0f * static_cast<float>(M_PI);
    float sunHeight = -cos(angle);

    // Sun direction in world space (opposite of light direction for rendering position)
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, -cos(angle), sin(angle)));
    glm::vec3 moonDir = -sunDir; // Moon is opposite to sun

    // Sun brightness: visible when above horizon
    float sunBrightness = (sunHeight < 0.0f) ? glm::clamp(-sunHeight * 2.0f, 0.0f, 1.0f) : 0.0f;
    // Moon brightness: visible when sun is below
    float moonBrightness = (sunHeight > 0.0f) ? glm::clamp(sunHeight * 2.0f, 0.0f, 0.8f) : 0.0f;

    // Render with depth test disabled (always behind everything) and blending
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    // Use additive blending to make black background transparent
    glBlendFunc(GL_ONE, GL_ONE); 
    glDisable(GL_CULL_FACE);

    skyShader->use();
    (*skyShader)["view"] = view;
    (*skyShader)["projection"] = projection;

    if (sunBrightness > 0.01f) {
        renderBillboard(sunTexture, -sunDir, 80.0f, view, projection, cameraPos, sunBrightness);
    }
    if (moonBrightness > 0.01f) {
        renderBillboard(moonTexture, -moonDir, 60.0f, view, projection, cameraPos, moonBrightness);
    }

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Restore standard alpha blending
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Sky::renderBillboard(GLuint texture, const glm::vec3& direction, float size,
                          const glm::mat4& view, const glm::mat4& projection,
                          const glm::vec3& cameraPos, float brightness) {
    // Position the billboard far away in the given direction
    glm::vec3 billboardPos = cameraPos + direction * 500.0f;

    // Build a model matrix that makes the billboard face the camera
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, billboardPos);

    // Extract camera right and up vectors from view matrix for billboard orientation
    glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
    glm::vec3 up = glm::vec3(view[0][1], view[1][1], view[2][1]);

    // Build rotation matrix to face camera
    glm::mat4 billboardRot = glm::mat4(1.0f);
    billboardRot[0] = glm::vec4(right * size, 0.0f);
    billboardRot[1] = glm::vec4(up * size, 0.0f);
    billboardRot[2] = glm::vec4(glm::normalize(cameraPos - billboardPos) * size, 0.0f);

    model = model * billboardRot;

    (*skyShader)["model"] = model;
    (*skyShader)["brightness"] = brightness;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture);
    (*skyShader)["skyTex"] = 1;

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Restore texture binding
    glActiveTexture(GL_TEXTURE0);
}
