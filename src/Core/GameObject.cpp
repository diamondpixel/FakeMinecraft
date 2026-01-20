#include "GameObject.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <utility>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <SDL2/SDL_mouse.h>

#include "Blocks.h"
#include "SDL2/SDL_timer.h"

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
int convertToASCIIsum(const std::string &input) {
    int sum = 0;
    for (const char c : input) {
        const int charVal = static_cast<unsigned char>(c);
        if (sum > INT_MAX - charVal) return INT_MAX;
        if (sum < INT_MIN + charVal) return INT_MIN;
        sum += charVal;
    }
    return sum;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================
GameObject::GameObject(const float x, const float y, std::string windowName)
    : fpsSlider(), renderDistanceSlider(), seedBox(), windowX(x), windowY(y), lastX(x / 2), lastY(y / 2),
      window_name(std::move(windowName)) {
}

GameObject::~GameObject() {
    // Cleanup OpenGL resources
    if (outlineVAO)
        glDeleteVertexArrays(1, &outlineVAO);
    if (outlineVBO)
        glDeleteBuffers(1, &outlineVBO);

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

    Planet::planet = new Planet(&worldShader, &fluidShader, &billboardShader, &bboxShader);

    // Setup callbacks with optimized lambdas
    graphics::setPreDrawFunction([this] {
        updateWindowTitle();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, gameTexture->getID());
        setupRenderingState();
        updateFPSSettings();
        updateShaders();

        // Use saved player position for chunk loading during freecam
        const glm::vec3 &updatePos = freecamActive ? savedPlayerPosition : camera.Position;
        // ENABLE occlusion only if NOT in freecam.
        // In freecam, we want to see all chunks in the player's frustum (no occlusion culling).
        Planet::planet->update(updatePos, !freecamActive);
        renderBlockOutline(outlineVAO);
        
        // Render Player Visualization in Freecam (Black Box)
        if (freecamActive) {
            bboxShader.use();
            // IMPORTANT: Use current camera matrices for visualization, NOT the frozen frustum matrices
            bboxShader["viewProjection"] = cachedMatrices.projection * cachedMatrices.view;
            
            // Black color for the box
            bboxShader["color"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            
            // Standard player size approx 0.6x1.8x0.6. Box uses center/extents.
            // Center is position - eye level (approx 1.62m). 
            glm::vec3 playerCenter = savedPlayerPosition - glm::vec3(0.0f, 0.81f, 0.0f); 
            glm::vec3 playerExtents(0.3f, 0.9f, 0.3f); // Half-widths/heights
            
            // Draw filled box
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            Planet::planet->drawBoundingBox(playerCenter, playerExtents);
        }

        SEED = convertToASCIIsum(seedBox.getText());
    });

    graphics::setResizeFunction([this](const int width, const int height) {
        // Cache-friendly batch update
        windowX = width;
        windowY = height;

        const float halfX = windowX * 0.5f;
        const float halfY = windowY * 0.5f;

        renderDistanceSlider.setDimensions(halfX - 100, halfY + 130.0f);
        fpsSlider.setDimensions(halfX - 100, halfY + 70.0f);
        fullscreenCheckBox.setDimensions(halfX, halfY);
        seedBox.setDimensions(halfX, halfY + 160.0f);

        cachedMatrices.dirty = true; // Mark matrices for recalculation
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

    camera = Camera(glm::vec3(0.0f, MAX_HEIGHT/2 + 10, 0.0f));
    textureManager = &graphics::TextureManager::getInstance();

    const std::string texturePath = "../assets/sprites/block_map.png";
    gameTexture = textureManager->createTexture(
        texturePath, false, [&texturePath](graphics::Texture &tex) {
            stbi_set_flip_vertically_on_load(true);

            int width, height, channels;
            unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &channels, 4);

            if (!data) {
                std::cerr << "Failed to load texture: " << texturePath << std::endl;
                return;
            }

            // Set texture dimensions in the wrapper
            // Note: We're hacking the wrapper to store array dimensions if needed, 
            // but effectively we just need the ID to be valid.
            // tex.setWidth(width); tex.setHeight(height); // Wrapper probably does this?
            // Actually the wrapper calls this lambda. We should probably respect its structure.
            // But we are managing the GL texture ourselves here.
            
            // Assume 16x16 grid of tiles
            const int tilesX = 16;
            const int tilesY = 16;
            const int tileW = width / tilesX;
            const int tileH = height / tilesY;
            const int layerCount = tilesX * tilesY;
            
            // Re-arrange data into layers
            std::vector<unsigned char> arrayBuffer(width * height * 4);
            
            for (int ly = 0; ly < tilesY; ly++) {
                for (int lx = 0; lx < tilesX; lx++) {
                    int layer = ly * tilesX + lx;
                    for (int y = 0; y < tileH; y++) {
                        for (int x = 0; x < tileW; x++) {
                            // Source: Atlas coordinates (flipped vertically? stbi_set_flip... is true)
                            // If flipped, row 0 is bottom. 
                            // Standard Minecraft atlas: Row 0 is top.
                            // If stbi flips, Row 0 becomes bottom.
                            
                            int srcX = lx * tileW + x;
                            int srcY = ly * tileH + y;
                            int srcIdx = (srcY * width + srcX) * 4;
                            
                            // Dest: Layer `layer` (0..255)
                            // Within layer: y * tileW + x
                            int destIdx = (layer * (tileW * tileH) + (y * tileW + x)) * 4;
                            
                            arrayBuffer[destIdx + 0] = data[srcIdx + 0];
                            arrayBuffer[destIdx + 1] = data[srcIdx + 1];
                            arrayBuffer[destIdx + 2] = data[srcIdx + 2];
                            arrayBuffer[destIdx + 3] = data[srcIdx + 3];
                        }
                    }
                }
            }

            stbi_image_free(data);

            // Setup OpenGL Array Texture
            glGenTextures(1, tex.getIDPointer());
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex.getID());
            
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
            
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, tileW, tileH, layerCount,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, arrayBuffer.data());
                         
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        });
}

