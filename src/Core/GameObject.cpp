#include "GameObject.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <utility>
#include <SDL2/SDL_mouse.h>
#include "TextureManager.h"
#include "Blocks.h"
#include "../World/Generation/BiomeRegistry.h"
#include "SDL2/SDL_timer.h"

/**
 * @file GameObject.cpp
 * @brief Implementation of the central game controller and main engine logic.
 */

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
namespace {
    /**
     * @brief Compile-time constant string hashing for fast block name comparisons.
     * Uses the DJB2 algorithm.
     */
    constexpr uint32_t hashCString(const char *str) {
        uint32_t hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(*str++);
        }
        return hash;
    }

    // Pre-computed block name hashes to avoid runtime handle lookups
    constexpr uint32_t HASH_WATER = hashCString("WATER");
    constexpr uint32_t HASH_LAVA = hashCString("LAVA");
    constexpr uint32_t HASH_TALL_GRASS_BOTTOM = hashCString("TALL_GRASS_BOTTOM");
    constexpr uint32_t HASH_TALL_GRASS_TOP = hashCString("TALL_GRASS_TOP");
    constexpr uint32_t HASH_GRASS_BLOCK = hashCString("GRASS_BLOCK");

    /**
     * @brief Converts simple block coordinates to chunk-relative coordinates.
     * This method correctly handles the negative coordinate wrapping logic.
     */
    [[gnu::always_inline]]
    int worldToChunkCoord(const int blockCoord, const int chunkSize) {
        return (blockCoord >= 0) ? (blockCoord / chunkSize) : ((blockCoord + 1) / chunkSize - 1);
    }

    /**
     * @brief High-performance branchless floor function for float-to-block-int conversion.
     */
    [[gnu::always_inline]]
    int fastBlockFloor(const float coord) {
        const int i = static_cast<int>(coord);
        return (coord < 0.0f && coord != i) ? i - 1 : i;
    }

    /**
     * @brief Utility structure to store and update chunk/local coordinates from world position.
     * Used to avoid repeated calculations in different parts of the UI layer.
     */
    struct ChunkLocalCoords {
        int chunkX, chunkY, chunkZ;
        int localX, localY, localZ;

        void update(const glm::vec3 &worldPos) {
            const int blockX = fastBlockFloor(worldPos.x);
            const int blockY = fastBlockFloor(worldPos.y);
            const int blockZ = fastBlockFloor(worldPos.z);

            chunkX = worldToChunkCoord(blockX, CHUNK_WIDTH);
            chunkY = worldToChunkCoord(blockY, CHUNK_HEIGHT);
            chunkZ = worldToChunkCoord(blockZ, CHUNK_WIDTH);

            localX = blockX - chunkX * CHUNK_WIDTH;
            localY = blockY - chunkY * CHUNK_HEIGHT;
            localZ = blockZ - chunkZ * CHUNK_WIDTH;
        }
    };
}

int convertToASCIIsum(const std::string &input) {
    int sum = 0;
    for (const char c: input) {
        const int charVal = static_cast<unsigned char>(c);
        //Use saturating arithmetic
        if (sum > INT_MAX - charVal) return INT_MAX;
        sum += charVal;
    }
    return sum;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================
GameObject::GameObject(const float x, const float y, std::string windowName)
    : windowX(x), windowY(y), lastX(x / 2), lastY(y / 2), fpsSlider(), renderDistanceSlider(), seedBox(),
      window_name(std::move(windowName)) {
}

GameObject::~GameObject() {
    if (outlineVAO)
        glDeleteVertexArrays(1, &outlineVAO);
    if (outlineVBO)
        glDeleteBuffers(1, &outlineVBO);

    delete worldShader;
    delete billboardShader;
    delete fluidShader;
    delete outlineShader;
    delete bboxShader;
    delete shadowShader;

    glDeleteFramebuffers(1, &multisampledFBO);
    glDeleteTextures(1, &screenTexture); // wait, screenTexture is attached to intermediateFBO
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &intermediateFBO);

    graphics::stopMessageLoop();
    graphics::destroyWindow();
}

