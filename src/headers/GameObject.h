#pragma once

#include <string>
#include <Graphics.h>
#include "Camera.h"
#include "Planet.h"
#include <Shader.h>

#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H


enum State {
    PLAYING,
    PAUSED
};

class GameState {
public:
    State state;
    uint16_t selectedBlock;

    GameState(): state(), selectedBlock(0) {
    };
};

class GameObject {
public:
    virtual void init();

    static GameObject &getInstance(float x = 0, float y = 0, const std::string &windowName = "Default");

    GameObject(const GameObject &) = delete;

    GameObject &operator=(const GameObject &) = delete;

private:
    GameState gameState;
    float lastX = 400, lastY = 300;
    bool firstMouse = true;

    GameObject(float x, float y, const std::string &windowName);

    virtual ~GameObject();

    std::string window_name;
    float windowX, windowY;

    void mouseCallBack();

    void keyboardCallBack(float deltaTime);

    Camera camera;
    graphics::TextureManager *textureManager;
    glm::vec3 inputDirection;

    Shader worldShader;
    Shader billboardShader;
    Shader fluidShader;
    Shader outlineShader;
};


#endif
