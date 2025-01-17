#pragma once

#include <Graphics.h>
#include "headers/GameObject.h"
#include <iostream>
#include "headers/Camera.h"
#include "headers/Planet.h"
#include <Shader.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL.h>
#include "headers/Blocks.h"
#include "headers/Physics.h"
#include <string>
#include <sstream>


int convertToASCIIsum(const std::string &input) {
    int sum = 0;
    for (char c: input) {
        if (sum > INT_MAX - static_cast<int>(c)) {
            return INT_MAX;
        }
        if (sum < INT_MIN + static_cast<int>(c)) {
            return INT_MIN;
        }
        sum += static_cast<int>(c);
    }
    return sum;
}

float outlineVertices[] =
{
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

GameObject::GameObject(float x, float y, const std::string &windowName)
    : lastX(x / 2), lastY(y / 2), windowX(x), windowY(y), window_name(windowName) {
}

// Define the destructor
GameObject::~GameObject() {
    graphics::stopMessageLoop();
    graphics::destroyWindow();
}

void GameObject::init() {
    initializeGraphics();
    initializeShaders();
    initializeUIElements();

    Planet::planet = new Planet(&worldShader, &fluidShader, &billboardShader);

    unsigned int outlineVAO, outlineVBO;
    glGenVertexArrays(1, &outlineVAO);
    glGenBuffers(1, &outlineVBO);
    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVertices), &outlineVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);


    graphics::setPreDrawFunction([this,outlineVAO] {
        std::string window_name
                = "Fake Minecraft / FPS: "
                  + std::to_string(graphics::getFPS())
                  + " Total Chunks: "
                  + std::to_string(Planet::planet->numChunks);

        graphics::setWindowName(window_name.c_str());
        textureManager->bindTexture(gameTexture, 0);
        setupRenderingState();

        if (fpsCap == 55) {
            graphics::setVSYNC(true);
            graphics::setTargetFPS(-1);
        } else if (fpsCap <= 360) {
            graphics::setVSYNC(false);
            graphics::setTargetFPS(fpsCap);
        } else {
            graphics::setVSYNC(false);
            graphics::setTargetFPS(-1);
        }

        updateShaders();

        Planet::planet->update(camera.Position); {
            outlineShader.use(true);
            auto result = Physics::raycast(camera.Position, camera.Front, 5); //THIS IS COLLISION TECHNICALLY
            if (result.hit) {
                outlineShader["model"] = glm::vec4(result.blockX, result.blockY, result.blockZ, 1);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLogicOp(GL_INVERT);
                glDisable(GL_CULL_FACE);
                glBindVertexArray(outlineVAO);
                glLineWidth(2.0);
                glDrawArrays(GL_LINES, 0, 24);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glEnable(GL_CULL_FACE);
            }
        }

        SEED = convertToASCIIsum(seedBox.getText());
    });

    graphics::setResizeFunction([this](int width, int height) {
        renderDistanceSlider.setDimensions(windowX / 2 - 100, windowY / 2 + 130.0f);
        fpsSlider.setDimensions(windowX / 2 - 100, windowY / 2 + 70.0f);
        fullscreenCheckBox.setDimensions(windowX / 2, windowY / 2);
        seedBox.setDimensions(windowX / 2, windowY / 2 + 160.0f);
        windowX = width;
        windowY = height;
    });

    graphics::setDrawFunction([this] {
        graphics::Brush bck;
        setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);
        graphics::setCanvasSize(windowX, windowY);
        int blockX = camera.Position.x < 0 ? camera.Position.x - 1 : camera.Position.x;
        int blockY = camera.Position.y < 0 ? camera.Position.y - 1 : camera.Position.y;
        int blockZ = camera.Position.z < 0 ? camera.Position.z - 1 : camera.Position.z;

        int chunkX = blockX < 0 ? floorf(blockX / (float) CHUNK_SIZE) : blockX / (int) CHUNK_SIZE;
        int chunkY = blockY < 0 ? floorf(blockY / (float) CHUNK_SIZE) : blockY / (int) CHUNK_SIZE;
        int chunkZ = blockZ < 0 ? floorf(blockZ / (float) CHUNK_SIZE) : blockZ / (int) CHUNK_SIZE;

        int localBlockX = blockX - (chunkX * CHUNK_SIZE);
        int localBlockY = blockY - (chunkY * CHUNK_SIZE);
        int localBlockZ = blockZ - (chunkZ * CHUNK_SIZE);

        Chunk *chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
        if (chunk != nullptr) {
            unsigned int blockType = chunk->getBlockAtPos(
                localBlockX,
                localBlockY,
                localBlockZ);

            if (Blocks::blocks[blockType].blockName == "WATER") {
                bck.fill_color[0] = 0.0;
                bck.fill_color[1] = 0.0;
                bck.fill_color[2] = 0.45;
                bck.fill_opacity = 0.6;
                drawRect(windowX / 2, windowY / 2, windowX, windowY, bck);
            } else if (Blocks::blocks[blockType].blockName == "LAVA") {
                bck.fill_color[0] = 1.0;
                bck.fill_color[1] = 0.5;
                bck.fill_color[2] = 0.0;
                bck.fill_opacity = 0.4;
                drawRect(windowX / 2, windowY / 2, windowX, windowY, bck);
            }
        }

        switch (gameState.state) {
            case PLAYING: {
                bck.fill_color[0] = 0.7;
                bck.fill_color[1] = 0.7;
                bck.fill_color[2] = 0.7;
                bck.fill_opacity = 1.0f;
                bck.outline_opacity = 0.0f;
                drawRect(windowX / 2, windowY / 2, 5, 25, bck);
                drawRect(windowX / 2, windowY / 2, 25, 5, bck);

                bck.fill_color[0] = 0.0;
                bck.fill_color[1] = 0.0;
                bck.fill_color[2] = 0.0;

                std::string coordsText = "COORDS: " +
                                         std::to_string(static_cast<int>(camera.Position.x)) + ", " +
                                         std::to_string(static_cast<int>(camera.Position.y)) + ", " +
                                         std::to_string(static_cast<int>(camera.Position.z));

                drawText(10, 30, 15, "FPS: " + std::to_string((int) graphics::getFPS()), bck);
                drawText(10, 50, 15, coordsText, bck);
                drawText(10, 70, 15, "SELECTED BLOCK: " + Blocks::blocks[gameState.selectedBlock].blockName, bck);

                break;
            }
            case PAUSED:
                bck.outline_opacity = 0.0f;
                bck.fill_color[0] = 0.2;
                bck.fill_color[1] = 0.2;
                bck.fill_color[2] = 0.2;
                bck.fill_opacity = 0.9f;
                drawRect(windowX / 2, windowY / 2, windowX, windowY, bck);
                graphics::Brush br;
                br.fill_color[0] = 0.9;
                br.fill_color[1] = 0.9;
                br.fill_color[2] = 0.9;
                renderDistanceSlider.draw();
                fpsSlider.draw();
                fullscreenCheckBox.draw();
                seedBox.draw();

                graphics::Brush TextBrush;

                if (fpsCap == 55) {
                    drawText(windowX / 2 - 40, windowY / 2 + 50.0f, 20, "FPS: VSYNC", TextBrush);
                } else if (fpsCap <= 360) {
                    drawText(windowX / 2 - 40, windowY / 2 + 50.0f, 20, "FPS: " + std::to_string(fpsCap), TextBrush);
                } else {
                    drawText(windowX / 2 - 40, windowY / 2 + 50.0f, 20, "FPS: UNCAPPED", TextBrush);
                }
            //-----------------------------------------------------------------------------------------------------------------------------
                drawText(windowX / 2 - 80, windowY / 2 + 110.0f, 20,
                         "Render Distance: " + std::to_string(Planet::planet->renderDistance), TextBrush);
                drawText(windowX / 2 - 40, windowY / 2 - 40, 20, "FullScreen", TextBrush);
                break;
        }
    });

    graphics::setUpdateFunction([this](float dt) {
        keyboardCallBack(dt);
        mouseCallBack();
    });

    graphics::playMusic("../assets/sounds/songs/Minecraft Volume Alpha.ogg", 0.8f, true);
    graphics::startMessageLoop();
}