GameObject &GameObject::getInstance(float x, float y, const std::string &windowName) {
    static GameObject instance(x, y, windowName);
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void GameObject::init() {
    initializeGraphics();
    initializeShaders();
    initializeUIElements();
    initializeOutlineVAO();

    Planet::planet = new Planet(worldShader, fluidShader, billboardShader, bboxShader);

    // Cache frequently accessed values
    cachedWindowCenter.x = windowX * 0.5f;
    cachedWindowCenter.y = windowY * 0.5f;

    // Setup callbacks with optimized lambdas
    // Setup callbacks with optimized lambdas
    graphics::setPreDrawFunction([ this] {
        sky.update(1.0f / std::max(graphics::getFPS(), 1.0f));
        //sky.update(0.1);
        updateWindowTitle();
        
        // 0. Reflection Pass
        // -----------------------------------------------------------------
        Planet::planet->renderReflection(
            camera.Position, 
            camera.Front, 
            static_cast<float>(windowX) / static_cast<float>(windowY)
        );
        
        // Reset Viewport for Shadow Pass
        // Shadow pass sets its own viewport, but we should be careful.
        
        // 1. Shadow Pass
        // -----------------------------------------------------------------
        const float shadowDist = Planet::planet->shadowDistance;
        
        // Position light source based on sky/sun
        glm::vec3 sunTo = -sky.getSunDirection();
        
        // QUANTIZE SUN DIRECTION: Snap to discrete angles to prevent jitter during sun movement
        // Without this, continuous sun rotation causes light-space basis to rotate → texel grid rotates → jitter
        const float angleQuantization = glm::radians(0.5f); // 0.5 degree steps
        
        // Convert to spherical coordinates
        float sunLength = glm::length(sunTo);
        if (sunLength > 0.0001f) {
            glm::vec3 sunNorm = sunTo / sunLength;
            
            // Calculate azimuth (XZ plane angle) and elevation (Y angle)
            float azimuth = std::atan2(sunNorm.z, sunNorm.x);
            float elevation = std::asin(sunNorm.y);
            
            // Quantize angles
            azimuth = std::round(azimuth / angleQuantization) * angleQuantization;
            elevation = std::round(elevation / angleQuantization) * angleQuantization;
            
            // Reconstruct quantized sun direction
            float cosElev = std::cos(elevation);
            sunTo = glm::vec3(
                std::cos(azimuth) * cosElev,
                std::sin(elevation),
                std::sin(azimuth) * cosElev
            ) * sunLength;
        }
        
        // Choose up vector that avoids degeneracy when sun is near zenith
        glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(glm::normalize(sunTo), lightUp)) > 0.99f) {
            lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        // TEXEL SNAPPING: Snap shadow frustum center to texel grid
        // Calculate world-space size of one shadow texel
        const float worldUnitsPerTexel = (2.0f * shadowDist) / static_cast<float>(Planet::planet->SHADOW_WIDTH);
        
        // Build orthonormal basis for light space (matching lookAt)
        glm::vec3 lightDir = glm::normalize(-sunTo);
        glm::vec3 lightRight = glm::normalize(glm::cross(lightUp, lightDir));
        glm::vec3 lightFinalUp = glm::cross(lightDir, lightRight);
        
        // Project camera position onto light-space XY plane (ignore Z, that's depth)
        glm::vec3 shadowCenter = camera.Position;
        float centerX = glm::dot(shadowCenter, lightRight);
        float centerY = glm::dot(shadowCenter, lightFinalUp);
        
        // Snap to texel grid in world space
        centerX = glm::floor(centerX / worldUnitsPerTexel) * worldUnitsPerTexel;
        centerY = glm::floor(centerY / worldUnitsPerTexel) * worldUnitsPerTexel;
        
        // Reconstruct snapped world position (preserve depth component)
        float centerZ = glm::dot(shadowCenter, lightDir);
        glm::vec3 snappedCenter = lightRight * centerX + lightFinalUp * centerY + lightDir * centerZ;
        
        // Build view matrix with snapped center
        const glm::mat4 lightView = glm::lookAt(snappedCenter + sunTo * 100.0f, snappedCenter, lightUp);
        const glm::mat4 lightProjection = glm::ortho(-shadowDist, shadowDist, -shadowDist, shadowDist, -4000.0f, 4000.0f);
        
        Planet::planet->lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, Planet::planet->SHADOW_WIDTH, Planet::planet->SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, Planet::planet->depthMapFBO);
        glDepthMask(GL_TRUE); // Required to ensure glClear works!
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glClearDepth(1.0f); // Shadows need depth 1.0 (far), unlike main pass (0.0/reversed-z or standard)
        glClear(GL_DEPTH_BUFFER_BIT);
        glDepthFunc(GL_LESS);  // Shadow map uses standard depth
        
        // Disable culling to ensure all geometry casts shadows (especially thin pillars)
        glDisable(GL_CULL_FACE);
        
        // Polygon offset to prevent shadow acne (self-shadowing stripes)
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 2.0f);

        // Bind Texture Array for Beta Alpha Testing in Shadow Shader
        // Note: This relies on shadowShader using binding=0 for 'tex' or being set via uniform
        glActiveTexture(GL_TEXTURE0);
        const GLuint texArrayID = TextureManager::getInstance().getTextureArrayID();
        glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);

        shadowShader->use();
        (*shadowShader)["lightSpaceMatrix"] = Planet::planet->lightSpaceMatrix;
        Planet::planet->renderShadows(shadowShader);
        
        
        glDisable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_CULL_FACE); // Re-enable for main rendering

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. MSAA Pass
        // -----------------------------------------------------------------
        glViewport(0, 0, windowX, windowY);
        glBindFramebuffer(GL_FRAMEBUFFER, multisampledFBO);
        glClearColor(sky.getSkyColor().r, sky.getSkyColor().g, sky.getSkyColor().b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Batch texture binding
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);

        // Bind shadow map
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Planet::planet->depthMap);

        // Bind reflection map
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Planet::planet->reflectionTexture);

        setupRenderingState(); // Reset state (depth test, etc)
        updateFPSSettings();
        updateShaders(); // Updates lightSpaceMatrix uniform in world shader
        
        // Disable clipping for main pass - Use (0,0,0,1) to ensure dot product is positive (1.0)
        // Using (0,0,0,0) results in gl_ClipDistance = 0.0 which is undefined/z-fighting boundary on some drivers.
        worldShader->use();
        (*worldShader)["clipPlane"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        billboardShader->use();
        (*billboardShader)["clipPlane"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        sky.render(cachedMatrices.view, cachedMatrices.projection, camera.Position);

        // CRITICAL: sky.render() overwrites GL_TEXTURE1 with sun/moon texture.
        // Re-bind shadow map AFTER sky rendering!
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Planet::planet->depthMap);
        glActiveTexture(GL_TEXTURE0);

        const glm::vec3 &updatePos = freecamActive ? savedPlayerPosition : camera.Position;
        Planet::planet->update(updatePos, !freecamActive);
        renderBlockOutline(outlineVAO);

        // 3. Blit MSAA to Default Framebuffer
        // -----------------------------------------------------------------
        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampledFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Blit directly to screen
        glBlitFramebuffer(0, 0, windowX, windowY, 0, 0, windowX, windowY, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Re-bind default framebuffer for UI rendering (which happens in setDrawFunction)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (freecamActive) [[unlikely]] {
            bboxShader->use();
            (*bboxShader)["viewProjection"] = cachedMatrices.projection * cachedMatrices.view;
            (*bboxShader)["color"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

            const glm::vec3 playerCenter = savedPlayerPosition - glm::vec3(0.0f, 0.81f, 0.0f);
            const glm::vec3 playerExtents(0.3f, 0.9f, 0.3f);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            Planet::planet->drawBoundingBox(playerCenter, playerExtents);
        }

        SEED = convertToASCIIsum(seedBox.getText());
    });

    graphics::setResizeFunction([this](const int width, const int height) {
        windowX = width;
        windowY = height;

        cachedWindowCenter.x = width * 0.5f;
        cachedWindowCenter.y = height * 0.5f;

        renderDistanceSlider.setDimensions(cachedWindowCenter.x - 100, cachedWindowCenter.y + 130.0f);
        fpsSlider.setDimensions(cachedWindowCenter.x - 100, cachedWindowCenter.y + 70.0f);
        fullscreenCheckBox.setDimensions(cachedWindowCenter.x, cachedWindowCenter.y);
        seedBox.setDimensions(cachedWindowCenter.x, cachedWindowCenter.y + 160.0f);

        cachedMatrices.dirty = true;

        // Resize MSAA and Intermediate FBOs
        if (multisampledFBO)
            glDeleteFramebuffers(1, &multisampledFBO);
        if (rbo)
            glDeleteRenderbuffers(1, &rbo);
        if (intermediateFBO)
            glDeleteFramebuffers(1, &intermediateFBO);
        if (screenTexture) glDeleteTextures(1, &screenTexture);

        initializeMSAA();
    });

    graphics::setDrawFunction([this] { renderUI(); });
    graphics::setUpdateFunction([this](const float dt) {
        keyboardCallBack(dt);
        mouseCallBack();
    });

    graphics::playMusic("../assets/sounds/songs/Minecraft Volume Alpha.ogg",
                        GameConstants::MUSIC_VOLUME, true);
    graphics::startMessageLoop();
}