void GameObject::initializeShaders() {
    // We bind the texture to slot 0 in the render loop, so we set the shader uniform to 0.
    const int slotBound = 0;
    constexpr float texMultiplier = 0.0625f; // 1/16 for texture atlas

    // World shader
    worldShader = Shader("../assets/shaders/world_vertex_shader.glsl",
                         "../assets/shaders/world_fragment_shader.glsl");
    worldShader["SMART_texMultiplier"] = texMultiplier;
    worldShader["SMART_tex"] = slotBound;

    // Billboard shader
    billboardShader = Shader("../assets/shaders/billboard_vertex_shader.glsl",
                             "../assets/shaders/billboard_fragment_shader.glsl");
    billboardShader["SMART_texMultiplier"] = texMultiplier;
    billboardShader["SMART_tex"] = slotBound;

    // Fluid shader
    fluidShader = Shader("../assets/shaders/fluids_vertex_shader.glsl",
                         "../assets/shaders/fluids_fragment_shader.glsl");
    fluidShader["SMART_texMultiplier"] = texMultiplier;
    fluidShader["SMART_tex"] = slotBound;

    // Outline shader
    outlineShader = Shader("../assets/shaders/block_outline_vertex_shader.glsl",
                           "../assets/shaders/block_outline_fragment_shader.glsl");

    // BBox shader (Occlusion Queries)
    bboxShader = Shader("../assets/shaders/bbox_vertex.glsl", "../assets/shaders/bbox_fragment.glsl");
}

