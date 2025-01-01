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
    : gameState(), lastX(x / 2), lastY(y / 2), windowX(x), windowY(y), window_name(windowName) {
}

// Define the destructor
GameObject::~GameObject() {
    graphics::stopMessageLoop();
    graphics::destroyWindow();
}

void GameObject::init() {
    // Initialize the graphics system
    graphics::createWindow(windowX, windowY, "");

    // Camera and shader initialization
    std::string path = "../assets/sprites/block_map.png";
    camera = Camera(glm::vec3(0.0f, 25.0f, 0.0f));

    textureManager = &graphics::TextureManager::getInstance();
    graphics::Texture *texture = textureManager->createTexture(
        path, false, [&](graphics::Texture &tex) {
            stbi_set_flip_vertically_on_load(true);
            unsigned char *data =
                    stbi_load(path.c_str(), (int *) tex.getWidthPointer(), (int *) tex.getHeightPointer(),
                              (int *) tex.getChannelsPointer(), 0);
            glGenTextures(1, tex.getIDPointer());
            glBindTexture(GL_TEXTURE_2D, tex.getID());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.getWidth(), tex.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
        });
    textureManager->bindTexture(texture);

    //Init all shaders ---------------------------------------------------------------------------------------------------------
    worldShader = Shader("../assets/shaders/world_vertex_shader.glsl", "../assets/shaders/world_fragment_shader.glsl");
    worldShader["SMART_texMultiplier"] = 0.0625f;
    worldShader["SMART_tex"] = (int) texture->getBoundSlot();
    //--------------------------------------------------------------------------------------------------------------------------
    billboardShader = Shader("../assets/shaders/billboard_vertex_shader.glsl",
                             "../assets/shaders/billboard_fragment_shader.glsl");
    billboardShader["SMART_texMultiplier"] = 0.0625f;
    billboardShader["SMART_tex"] = (int) texture->getBoundSlot();
    //--------------------------------------------------------------------------------------------------------------------------
    fluidShader = Shader("../assets/shaders/fluids_vertex_shader.glsl",
                         "../assets/shaders/fluids_fragment_shader.glsl");
    fluidShader["SMART_texMultiplier"] = 0.0625f;
    fluidShader["SMART_tex"] = (int) texture->getBoundSlot();
    //--------------------------------------------------------------------------------------------------------------------------
    outlineShader = Shader("../assets/shaders/block_outline_vertex_shader.glsl",
                           "../assets/shaders/block_outline_fragment_shader.glsl");
    //--------------------------------------------------------------------------------------------------------------------------

    unsigned int outlineVAO, outlineVBO;
    glGenVertexArrays(1, &outlineVAO);
    glGenBuffers(1, &outlineVBO);
    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVertices), &outlineVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);

    fpsSlider = Slider(windowX / 2, windowY / 2, 300.0f, 20.0f, 0.0f, 100.0f, 50.0f);

    Planet::planet = new Planet(&worldShader, &fluidShader, &billboardShader);

    graphics::setPreDrawFunction([this,outlineVAO] {
        std::string window_name
                = "Fake Minecraft / FPS: "
                  + std::to_string(graphics::getFPS())
                  + " Total Chunks: "
                  + std::to_string(Planet::planet->numChunks)
                  + " Rendered Chunks: "
                  + std::to_string(Planet::planet->numChunksRendered);

        graphics::setWindowName(window_name.c_str()); {
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
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), windowX / windowY, 0.1f, 100.0f);
        //--------------------------------------------
        worldShader.use(true);
        worldShader["SMART_view"] = view;
        worldShader["SMART_projection"] = projection;
        //--------------------------------------------
        billboardShader.use(true);
        billboardShader["SMART_view"] = view;
        billboardShader["SMART_projection"] = projection;
        //--------------------------------------------
        fluidShader.use(true);
        fluidShader["SMART_view"] = view;
        fluidShader["SMART_projection"] = projection;
        fluidShader["SMART_time"] = SDL_GetTicks() / 1000.0f;
        //--------------------------------------------
        outlineShader.use(true);
        outlineShader["SMART_view"] = view;
        outlineShader["SMART_projection"] = projection;
        //--------------------------------------------

        Planet::planet->update(camera.Position); {
            // Get block position
            outlineShader.use(true);
            auto result = Physics::raycast(camera.Position, camera.Front, 5);
            if (result.hit) {
                // Set outline view to position

                outlineShader["model"] = glm::vec4(result.blockX, result.blockY, result.blockZ, 1);
                // Render
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
    });

    graphics::setResizeFunction([this](int width, int height) {
        windowX = width;
        windowY = height;
    });

    graphics::setDrawFunction([this] {
        //Crosshair rendering
        graphics::Brush bck;

        setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);
        graphics::setCanvasSize(windowX, windowY);

        // Check if player is underwater
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

            if (Blocks::blocks[blockType].blockType == Block::LIQUID) {
                bck.fill_color[0] = 0.0;
                bck.fill_color[1] = 0.0;
                bck.fill_color[2] = 0.45;
                bck.fill_opacity = 0.6;
                drawRect(windowX / 2, windowY / 2, windowX, windowY, bck);
            }
        }

        switch (gameState.state) {
            case PLAYING:
                bck.fill_color[0] = 0.7;
                bck.fill_color[1] = 0.7;
                bck.fill_color[2] = 0.7;
                bck.fill_opacity = 1.0f;
                drawRect(windowX / 2, windowY / 2, 5, 25, bck);
                drawRect(windowX / 2, windowY / 2, 25, 5, bck);
                break;
            case PAUSED:
                bck.fill_color[0] = 0.2;
                bck.fill_color[1] = 0.2;
                bck.fill_color[2] = 0.2;
                bck.fill_opacity = 0.9f;
                drawRect(windowX / 2, windowY / 2, windowX, windowY, bck);
                fpsSlider.draw();
                break;
        }
    });


    graphics::setUpdateFunction([this](float dt) {
        keyboardCallBack(dt);
        mouseCallBack();
    });

    graphics::startMessageLoop();
}