void GameObject::initializeGraphics() {
    // Initialize the graphics system
    graphics::createWindow(windowX, windowY, "");-
    graphics::setFont("../assets/fonts/Arial.ttf");
    std::string path = "../assets/sprites/block_map.png";

    camera = Camera(glm::vec3(0.0f, MAX_HEIGHT + 10, 0.0f));
    textureManager = &graphics::TextureManager::getInstance();

    gameTexture = textureManager->createTexture(
        path, false, [&](graphics::Texture &tex) {
            stbi_set_flip_vertically_on_load(true);
            unsigned char *data = stbi_load(
                path.c_str(), reinterpret_cast<int *>(tex.getWidthPointer()),
                reinterpret_cast<int *>(tex.getHeightPointer()),
                reinterpret_cast<int *>(tex.getChannelsPointer()), 4 // Force RGBA
            );

            if (!data) {
                std::cerr << "Failed to load texture: " << path << std::endl;
                return;
            }

            std::vector<unsigned char> *buffer = tex.getBufferPointer();
            if (buffer) {
                buffer->assign(data, data + tex.getWidth() * tex.getHeight() * 4); // 4 channels (RGBA)
            }
            stbi_image_free(data);
            glGenTextures(1, tex.getIDPointer());
            glBindTexture(GL_TEXTURE_2D, tex.getID());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA, tex.getWidth(), tex.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer->data()
            );
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        });
    textureManager->bindTexture(gameTexture, 0);
}