void GameObject::initializeUIElements() {
    const float halfX = windowX * 0.5f;
    const float halfY = windowY * 0.5f;

    fullscreenCheckBox = Checkbox(halfX, halfY, 50);
    fpsSlider = Slider(halfX - 100, halfY + 50.0f, 200.0f, 20.0f, 55.0f, 361.0f, 144.0f);
    renderDistanceSlider = Slider(halfX - 100, halfY + 130.0f, 200.0f, 20.0f, 4.0f, 30.f, 30.f);
    seedBox = TypeBox(halfX - 100, halfY + 230.0f, 200.0f, 20.0f);
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

// ============================================================================
// UPDATE METHODS
// ============================================================================
void GameObject::updateWindowTitle() {
    // Avoid string construction if FPS hasn't changed significantly
    static int lastFPS = -1;
    static int lastChunkCount = -1;

    const int currentFPS = static_cast<int>(graphics::getFPS());
    const int currentChunks = Planet::planet->numChunks;

    if (currentFPS != lastFPS || currentChunks != lastChunkCount) {
        std::ostringstream oss;
        oss << "Fake Minecraft / FPS: " << currentFPS
                << " Total Chunks: " << currentChunks;
        graphics::setWindowName(oss.str().c_str());

        lastFPS = currentFPS;
        lastChunkCount = currentChunks;
    }
}

void GameObject::updateFPSSettings() const {
    static int lastFpsCap = -1;

    if (fpsCap != lastFpsCap) {
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
    // Only recalculate matrices if needed
    if (cachedMatrices.dirty) {
        cachedMatrices.view = camera.getViewMatrix();
        cachedMatrices.projection = glm::perspective(
            glm::radians(90.0f),
            windowX / windowY,
            0.1f,
            10000.0f
        );
        cachedMatrices.dirty = false;
    }

    const glm::mat4 &view = cachedMatrices.view;
    const glm::mat4 &projection = cachedMatrices.projection;
    
    const glm::mat4 currentViewProjection = projection * view;

    // Use frozen frustum during freecam, otherwise use current camera
    const glm::mat4 &frustumVP = freecamActive ? savedViewProjection : currentViewProjection;
    // But ALWAYS use current camera for rendering/queries (matches depth buffer)
    Planet::planet->updateFrustum(frustumVP, currentViewProjection);

    // Batch shader updates
    worldShader.use(true);
    worldShader["SMART_view"] = view;
    worldShader["SMART_projection"] = projection;

    billboardShader.use(true);
    billboardShader["SMART_view"] = view;
    billboardShader["SMART_projection"] = projection;

    fluidShader.use(true);
    fluidShader["SMART_view"] = view;
    fluidShader["SMART_projection"] = projection;
    fluidShader["SMART_time"] = SDL_GetTicks() / 1000.0f;

    outlineShader.use(true);
    outlineShader["SMART_view"] = view;
    outlineShader["SMART_projection"] = projection;
}

void GameObject::setupRenderingState() {
    glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
    outlineShader.use(true);
    const auto result = Physics::raycast(camera.Position, camera.Front,
                                   GameConstants::RAYCAST_DISTANCE);
    if (result.hit) {
        outlineShader["model"] = glm::vec4(result.blockX, result.blockY, result.blockZ, 1);
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

    switch (gameState.state) {
        case PLAYING:
            renderCrosshair();
            renderDebugInfo();
            break;
        case PAUSED:
            renderPauseMenu();
            break;
    }
}

void GameObject::renderFluidOverlay() const {
    // Quick block lookup optimization
    const glm::vec3 &pos = camera.Position;
    const int blockX = pos.x < 0 ? pos.x - 1 : pos.x;
    const int blockY = pos.y < 0 ? pos.y - 1 : pos.y;
    const int blockZ = pos.z < 0 ? pos.z - 1 : pos.z;

    const int chunkX = blockX < 0 ? floorf(blockX / static_cast<float>(CHUNK_WIDTH)) : blockX / CHUNK_WIDTH;
    const int chunkY = blockY < 0 ? floorf(blockY / static_cast<float>(CHUNK_HEIGHT)) : blockY / CHUNK_HEIGHT;
    const int chunkZ = blockZ < 0 ? floorf(blockZ / static_cast<float>(CHUNK_WIDTH)) : blockZ / CHUNK_WIDTH;

    const int localBlockX = blockX - (chunkX * CHUNK_WIDTH);
    const int localBlockY = blockY - (chunkY * CHUNK_HEIGHT);
    const int localBlockZ = blockZ - (chunkZ * CHUNK_WIDTH);

    Chunk *chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
    if (!chunk) return;

    const uint32_t blockType = chunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);
    const std::string &blockName = Blocks::blocks[blockType].blockName;

    graphics::Brush overlay;
    overlay.outline_opacity = 0.0f;

    if (blockName == "WATER") {
        overlay.fill_color[0] = 0.0f;
        overlay.fill_color[1] = 0.0f;
        overlay.fill_color[2] = 0.45f;
        overlay.fill_opacity = 0.6f;
        drawRect(windowX * 0.5f, windowY * 0.5f, windowX, windowY, overlay);
    } else if (blockName == "LAVA") {
        overlay.fill_color[0] = 1.0f;
        overlay.fill_color[1] = 0.5f;
        overlay.fill_color[2] = 0.0f;
        overlay.fill_opacity = 0.4f;
        drawRect(windowX * 0.5f, windowY * 0.5f, windowX, windowY, overlay);
    }
}

void GameObject::renderCrosshair() const {
    graphics::Brush crosshair;
    crosshair.fill_color[0] = 0.7f;
    crosshair.fill_color[1] = 0.7f;
    crosshair.fill_color[2] = 0.7f;
    crosshair.fill_opacity = 1.0f;
    crosshair.outline_opacity = 0.0f;

    const float centerX = windowX * 0.5f;
    const float centerY = windowY * 0.5f;

    drawRect(centerX, centerY, GameConstants::CROSSHAIR_SIZE,
             GameConstants::CROSSHAIR_THICKNESS, crosshair);
    drawRect(centerX, centerY, GameConstants::CROSSHAIR_THICKNESS,
             GameConstants::CROSSHAIR_SIZE, crosshair);
}

void GameObject::renderDebugInfo() const {
    graphics::Brush textBrush;
    textBrush.fill_color[0] = 0.0f;
    textBrush.fill_color[1] = 0.0f;
    textBrush.fill_color[2] = 0.0f;

    std::ostringstream coordsText;
    coordsText << "COORDS: "
            << static_cast<int>(camera.Position.x) << ", "
            << static_cast<int>(camera.Position.y) << ", "
            << static_cast<int>(camera.Position.z);

    drawText(10, 30, 15, "FPS: " + std::to_string(static_cast<int>(graphics::getFPS())), textBrush);
    drawText(10, 50, 15, coordsText.str(), textBrush);
    drawText(10, 70, 15, "SELECTED BLOCK: " + Blocks::blocks[gameState.selectedBlock].blockName, textBrush);
    
    if (freecamActive) {
        textBrush.fill_color[0] = 1.0f;
        textBrush.fill_color[1] = 0.5f;
        textBrush.fill_color[2] = 0.0f;
        drawText(10, 90, 15, "FREECAM  ON(F1 to exit)", textBrush);
    }
}

void GameObject::renderPauseMenu() const {
    // Dark overlay
    graphics::Brush overlay;
    overlay.outline_opacity = 0.0f;
    overlay.fill_color[0] = 0.2f;
    overlay.fill_color[1] = 0.2f;
    overlay.fill_color[2] = 0.2f;
    overlay.fill_opacity = 0.9f;
    drawRect(windowX * 0.5f, windowY * 0.5f, windowX, windowY, overlay);

    // UI elements
    renderDistanceSlider.draw();
    fpsSlider.draw();
    fullscreenCheckBox.draw();
    seedBox.draw();

    // Text
    graphics::Brush textBrush;
    textBrush.fill_color[0] = 0.9f;
    textBrush.fill_color[1] = 0.9f;
    textBrush.fill_color[2] = 0.9f;

    const float centerX = windowX * 0.5f;
    const float centerY = windowY * 0.5f;

    // FPS display
    std::string fpsText;
    if (fpsCap == GameConstants::FPS_VSYNC_THRESHOLD) {
        fpsText = "FPS: VSYNC";
    } else if (fpsCap <= GameConstants::FPS_MAX_CAPPED) {
        fpsText = "FPS: " + std::to_string(fpsCap);
    } else {
        fpsText = "FPS: UNCAPPED";
    }
    drawText(centerX - 40, centerY + 50.0f, 20, fpsText, textBrush);

    drawText(centerX - 80, centerY + 110.0f, 20,
             "Render Distance: " + std::to_string(Planet::planet->renderDistance), textBrush);
    drawText(centerX - 40, centerY - 40, 20, "FullScreen", textBrush);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void GameObject::mouseCallBack() {
    graphics::MouseState ms{};
    getMouseState(ms);

    switch (gameState.state) {
        case PLAYING:
            handlePlayingMouseInput(ms);
            cachedMatrices.dirty = true; // Camera moved
            break;
        case PAUSED:
            handlePausedMouseInput(ms);
            break;
    }
}

void GameObject::handlePlayingMouseInput(graphics::MouseState &ms) {
    // Mouse look
    const auto xOffset = static_cast<float>(ms.rel_x);
    const auto yOffset = static_cast<float>(-ms.rel_y);
    camera.processMouseMovement(xOffset, yOffset, true);

    // Block interaction
    if (ms.button_left_pressed || ms.button_middle_pressed || ms.button_right_pressed) {
        const auto result = Physics::raycast(camera.Position, camera.Front,
                                             GameConstants::RAYCAST_DISTANCE);
        if (!result.hit) return;

        if (ms.button_left_pressed) {
            handleBlockBreak(result);
        } else if (ms.button_middle_pressed) {
            handleBlockPick(result);
        } else {
            handleBlockPlace(result);
        }
    }
}

void GameObject::handlePausedMouseInput(graphics::MouseState &ms) {
    if (ms.button_left_pressed) {
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

void GameObject::keyboardCallBack(float deltaTime) {
    static bool f11Pressed = false;

    // Toggle pause
    if (getKeyState(graphics::SCANCODE_ESCAPE)) {
        gameState.state = SDL_GetRelativeMouseMode() ? PLAYING : PAUSED;
    }

    if (gameState.state == PAUSED) {
        seedBox.handleInput();
        return;
    }

    // Movement
    const float movementSpeed = deltaTime * GameConstants::MOVEMENT_SPEED_MULTIPLIER;

    static constexpr std::array<std::pair<graphics::scancode_t, Camera_Movement>, 6> keyBindings = {
        {
            {graphics::SCANCODE_W, FORWARD},
            {graphics::SCANCODE_S, BACKWARD},
            {graphics::SCANCODE_A, LEFT},
            {graphics::SCANCODE_D, RIGHT},
            {graphics::SCANCODE_SPACE, UP},
            {graphics::SCANCODE_LSHIFT, DOWN}
        }
    };

    for (const auto &[key, direction]: keyBindings) {
        if (getKeyState(key)) {
            camera.processKeyboard(direction, movementSpeed);
            cachedMatrices.dirty = true;
        }
    }

    // F1 freecam toggle
    static bool f1Pressed = false;
    if (getKeyState(graphics::SCANCODE_F1)) {
        if (!f1Pressed) {
            f1Pressed = true;
            freecamActive = !freecamActive;
            
            if (freecamActive) {
                // Entering freecam: Save current position and view frustum
                savedPlayerPosition = camera.Position;
                savedViewProjection = cachedMatrices.projection * cachedMatrices.view;
            } else {
                // Exiting freecam: Teleport back to saved position
                camera.Position = savedPlayerPosition;
                cachedMatrices.dirty = true;
            }
        }
    } else {
        f1Pressed = false;
    }

    // F11 fullscreen toggle
    if (getKeyState(graphics::SCANCODE_F11)) {
        if (!f11Pressed) {
            f11Pressed = true;
            bool isChecked = fullscreenCheckBox.isChecked();
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
    playSound(blockType);
    result.chunk->updateBlock(result.localBlockX, result.localBlockY, result.localBlockZ, 0);
}

void GameObject::handleBlockPick(const Physics::RaycastResult &result) {
    gameState.selectedBlock = result.chunk->getBlockAtPos(
        result.localBlockX, result.localBlockY, result.localBlockZ);
}

void GameObject::handleBlockPlace(const Physics::RaycastResult &result) {
    // Calculate placement position
    const float distX = result.hitPos.x - (result.blockX + 0.5f);
    const float distY = result.hitPos.y - (result.blockY + 0.5f);
    const float distZ = result.hitPos.z - (result.blockZ + 0.5f);

    int blockX = result.blockX;
    int blockY = result.blockY;
    int blockZ = result.blockZ;

    const float absDX = fabs(distX);
    const float absDY = fabs(distY);
    const float absDZ = fabs(distZ);

    if (absDX > absDY && absDX > absDZ) {
        blockX += (distX > 0 ? 1 : -1);
    } else if (absDY > absDX && absDY > absDZ) {
        blockY += (distY > 0 ? 1 : -1);
    } else {
        blockZ += (distZ > 0 ? 1 : -1);
    }

    // Get chunk coordinates
    const int chunkX = blockX < 0 ? floorf(blockX / static_cast<float>(CHUNK_WIDTH)) : blockX / CHUNK_WIDTH;
    const int chunkY = blockY < 0 ? floorf(blockY / static_cast<float>(CHUNK_HEIGHT)) : blockY / CHUNK_HEIGHT;
    const int chunkZ = blockZ < 0 ? floorf(blockZ / static_cast<float>(CHUNK_WIDTH)) : blockZ / CHUNK_WIDTH;

    const int localBlockX = blockX - (chunkX * CHUNK_WIDTH);
    const int localBlockY = blockY - (chunkY * CHUNK_HEIGHT);
    const int localBlockZ = blockZ - (chunkZ * CHUNK_WIDTH);

    Chunk *chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
    if (!chunk) return;

    const uint16_t blockToReplace = chunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);

    // Check if we can place
    if (blockToReplace != 0 && Blocks::blocks[blockToReplace].blockType != Block::LIQUID) {
        return;
    }

    // Billboard placement validation
    if (Blocks::blocks[gameState.selectedBlock].blockType == Block::BILLBOARD) {
        const uint16_t blockBelow = chunk->getBlockAtPos(localBlockX, localBlockY - 1, localBlockZ);
        if (Blocks::blocks[blockBelow].blockName != "GRASS_BLOCK") {
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
    // Sound lookup table for better performance
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

    // Grass blocks (5-12)
    if (block_id >= 5 && block_id <= 12) {
        graphics::playSound("../assets/sounds/GRASS.ogg", 0.4f);
        return;
    }

    // Lookup in table
    for (const auto &[id, sound, volume]: soundMap) {
        if (id == block_id) {
            graphics::playSound(sound, volume);
            return;
        }
    }
}