GameObject &GameObject::getInstance(float x, float y, const std::string &windowName) {
    static GameObject instance(x, y, windowName);
    return instance;
}

void GameObject::mouseCallBack() {
    graphics::MouseState ms;
    getMouseState(ms);

    switch (gameState.state) {
        case PLAYING: {
            // Handle mouse movement
            float xoffset = static_cast<float>(ms.rel_x);
            float yoffset = static_cast<float>(-ms.rel_y);
            camera.processMouseMovement(xoffset, yoffset, true);

            // Handle mouse button press events
            if (ms.button_left_pressed || ms.button_middle_pressed || ms.button_right_pressed) {
                // Perform the raycast only once
                auto result = Physics::raycast(camera.Position, camera.Front, 5);
                if (!result.hit)
                    return;

                if (ms.button_left_pressed) {
                    // Handle left-click (destroy block)
                    result.chunk->updateBlock(result.localBlockX, result.localBlockY, result.localBlockZ, 0);
                }

                if (ms.button_middle_pressed) {
                    // Handle middle-click (select block)
                    gameState.selectedBlock = result.chunk->getBlockAtPos(result.localBlockX, result.localBlockY,
                                                                          result.localBlockZ);
                }

                if (ms.button_right_pressed) {
                    // Handle right-click (place block)
                    float distX = result.hitPos.x - (result.blockX + 0.5f);
                    float distY = result.hitPos.y - (result.blockY + 0.5f);
                    float distZ = result.hitPos.z - (result.blockZ + 0.5f);

                    int blockX = result.blockX;
                    int blockY = result.blockY;
                    int blockZ = result.blockZ;

                    // Determine which face to place on
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

                    // Check if the target position is air or liquid
                    if (blockToReplace == 0 || Blocks::blocks[blockToReplace].blockType == Block::LIQUID) {
                        // If the selected block is a billboard-type block
                        if (Blocks::blocks[gameState.selectedBlock].blockType == Block::BILLBOARD) {
                            if (Blocks::blocks[blockBelow].blockName != "Grass Block") {
                                return;
                            }
                        }

                        // Handle block placement normally
                        chunk->updateBlock(localBlockX, localBlockY, localBlockZ, gameState.selectedBlock);
                    }
                }
            }
            break;
        }
        case PAUSED: {
            // Q&A 😭
            ms.button_left_down
                ? fpsSlider.startDragging(ms.cur_pos_x, ms.cur_pos_y)
                : fpsSlider.stopDragging();

            //Action
            fpsSlider.update(ms.cur_pos_x);
            break;
        }
    }
}


void GameObject::keyboardCallBack(float deltaTime) {
    if (getKeyState(graphics::SCANCODE_ESCAPE))
        SDL_GetRelativeMouseMode()
            ? gameState.state = PLAYING
            : gameState.state = PAUSED;
    gameState.state == PAUSED ? deltaTime = 0 : deltaTime;
    if (getKeyState(graphics::SCANCODE_W)) camera.processKeyboard(FORWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_S)) camera.processKeyboard(BACKWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_A)) camera.processKeyboard(LEFT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_D)) camera.processKeyboard(RIGHT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_SPACE)) camera.processKeyboard(UP, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_LSHIFT)) camera.processKeyboard(DOWN, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_F11)) {
        if (fullScreen) {
            fullScreen = false;
            graphics::setFullScreen(false);
        } else {
            fullScreen = true;
            graphics::setFullScreen(true);
        }
    }
}