void GameObject::initializeShaders() {
    auto slotBound = textureManager->getBoundSlotOfTexture(gameTexture);
    //Init all shaders ---------------------------------------------------------------------------------------------------------
    worldShader = Shader("../assets/shaders/world_vertex_shader.glsl",
                         "../assets/shaders/world_fragment_shader.glsl");
    worldShader["SMART_texMultiplier"] = 0.0625f;
    worldShader["SMART_tex"] = slotBound;
    //--------------------------------------------------------------------------------------------------------------------------
    billboardShader = Shader("../assets/shaders/billboard_vertex_shader.glsl",
                             "../assets/shaders/billboard_fragment_shader.glsl");
    billboardShader["SMART_texMultiplier"] = 0.0625f;
    billboardShader["SMART_tex"] = slotBound;
    //--------------------------------------------------------------------------------------------------------------------------
    fluidShader = Shader("../assets/shaders/fluids_vertex_shader.glsl",
                         "../assets/shaders/fluids_fragment_shader.glsl");
    fluidShader["SMART_texMultiplier"] = 0.0625f;
    fluidShader["SMART_tex"] = slotBound;
    //--------------------------------------------------------------------------------------------------------------------------
    outlineShader = Shader("../assets/shaders/block_outline_vertex_shader.glsl",
                           "../assets/shaders/block_outline_fragment_shader.glsl");
    //--------------------------------------------------------------------------------------------------------------------------
}

void GameObject::initializeUIElements() {
    fullscreenCheckBox = Checkbox(windowX / 2, windowY / 2, 50);
    fpsSlider = Slider(windowX / 2 - 100, windowY / 2 + 50.0f, 200.0f, 20.0f, 55.0f, 361.0f, 144.0f);
    renderDistanceSlider = Slider(windowX / 2 - 100, windowY / 2 + 130.0f, 200.0f, 20.0f, 1.0f, 30.f, 5.f);
    seedBox = TypeBox(windowX / 2 - 100, windowY / 2 + 230.0f, 200.0f, 20.0f);
}