void GameObject::initializeGraphics() {
    graphics::createWindow(windowX, windowY, "");
    graphics::setFont("../assets/fonts/Arial.ttf");

    camera = Camera(glm::vec3(0.0f, MAX_HEIGHT / 2 + 10, 0.0f));
    TextureManager::getInstance().loadTextures("../assets/sprites/blocks");
    Blocks::init();
    BiomeRegistry::getInstance().init();
    sky.init();
}

void GameObject::initializeShaders() {
    constexpr int slotBound = 0;

    worldShader = new Shader("../assets/shaders/world_vertex_shader.glsl",
                             "../assets/shaders/world_fragment_shader.glsl");
    (*worldShader)["SMART_tex"] = slotBound;

    billboardShader = new Shader("../assets/shaders/billboard_vertex_shader.glsl",
                                 "../assets/shaders/billboard_fragment_shader.glsl");
    (*billboardShader)["SMART_tex"] = slotBound;

    fluidShader = new Shader("../assets/shaders/fluids_vertex_shader.glsl",
                             "../assets/shaders/fluids_fragment_shader.glsl");
    (*fluidShader)["SMART_tex"] = slotBound;

    outlineShader = new Shader("../assets/shaders/block_outline_vertex_shader.glsl",
                               "../assets/shaders/block_outline_fragment_shader.glsl");

    bboxShader = new Shader("../assets/shaders/bbox_vertex.glsl",
                            "../assets/shaders/bbox_fragment.glsl");

    // Initialize Shadow Shader
    shadowShader = new Shader("../assets/shaders/shadow_mapping_depth.vert",
                              "../assets/shaders/shadow_mapping_depth.frag");

    initializeMSAA();

    if (!worldShader->program || !billboardShader->program || !fluidShader->program ||
        !outlineShader->program || !bboxShader->program || !shadowShader->program) [[unlikely]] {
        std::cerr << "[CRITICAL] Shader compilation failed!" << std::endl;
        std::cerr << "World: " << worldShader->program << std::endl;
        std::cerr << "Billboard: " << billboardShader->program << std::endl;
        std::cerr << "Fluid: " << fluidShader->program << std::endl;
        std::cerr << "Outline: " << outlineShader->program << std::endl;
        std::cerr << "BBox: " << bboxShader->program << std::endl;
        std::cerr << "Shadow: " << shadowShader->program << std::endl;
    }

    // Set Shadow Map Uniform
    worldShader->use();
    (*worldShader)["tex"] = 0; // Explicitly set tex to slot 0
    (*worldShader)["shadowMap"] = 1; // Slot 1
    (*worldShader)["clipPlane"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Default: No clipping (+1.0 distance)

    billboardShader->use();
    (*billboardShader)["tex"] = 0;
    (*billboardShader)["shadowMap"] = 1;
    (*billboardShader)["clipPlane"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    fluidShader->use();
    (*fluidShader)["tex"] = 0;
    (*fluidShader)["shadowMap"] = 1;
    (*fluidShader)["reflectionMap"] = 2; // Slot 2
}

void GameObject::initializeUIElements() {
    cachedWindowCenter.x = windowX * 0.5f;
    cachedWindowCenter.y = windowY * 0.5f;

    fullscreenCheckBox = Checkbox(cachedWindowCenter.x, cachedWindowCenter.y, 50);
    fpsSlider = Slider(cachedWindowCenter.x - 100, cachedWindowCenter.y + 50.0f, 200.0f, 20.0f, 55.0f, 361.0f, 144.0f);
    renderDistanceSlider = Slider(cachedWindowCenter.x - 100, cachedWindowCenter.y + 130.0f, 200.0f, 20.0f, 4.0f, 30.f,
                                  30.f);
    seedBox = TypeBox(cachedWindowCenter.x - 100, cachedWindowCenter.y + 230.0f, 200.0f, 20.0f);
}

void GameObject::initializeOutlineVAO() {
    glGenVertexArrays(1, &outlineVAO);
    glGenBuffers(1, &outlineVBO);
    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GameConstants::OUTLINE_VERTICES),
                 GameConstants::OUTLINE_VERTICES, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void GameObject::initializeMSAA() {
    // Configure MSAA framebuffer
    glGenFramebuffers(1, &multisampledFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, multisampledFBO);

    // Create a multisampled color attachment texture
    unsigned int textureColorBufferMultiSampled;
    glGenTextures(1, &textureColorBufferMultiSampled);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled);
    // 4 samples
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, windowX, windowY, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           textureColorBufferMultiSampled, 0);

    // Create a multisampled renderbuffer object for depth and stencil attachments
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, windowX, windowY);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Multisampled Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Configure intermediate framebuffer
    glGenFramebuffers(1, &intermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

    // Create a color attachment texture
    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowX, windowY, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Intermediate Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ============================================================================
// UPDATE METHODS
// ============================================================================
void GameObject::updateWindowTitle() {
    // OPTIMIZATION: Only update if values changed significantly
    static int lastFPS = -1;
    static int lastChunkCount = -1;

    const int currentFPS = static_cast<int>(graphics::getFPS());
    const int currentChunks = Planet::planet->numChunks;

    if (currentFPS != lastFPS || currentChunks != lastChunkCount) [[unlikely]] {
        // Use static buffer to avoid allocations
        static char titleBuffer[128];
        snprintf(titleBuffer, sizeof(titleBuffer),
                 "Fake Minecraft / FPS: %d Total Chunks: %d",
                 currentFPS, currentChunks);
        graphics::setWindowName(titleBuffer);

        lastFPS = currentFPS;
        lastChunkCount = currentChunks;
    }
}

void GameObject::updateFPSSettings() const {
    static int lastFpsCap = -1;

    if (fpsCap != lastFpsCap) [[unlikely]] {
        if (fpsCap == GameConstants::FPS_VSYNC_THRESHOLD) {
            graphics::setVSYNC(true);
            graphics::setTargetFPS(-1);
        } else if (fpsCap <= GameConstants::FPS_MAX_CAPPED) {
            graphics::setVSYNC(false);
            graphics::setTargetFPS(fpsCap);
        } else {
            graphics::setVSYNC(false);
            graphics::setTargetFPS(-1);
        }
        lastFpsCap = fpsCap;
    }
}

void GameObject::updateShaders() {
    if (cachedMatrices.dirty) [[unlikely]] {
        cachedMatrices.view = camera.getViewMatrix();
        cachedMatrices.projection = glm::perspective(
            glm::radians(90.0f),
            windowX / windowY,
            10000.0f,
            0.1f
        );
        cachedMatrices.dirty = false;
    }

    const glm::mat4 &view = cachedMatrices.view;
    const glm::mat4 &projection = cachedMatrices.projection;
    const glm::mat4 currentViewProjection = projection * view;
    const glm::mat4 &frustumVP = freecamActive ? savedViewProjection : currentViewProjection;

    Planet::planet->updateFrustum(frustumVP, currentViewProjection);

    // Batch sky calculations
    const glm::vec3 sunDir = sky.getSunDirection();
    const glm::vec3 sunCol = sky.getSunColor();
    const float ambient = sky.getAmbientStrength();

    // Clear GL errors
    while (glGetError() != GL_NO_ERROR);

    // Batch shader uniform updates
    worldShader->use(true);
    (*worldShader)["view"] = view;
    (*worldShader)["projection"] = projection;
    (*worldShader)["sunDirection"] = sunDir;
    (*worldShader)["sunColor"] = sunCol;
    (*worldShader)["ambientStrength"] = ambient;
    (*worldShader)["lightSpaceMatrix"] = Planet::planet->lightSpaceMatrix;

    billboardShader->use(true);
    (*billboardShader)["view"] = view;
    (*billboardShader)["projection"] = projection;
    (*billboardShader)["sunDirection"] = sunDir;
    (*billboardShader)["sunColor"] = sunCol;
    (*billboardShader)["ambientStrength"] = ambient;
    (*billboardShader)["lightSpaceMatrix"] = Planet::planet->lightSpaceMatrix;

    fluidShader->use(true);
    (*fluidShader)["view"] = view;
    (*fluidShader)["projection"] = projection;
    (*fluidShader)["time"] = SDL_GetTicks() * 0.001f;
    (*fluidShader)["sunDirection"] = sunDir;
    (*fluidShader)["sunColor"] = sunCol;
    (*fluidShader)["ambientStrength"] = ambient;
    (*fluidShader)["lightSpaceMatrix"] = Planet::planet->lightSpaceMatrix;
    (*fluidShader)["reflectionMatrix"] = Planet::planet->reflectionViewProjection;
    (*fluidShader)["cameraPos"] = camera.Position;

    outlineShader->use(true);
    (*outlineShader)["SMART_view"] = view;
    (*outlineShader)["SMART_projection"] = projection;
}

void GameObject::setupRenderingState() const {
    const glm::vec3 skyCol = sky.getSkyColor();
    glClearColor(skyCol.r, skyCol.g, skyCol.b, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_GEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
}

// ============================================================================
// RENDERING
// ============================================================================
void GameObject::renderBlockOutline(const unsigned int vao) {
    outlineShader->use(true);
    const auto result = Physics::raycast(camera.Position, camera.Front,
                                         GameConstants::RAYCAST_DISTANCE);
    if (result.hit) [[likely]] {
        (*outlineShader)["model"] = glm::vec4(result.blockX, result.blockY, result.blockZ, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLogicOp(GL_INVERT);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(vao);
        glLineWidth(2.0);
        glDrawArrays(GL_LINES, 0, 24);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }
}

void GameObject::renderUI() const {
    setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);
    graphics::setCanvasSize(windowX, windowY);

    renderFluidOverlay();

    if (gameState.state == PLAYING) [[likely]] {
        renderCrosshair();
        renderDebugInfo();
    } else {
        renderPauseMenu();
    }
}

void GameObject::renderFluidOverlay() const {
    // Use helper for coordinate conversion
    ChunkLocalCoords coords;
    coords.update(camera.Position);

    const Chunk *chunk = Planet::planet->getChunk(ChunkPos(coords.chunkX, coords.chunkY, coords.chunkZ));
    if (!chunk) [[unlikely]] return;

    const uint32_t blockType = chunk->getBlockAtPos(coords.localX, coords.localY, coords.localZ);
    const char *blockName = BlockRegistry::getInstance().getBlock(blockType).blockName;
    const uint32_t blockHash = hashCString(blockName);

    graphics::Brush overlay;
    overlay.outline_opacity = 0.0f;

    if (blockHash == HASH_WATER) [[unlikely]] {
        overlay.fill_color[0] = 0.0f;
        overlay.fill_color[1] = 0.0f;
        overlay.fill_color[2] = 0.45f;
        overlay.fill_opacity = 0.6f;
        drawRect(cachedWindowCenter.x, cachedWindowCenter.y, windowX, windowY, overlay);
    } else if (blockHash == HASH_LAVA) [[unlikely]] {
        overlay.fill_color[0] = 1.0f;
        overlay.fill_color[1] = 0.5f;
        overlay.fill_color[2] = 0.0f;
        overlay.fill_opacity = 0.4f;
        drawRect(cachedWindowCenter.x, cachedWindowCenter.y, windowX, windowY, overlay);
    }
}

void GameObject::renderCrosshair() const {
    static const graphics::Brush crosshair = []() {
        graphics::Brush b;
        b.fill_color[0] = 0.7f;
        b.fill_color[1] = 0.7f;
        b.fill_color[2] = 0.7f;
        b.fill_opacity = 1.0f;
        b.outline_opacity = 0.0f;
        return b;
    }();

    drawRect(cachedWindowCenter.x, cachedWindowCenter.y, GameConstants::CROSSHAIR_SIZE,
             GameConstants::CROSSHAIR_THICKNESS, crosshair);
    drawRect(cachedWindowCenter.x, cachedWindowCenter.y, GameConstants::CROSSHAIR_THICKNESS,
             GameConstants::CROSSHAIR_SIZE, crosshair);
}

void GameObject::renderDebugInfo() const {
    static const graphics::Brush textBrush = []() {
        graphics::Brush b;
        b.fill_color[0] = 0.0f;
        b.fill_color[1] = 0.0f;
        b.fill_color[2] = 0.0f;
        return b;
    }();

    // OPTIMIZATION: Use static buffer for text
    static char coordsBuffer[64];
    snprintf(coordsBuffer, sizeof(coordsBuffer), "COORDS: %d, %d, %d",
             static_cast<int>(camera.Position.x),
             static_cast<int>(camera.Position.y),
             static_cast<int>(camera.Position.z));

    static char fpsBuffer[32];
    snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %d",
             static_cast<int>(graphics::getFPS()));

    drawText(10, 30, 15, fpsBuffer, textBrush);
    drawText(10, 50, 15, coordsBuffer, textBrush);

    // Cache block registry reference
    static const BlockRegistry &registry = BlockRegistry::getInstance();
    const char *blockName = registry.getBlock(gameState.selectedBlock).blockName;

    static char blockBuffer[128];
    snprintf(blockBuffer, sizeof(blockBuffer), "SELECTED BLOCK: %s", blockName);
    drawText(10, 70, 15, blockBuffer, textBrush);

    if (freecamActive) [[unlikely]] {
        static const graphics::Brush freecamBrush = []() {
            graphics::Brush b;
            b.fill_color[0] = 1.0f;
            b.fill_color[1] = 0.5f;
            b.fill_color[2] = 0.0f;
            return b;
        }();
        drawText(10, 90, 15, "FREECAM ON (F1 to exit)", freecamBrush);
    }
}

void GameObject::renderPauseMenu() const {
    static const graphics::Brush overlay = []() {
        graphics::Brush b;
        b.outline_opacity = 0.0f;
        b.fill_color[0] = 0.2f;
        b.fill_color[1] = 0.2f;
        b.fill_color[2] = 0.2f;
        b.fill_opacity = 0.9f;
        return b;
    }();

    drawRect(cachedWindowCenter.x, cachedWindowCenter.y, windowX, windowY, overlay);

    renderDistanceSlider.draw();
    fpsSlider.draw();
    fullscreenCheckBox.draw();
    seedBox.draw();

    static const graphics::Brush textBrush = []() {
        graphics::Brush b;
        b.fill_color[0] = 0.9f;
        b.fill_color[1] = 0.9f;
        b.fill_color[2] = 0.9f;
        return b;
    }();

    //Pre-format FPS text
    static char fpsBuffer[32];
    if (fpsCap == GameConstants::FPS_VSYNC_THRESHOLD) {
        strcpy_s(fpsBuffer, "FPS: VSYNC");
    } else if (fpsCap <= GameConstants::FPS_MAX_CAPPED) {
        snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %d", fpsCap);
    } else {
        strcpy_s(fpsBuffer, "FPS: UNCAPPED");
    }

    static char renderDistBuffer[64];
    snprintf(renderDistBuffer, sizeof(renderDistBuffer),
             "Render Distance: %d", Planet::planet->renderDistance);

    drawText(cachedWindowCenter.x - 40, cachedWindowCenter.y + 50.0f, 20, fpsBuffer, textBrush);
    drawText(cachedWindowCenter.x - 80, cachedWindowCenter.y + 110.0f, 20, renderDistBuffer, textBrush);
    drawText(cachedWindowCenter.x - 40, cachedWindowCenter.y - 40, 20, "FullScreen", textBrush);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void GameObject::mouseCallBack() {
    graphics::MouseState ms{};
    getMouseState(ms);

    if (gameState.state == PLAYING) [[likely]] {
        handlePlayingMouseInput(ms);
        cachedMatrices.dirty = true;
    } else {
        handlePausedMouseInput(ms);
    }
}

void GameObject::handlePlayingMouseInput(const graphics::MouseState &ms) {
    const auto xOffset = static_cast<float>(ms.rel_x);
    const auto yOffset = static_cast<float>(-ms.rel_y);
    camera.processMouseMovement(xOffset, yOffset, true);

    if (ms.button_left_pressed || ms.button_middle_pressed || ms.button_right_pressed) [[unlikely]] {
        const auto result = Physics::raycast(camera.Position, camera.Front,
                                             GameConstants::RAYCAST_DISTANCE);
        if (!result.hit) [[unlikely]] return;

        if (ms.button_left_pressed) {
            handleBlockBreak(result);
        } else if (ms.button_middle_pressed) {
            handleBlockPick(result);
        } else {
            handleBlockPlace(result);
        }
    }
}

void GameObject::handlePausedMouseInput(const graphics::MouseState &ms) {
    if (ms.button_left_pressed) [[unlikely]] {
        fullscreenCheckBox.handleClick(ms.cur_pos_x, ms.cur_pos_y,
                                       [] { graphics::setFullScreen(true); },
                                       [] { graphics::setFullScreen(false); });
        return;
    }

    if (ms.button_left_down) {
        fpsSlider.startDragging(ms.cur_pos_x, ms.cur_pos_y);
        renderDistanceSlider.startDragging(ms.cur_pos_x, ms.cur_pos_y);
    } else {
        fpsSlider.stopDragging();
        renderDistanceSlider.stopDragging();
    }

    renderDistanceSlider.update(ms.cur_pos_x, Planet::planet->renderDistance);
    fpsSlider.update(ms.cur_pos_x, fpsCap);
}

void GameObject::keyboardCallBack(const float deltaTime) {
    // Toggle pause
    if (getKeyState(graphics::SCANCODE_ESCAPE)) [[unlikely]] {
        gameState.state = SDL_GetRelativeMouseMode() ? PLAYING : PAUSED;
    }

    if (gameState.state == PAUSED) [[unlikely]] {
        seedBox.handleInput();
        return;
    }

    // Pre-calculate movement speed
    const float movementSpeed = deltaTime * GameConstants::MOVEMENT_SPEED_MULTIPLIER;

    // Unrolled movement checks
    bool moved = false;
    if (getKeyState(graphics::SCANCODE_W)) {
        camera.processKeyboard(FORWARD, movementSpeed);
        moved = true;
    }
    if (getKeyState(graphics::SCANCODE_S)) {
        camera.processKeyboard(BACKWARD, movementSpeed);
        moved = true;
    }
    if (getKeyState(graphics::SCANCODE_A)) {
        camera.processKeyboard(LEFT, movementSpeed);
        moved = true;
    }
    if (getKeyState(graphics::SCANCODE_D)) {
        camera.processKeyboard(RIGHT, movementSpeed);
        moved = true;
    }
    if (getKeyState(graphics::SCANCODE_SPACE)) {
        camera.processKeyboard(UP, movementSpeed);
        moved = true;
    }
    if (getKeyState(graphics::SCANCODE_LSHIFT)) {
        camera.processKeyboard(DOWN, movementSpeed);
        moved = true;
    }

    if (moved) [[likely]] {
        cachedMatrices.dirty = true;
    }

    // F-key toggles with debouncing
    static bool f1Pressed = false;
    if (getKeyState(graphics::SCANCODE_F1)) {
        if (!f1Pressed) [[unlikely]] {
            f1Pressed = true;
            freecamActive = !freecamActive;

            if (freecamActive) {
                // Save state to allow detached investigation of the world
                savedPlayerPosition = camera.Position;
                savedViewProjection = cachedMatrices.projection * cachedMatrices.view;
            } else {
                // Teleport back to player state
                camera.Position = savedPlayerPosition;
                cachedMatrices.dirty = true;
            }
        }
    } else {
        f1Pressed = false;
    }

    static bool f2Pressed = false;
    if (getKeyState(graphics::SCANCODE_F2)) {
        if (!f2Pressed) [[unlikely]] {
            f2Pressed = true;
            sky.togglePause();
            std::cout << "Sky Time " << (sky.isPaused() ? "PAUSED" : "RESUMED") << std::endl;
        }
    } else {
        f2Pressed = false;
    }

    static bool f11Pressed = false;
    if (getKeyState(graphics::SCANCODE_F11)) {
        if (!f11Pressed) [[unlikely]] {
            f11Pressed = true;
            const bool isChecked = fullscreenCheckBox.isChecked();
            fullscreenCheckBox.setChecked(!isChecked);
            graphics::setFullScreen(!isChecked);
        }
    } else {
        f11Pressed = false;
    }
}

// ============================================================================
// BLOCK OPERATIONS
// ============================================================================
void GameObject::handleBlockBreak(const Physics::RaycastResult &result) {
    const uint32_t blockType = result.chunk->getBlockAtPos(
        result.localBlockX, result.localBlockY, result.localBlockZ);

    // Dependent breaking logic for tall structures
    const Block &currentBlock = BlockRegistry::getInstance().getBlock(blockType);
    const uint32_t blockHash = hashCString(currentBlock.blockName);

    if (blockHash == HASH_TALL_GRASS_BOTTOM) [[unlikely]] {
        // Automatically destroy the top half of tall grass if the base is removed
        const uint32_t aboveBlock = result.chunk->getBlockAtPos(
            result.localBlockX, result.localBlockY + 1, result.localBlockZ);

        const Block &aboveBlockData = BlockRegistry::getInstance().getBlock(aboveBlock);
        const uint32_t aboveHash = hashCString(aboveBlockData.blockName);

        if (aboveHash == HASH_TALL_GRASS_TOP) {
            result.chunk->updateBlock(result.localBlockX, result.localBlockY + 1, result.localBlockZ, 0);
        }
    }

    playSound(blockType);
    result.chunk->updateBlock(result.localBlockX, result.localBlockY, result.localBlockZ, 0);
}

void GameObject::handleBlockPick(const Physics::RaycastResult &result) {
    gameState.selectedBlock = result.chunk->getBlockAtPos(
        result.localBlockX, result.localBlockY, result.localBlockZ);
}

void GameObject::handleBlockPlace(const Physics::RaycastResult &result) const {
    //Batch absolute value calculations
    const float distX = result.hitPos.x - (result.blockX + 0.5f);
    const float distY = result.hitPos.y - (result.blockY + 0.5f);
    const float distZ = result.hitPos.z - (result.blockZ + 0.5f);

    const float absDX = std::abs(distX);
    const float absDY = std::abs(distY);
    const float absDZ = std::abs(distZ);

    int blockX = result.blockX;
    int blockY = result.blockY;
    int blockZ = result.blockZ;

    //Branchless selection
    const bool xMax = (absDX > absDY) & (absDX > absDZ);
    const bool yMax = (absDY > absDX) & (absDY > absDZ);

    blockX += xMax ? (distX > 0 ? 1 : -1) : 0;
    blockY += yMax ? (distY > 0 ? 1 : -1) : 0;
    blockZ += (!xMax & !yMax) ? (distZ > 0 ? 1 : -1) : 0;

    // Get chunk coordinates
    const int chunkX = worldToChunkCoord(blockX, CHUNK_WIDTH);
    const int chunkY = worldToChunkCoord(blockY, CHUNK_HEIGHT);
    const int chunkZ = worldToChunkCoord(blockZ, CHUNK_WIDTH);

    const int localBlockX = blockX - chunkX * CHUNK_WIDTH;
    const int localBlockY = blockY - chunkY * CHUNK_HEIGHT;
    const int localBlockZ = blockZ - chunkZ * CHUNK_WIDTH;

    Chunk *chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
    if (!chunk) [[unlikely]] return;

    const uint16_t blockToReplace = chunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);
    const Block &blockToReplaceData = BlockRegistry::getInstance().getBlock(blockToReplace);

    if (blockToReplace != 0 && blockToReplaceData.blockType != Block::LIQUID) [[unlikely]] {
        return;
    }

    // Billboard validation
    const Block &selectedBlockData = BlockRegistry::getInstance().getBlock(gameState.selectedBlock);
    if (selectedBlockData.blockType == Block::BILLBOARD) [[unlikely]] {
        const uint16_t blockBelow = chunk->getBlockAtPos(localBlockX, localBlockY - 1, localBlockZ);
        const uint32_t belowHash = hashCString(BlockRegistry::getInstance().getBlock(blockBelow).blockName);
        if (belowHash != HASH_GRASS_BLOCK) {
            return;
        }
    }

    playSound(gameState.selectedBlock);
    chunk->updateBlock(localBlockX, localBlockY, localBlockZ, gameState.selectedBlock);
}

// ============================================================================
// UTILITY
// ============================================================================
void GameObject::playSound(const uint16_t block_id) {
    // OPTIMIZATION: Lookup table for common blocks
    static constexpr struct {
        uint16_t id;
        const char *sound;
        float volume;
    } soundMap[] = {
                {1, "../assets/sounds/DIRT.ogg", 0.4f},
                {2, "../assets/sounds/GRASS_BLOCK.ogg", 0.4f},
                {3, "../assets/sounds/STONE_BLOCK.ogg", 0.4f},
                {4, "../assets/sounds/LOG.ogg", 0.4f},
                {15, "../assets/sounds/SAND.ogg", 1.0f},
                {16, "../assets/sounds/STONE_BLOCK.ogg", 0.4f},
                {17, "../assets/sounds/STONE_BLOCK.ogg", 0.4f},
            };

    // Fast path for grass blocks (5-12)
    if (block_id >= 5 && block_id <= 12) [[likely]] {
        graphics::playSound("../assets/sounds/GRASS.ogg", 0.4f);
        return;
    }

    // Linear search is fast for small tables
    for (const auto &[id, sound, volume]: soundMap) {
        if (id == block_id) {
            graphics::playSound(sound, volume);
            return;
        }
    }
}
