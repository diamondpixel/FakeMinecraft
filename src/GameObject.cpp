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
    graphics::setVSYNC(false);


    // Camera and shader initialization
    std::string path = "C:/Users/stili/Desktop/FakeMinecraft/assets/sprites/block_map.png";
    camera = Camera(glm::vec3(0.0f, 25.0f, 0.0f));
    shader = Shader("C:/Users/stili/Desktop/FakeMinecraft/assets/shaders/vertex_shader.glsl",
                    "C:/Users/stili/Desktop/FakeMinecraft/assets/shaders/fragment_shader.glsl");


    textureManager = &graphics::TextureManager::getInstance();
    graphics::Texture *texture = textureManager->createTexture(
        path, false, [&](graphics::Texture &tex) {
            stbi_set_flip_vertically_on_load(true);
            unsigned char *data =
                stbi_load(path.c_str(), (int *) tex.getWidthPointer(), (int *) tex.getHeightPointer(), (int *) tex.getChannelsPointer(), 0);
            glGenTextures(1, tex.getIDPointer());
            glBindTexture(GL_TEXTURE_2D, tex.getID());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.getWidth(), tex.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
        });

    Planet::planet = new Planet();

    graphics::setPreDrawFunction([this, texture] {
        std::string window_name = "Fake Minecraft / FPS: " + std::to_string(graphics::getFPS());
        graphics::setWindowName(window_name.c_str());
        {
            shader.use(true);
            textureManager->bindTexture(texture);
            shader["tex"] = (int)texture->getBoundSlot();
            shader["texMultiplier"] = 0.25f;
            glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CW);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_BLEND);
        }
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), windowX / windowY, 0.1f, 100.0f);
        shader["view"] = view;
        shader["projection"] = projection;
        GLuint modelLoc = glGetUniformLocation(shader.program, "model");
        Planet::planet->update(camera.Position.x, camera.Position.y, camera.Position.z, modelLoc);
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
    float xoffset = static_cast<float>(ms.rel_x);
    float yoffset = static_cast<float>(-ms.rel_y);
    camera.processMouseMovement(xoffset, yoffset, true);
}

void GameObject::keyboardCallBack(float deltaTime) {
    if (getKeyState(graphics::SCANCODE_W)) camera.processKeyboard(FORWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_S)) camera.processKeyboard(BACKWARD, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_A)) camera.processKeyboard(LEFT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_D)) camera.processKeyboard(RIGHT, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_E)) camera.processKeyboard(UP, deltaTime * .01f);
    if (getKeyState(graphics::SCANCODE_Q)) camera.processKeyboard(DOWN, deltaTime * .01f);
}
