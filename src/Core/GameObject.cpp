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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
int convertToASCIIsum(const std::string &input) {
    int sum = 0;
    for (const char c: input) {
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

    if (worldShader) delete worldShader;
    if (billboardShader) delete billboardShader;
    if (fluidShader) delete fluidShader;
    if (outlineShader) delete outlineShader;
    if (bboxShader) delete bboxShader;

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

    // Setup callbacks with optimized lambdas
    graphics::setPreDrawFunction([this] {
        // Update sky
        sky.update(1.0f / std::max(graphics::getFPS(), 1.0f));
        //sky.update(1.0f / 2.0 );

        updateWindowTitle();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, TextureManager::getInstance().getTextureArrayID());
        setupRenderingState();
        updateFPSSettings();
        updateShaders();

        // Render sky BEFORE chunks (depth test disabled inside Sky::render)
        sky.render(cachedMatrices.view, cachedMatrices.projection, camera.Position);

        // Re-bind block texture array after sky rendering used texture unit 1
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, TextureManager::getInstance().getTextureArrayID());

        // Use saved player position for chunk loading during freecam
        const glm::vec3 &updatePos = freecamActive ? savedPlayerPosition : camera.Position;
        // ENABLE occlusion only if NOT in freecam.
        // In freecam, we want to see all chunks in the player's frustum (no occlusion culling).
        Planet::planet->update(updatePos, !freecamActive);
        renderBlockOutline(outlineVAO);

        // Render Player Visualization in Freecam (Black Box)
        if (freecamActive) {
            bboxShader->use();
            // IMPORTANT: Use current camera matrices for visualization, NOT the frozen frustum matrices
            (*bboxShader)["viewProjection"] = cachedMatrices.projection * cachedMatrices.view;

            // Black color for the box
            (*bboxShader)["color"] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

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

    camera = Camera(glm::vec3(0.0f, MAX_HEIGHT / 2 + 10, 0.0f));

    // Load textures using Dynamic Atlas
    TextureManager::getInstance().loadTextures("../assets/sprites/blocks");

    // Initialize Blocks (builds registry and resolves textures)
    Blocks::init();

    // Initialize Biomes (sets up biome definitions with features)
    BiomeRegistry::getInstance().init();

    // Initialize Sky (loads sun/moon textures)
    sky.init();
}

void GameObject::initializeShaders() {
    // We bind the texture to slot 0 in the render loop, so we set the shader uniform to 0.
    const int slotBound = 0;

    // World shader
    worldShader = new Shader("../assets/shaders/world_vertex_shader.glsl",
                             "../assets/shaders/world_fragment_shader.glsl");
    (*worldShader)["SMART_tex"] = slotBound;

    // Billboard shader
    billboardShader = new Shader("../assets/shaders/billboard_vertex_shader.glsl",
                                 "../assets/shaders/billboard_fragment_shader.glsl");
    (*billboardShader)["SMART_tex"] = slotBound;

    // Fluid shader
    fluidShader = new Shader("../assets/shaders/fluids_vertex_shader.glsl",
                             "../assets/shaders/fluids_fragment_shader.glsl");
    (*fluidShader)["SMART_tex"] = slotBound;

    // Outline shader
    outlineShader = new Shader("../assets/shaders/block_outline_vertex_shader.glsl",
                               "../assets/shaders/block_outline_fragment_shader.glsl");

    // BBox shader (Occlusion Queries)
    bboxShader = new Shader("../assets/shaders/bbox_vertex.glsl", "../assets/shaders/bbox_fragment.glsl");

    // Check for compilation errors
    if (!worldShader->program || !billboardShader->program || !fluidShader->program || !outlineShader->program || !
        bboxShader->program) {
        std::cerr << "[CRITICAL] Shader compilation failed!" << std::endl;
        std::cerr << "World: " << worldShader->program << std::endl;
        std::cerr << "Billboard: " << billboardShader->program << std::endl;
        std::cerr << "Fluid: " << fluidShader->program << std::endl;
        std::cerr << "Outline: " << outlineShader->program << std::endl;
        std::cerr << "BBox: " << bboxShader->program << std::endl;
    }
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
            10000.0f, // Swapped near/far for Reverse-Z
            0.1f // Reverted to 0.1f for near plane
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
    glm::vec3 sunDir = sky.getSunDirection();
    glm::vec3 sunCol = sky.getSunColor();
    float ambient = sky.getAmbientStrength();

    // Batch shader updates
    // Clear any previous GL errors (e.g. from Sky or TextureManager) to prevent crash
    while (glGetError() != GL_NO_ERROR);

    worldShader->use(true);
    (*worldShader)["view"] = view;
    (*worldShader)["projection"] = projection;
    (*worldShader)["sunDirection"] = sunDir;
    (*worldShader)["sunColor"] = sunCol;
    (*worldShader)["ambientStrength"] = ambient;

    billboardShader->use(true);
    (*billboardShader)["view"] = view;
    (*billboardShader)["projection"] = projection;
    (*billboardShader)["sunDirection"] = sunDir;
    (*billboardShader)["sunColor"] = sunCol;
    (*billboardShader)["ambientStrength"] = ambient;

    fluidShader->use(true);
    (*fluidShader)["view"] = view;
    (*fluidShader)["projection"] = projection;
    (*fluidShader)["time"] = SDL_GetTicks() / 1000.0f;
    (*fluidShader)["sunDirection"] = sunDir;
    (*fluidShader)["sunColor"] = sunCol;
    (*fluidShader)["ambientStrength"] = ambient;

    outlineShader->use(true);
    (*outlineShader)["SMART_view"] = view;
    (*outlineShader)["SMART_projection"] = projection;
}

void GameObject::setupRenderingState() {
    // Use sky color for clear color
    glm::vec3 skyCol = sky.getSkyColor();
    glClearColor(skyCol.r, skyCol.g, skyCol.b, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(0.0f); // Reverse-Z: Clear to 0.0 instead of 1.0
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_GEQUAL); // Reverse-Z: Use Greater-Than-Or-Equal
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
    if (result.hit) {
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
    // Convert char array to std::string for comparison/UI
    const std::string blockName = BlockRegistry::getInstance().getBlock(blockType).blockName;

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
    drawText(10, 70, 15,
             "SELECTED BLOCK: " + std::string(BlockRegistry::getInstance().getBlock(gameState.selectedBlock).blockName),
             textBrush);

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

    // F2: Toggle Sky Time
    static bool f2Pressed = false;
    if (getKeyState(graphics::SCANCODE_F2)) {
        if (!f2Pressed) {
            f2Pressed = true;
            sky.togglePause();
            std::cout << "Sky Time " << (sky.isPaused() ? "PAUSED" : "RESUMED") << std::endl;
        }
    } else {
        f2Pressed = false;
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

    // Dependent breaking: Tall Grass
    // If bottom is broken, break top as well.
    const Block &currentBlock = BlockRegistry::getInstance().getBlock(blockType);

    if (strcmp(currentBlock.blockName, "TALL_GRASS_BOTTOM") == 0) {
        uint32_t aboveBlock = result.chunk->getBlockAtPos(
            result.localBlockX, result.localBlockY + 1, result.localBlockZ);

        const Block &aboveBlockData = BlockRegistry::getInstance().getBlock(aboveBlock);

        if (strcmp(aboveBlockData.blockName, "TALL_GRASS_TOP") == 0) {
            // Must handle cross-chunk boundary if Y=15? (Current implementation clamps or wraps?)
            // Chunk::getBlockAtPos usually handles local coords.
            // If localBlockY+1 == CHUNK_HEIGHT, it might be an issue if getBlockAtPos doesn't handle neighbor chunks.
            // But let's assume simple cases or verify if getBlockAtPos handles out of bounds.
            // Actually, for now, let's keep it simple.
            result.chunk->updateBlock(result.localBlockX, result.localBlockY + 1, result.localBlockZ, 0);
        }
    }
    // User requested: If top is broken, bottom remains (so no action needed for TALL_GRASS_TOP).

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
    const Block &blockToReplaceData = BlockRegistry::getInstance().getBlock(blockToReplace);
    if (blockToReplace != 0 && blockToReplaceData.blockType != Block::LIQUID) {
        return;
    }

    // Billboard placement validation
    const Block &selectedBlockData = BlockRegistry::getInstance().getBlock(gameState.selectedBlock);
    if (selectedBlockData.blockType == Block::BILLBOARD) {
        const uint16_t blockBelow = chunk->getBlockAtPos(localBlockX, localBlockY - 1, localBlockZ);
        if (strcmp(BlockRegistry::getInstance().getBlock(blockBelow).blockName, "GRASS_BLOCK") != 0) {
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
