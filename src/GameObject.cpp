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


GameObject::GameObject(float x, float y, const std::string &windowName)
    : lastX(x / 2), lastY(y / 2), windowX(x), windowY(y), window_name(windowName) {
}

// Define the destructor
GameObject::~GameObject() {
    graphics::stopMessageLoop();
    graphics::destroyWindow();
}

void GameObject::init() {
    // Initialize the graphics system
    graphics::createWindow(windowX, windowY, "");
    graphics::setFullScreen(false);
    graphics::setVSYNC(true);


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


    Planet::planet = new Planet(&worldShader, &fluidShader, &billboardShader);

    graphics::setPreDrawFunction([this, texture] {
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
        Planet::planet->update(camera.Position);

    });

    graphics::setResizeFunction([this](int width, int height) {
        windowX = width;
        windowY = height;
    });

    graphics::setDrawFunction([this] {
        graphics::Brush bck;
        bck.fill_color[0] = 0.7;
        bck.fill_color[1] = 0.7;
        bck.fill_color[2] = 0.7;
        if (gameState == PLAYING) {
            drawRect(windowX / 2, windowY / 2, 5, 25, bck);
            drawRect(windowX / 2, windowY / 2, 25, 5, bck);
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
    float xoffset = gameState == PAUSED ? 0 : static_cast<float>(ms.rel_x);
    float yoffset = gameState == PAUSED ? 0 : static_cast<float>(-ms.rel_y);
    camera.processMouseMovement(xoffset, yoffset, true);
}

void GameObject::keyboardCallBack(float deltaTime) {
    if (getKeyState(graphics::SCANCODE_ESCAPE)) SDL_GetRelativeMouseMode() ? gameState = PLAYING : gameState = PAUSED;
    gameState == PAUSED ? deltaTime = 0 : deltaTime;
    if (getKeyState(graphics::SCANCODE_W)) camera.processKeyboard(FORWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_S)) camera.processKeyboard(BACKWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_A)) camera.processKeyboard(LEFT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_D)) camera.processKeyboard(RIGHT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_SPACE)) camera.processKeyboard(UP, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_LSHIFT)) camera.processKeyboard(DOWN, deltaTime * .01f);
}