void GameObject::updateShaders() {
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(90.0f, windowX / windowY, 0.1f, 10000.0f);

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

GameObject &GameObject::getInstance(float x, float y, const std::string &windowName) {
    static GameObject instance(x, y, windowName);
    return instance;
}

void GameObject::mouseCallBack() {
    graphics::MouseState ms{};
    getMouseState(ms);

    switch (gameState.state) {
        case PLAYING:
            handlePlayingMouseInput(ms);
            break;
        case PAUSED:
            handlePausedMouseInput(ms);
            break;
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
    //Action
    renderDistanceSlider.update(ms.cur_pos_x, Planet::planet->renderDistance);
    fpsSlider.update(ms.cur_pos_x, fpsCap);
}

void GameObject::handlePlayingMouseInput(graphics::MouseState &ms) {
    auto x_offset = static_cast<float>(ms.rel_x);
    auto y_offset = static_cast<float>(-ms.rel_y);
    camera.processMouseMovement(x_offset, y_offset, true);
    if (ms.button_left_pressed || ms.button_middle_pressed || ms.button_right_pressed) {
        auto result = Physics::raycast(camera.Position, camera.Front, 5);
        if (!result.hit)
            return;

        if (ms.button_left_pressed) {
            playSound(result.chunk->getBlockAtPos(result.localBlockX, result.localBlockY,
                                                  result.localBlockZ));
            result.chunk->updateBlock(result.localBlockX, result.localBlockY, result.localBlockZ, 0);
        }

        if (ms.button_middle_pressed) {
            gameState.selectedBlock = result.chunk->getBlockAtPos(result.localBlockX, result.localBlockY,
                                                                  result.localBlockZ);
        }

        if (ms.button_right_pressed) {
            float distX = result.hitPos.x - (result.blockX + 0.5f);
            float distY = result.hitPos.y - (result.blockY + 0.5f);
            float distZ = result.hitPos.z - (result.blockZ + 0.5f);

            int blockX = result.blockX;
            int blockY = result.blockY;
            int blockZ = result.blockZ;

            if (abs(distX) > abs(distY) && abs(distX) > abs(distZ))
                blockX += (distX > 0 ? 1 : -1);
            else if (abs(distY) > abs(distX) && abs(distY) > abs(distZ))
                blockY += (distY > 0 ? 1 : -1);
            else
                blockZ += (distZ > 0 ? 1 : -1);

            int chunkX = blockX < 0 ? floorf(blockX / (float) CHUNK_SIZE) : blockX / (int) CHUNK_SIZE;
            int chunkY = blockY < 0 ? floorf(blockY / (float) CHUNK_SIZE) : blockY / (int) CHUNK_SIZE;
            int chunkZ = blockZ < 0 ? floorf(blockZ / (float) CHUNK_SIZE) : blockZ / (int) CHUNK_SIZE;

            int localBlockX = blockX - (chunkX * CHUNK_SIZE);
            int localBlockY = blockY - (chunkY * CHUNK_SIZE);
            int localBlockZ = blockZ - (chunkZ * CHUNK_SIZE);

            Chunk *chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
            uint16_t blockToReplace = chunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);
            uint16_t blockBelow = chunk->getBlockAtPos(localBlockX, localBlockY - 1, localBlockZ);
            if (blockToReplace == 0 || Blocks::blocks[blockToReplace].blockType == Block::LIQUID) {
                if (Blocks::blocks[gameState.selectedBlock].blockType == Block::BILLBOARD) {
                    if (Blocks::blocks[blockBelow].blockName != "GRASS_BLOCK") {
                        return;
                    }
                }
                playSound(gameState.selectedBlock);
                chunk->updateBlock(localBlockX, localBlockY, localBlockZ, gameState.selectedBlock);
            }
        }
    }
}

void GameObject::keyboardCallBack(float deltaTime) {
    static bool f11Pressed = false;

    // Toggle game state with ESC
    if (getKeyState(graphics::SCANCODE_ESCAPE)) {
        gameState.state = SDL_GetRelativeMouseMode() ? PLAYING : PAUSED;
    }

    // Handle input when the game is paused
    if (gameState.state == PAUSED) {
        deltaTime = 0; // No movement in paused state
        seedBox.handleInput();
    } else {
        const float movementSpeed = deltaTime * 0.02f;
        const std::array<std::pair<graphics::scancode_t, Camera_Movement>, 6> keyBindings = {
            std::make_pair(graphics::SCANCODE_W, FORWARD),
            std::make_pair(graphics::SCANCODE_S, BACKWARD),
            std::make_pair(graphics::SCANCODE_A, LEFT),
            std::make_pair(graphics::SCANCODE_D, RIGHT),
            std::make_pair(graphics::SCANCODE_SPACE, UP),
            std::make_pair(graphics::SCANCODE_LSHIFT, DOWN)
        };

        for (const auto &[key, direction]: keyBindings) {
            if (getKeyState(key)) {
                camera.processKeyboard(direction, movementSpeed);
            }
        }
    }

    // Handle F11 fullscreen toggle
    if (getKeyState(graphics::SCANCODE_F11)) {
        if (!f11Pressed) {
            f11Pressed = true;
            bool isChecked = fullscreenCheckBox.isChecked();
            fullscreenCheckBox.setChecked(!isChecked);
            graphics::setFullScreen(!isChecked);
        }
    } else {
        f11Pressed = false; // Reset when F11 is released
    }
}

void GameObject::playSound(uint16_t block_id) {
    switch (block_id) {
        case 1:
            graphics::playSound("../assets/sounds/DIRT.ogg", .4f);
            break;
        case 2:
            graphics::playSound("../assets/sounds/GRASS_BLOCK.ogg", .4f);
            break;
        case 3:
            graphics::playSound("../assets/sounds/STONE_BLOCK.ogg", .4f);
            break;
        case 4:
            graphics::playSound("../assets/sounds/LOG.ogg", .4f);
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            graphics::playSound("../assets/sounds/GRASS.ogg", .4f);
            break;
        case 15:
            graphics::playSound("../assets/sounds/SAND.ogg", 1.f);
            break;
        case 16:
        case 17:
            graphics::playSound("../assets/sounds/STONE_BLOCK.ogg", .4f);
            break;
        default: break;
    }
}
