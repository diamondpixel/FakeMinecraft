#pragma once

#include <string>
#include <Graphics.h>
#include "Camera.h"
#include "Planet.h"
#include <Shader.h>

#include "./UI/headers/CheckBox.h"
#include "./UI/headers/Slider.h"
#include "./UI/headers/TypeBox.h"

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
    int fpsCap = 144;

    GameObject(float x, float y, const std::string &windowName);

    virtual ~GameObject();

    void initializeGraphics();

    void initializeShaders();

    void initializeUIElements();

    void setupCallbacks();

    void handleDraw();

    void updateShaders();

    static void setupRenderingState();

    std::string window_name;
    float windowX, windowY;

    void mouseCallBack();

    void handlePausedMouseInput(graphics::MouseState &ms);

    void handlePlayingMouseInput(graphics::MouseState &ms);

    void keyboardCallBack(float deltaTime);

    static void playSound(uint16_t block_id);

    Camera camera;
    graphics::TextureManager *textureManager;
    glm::vec3 inputDirection;

    graphics::Texture* gameTexture;

    Shader worldShader;
    Shader billboardShader;
    Shader fluidShader;
    Shader outlineShader;

    TypeBox seedBox;
    Slider renderDistanceSlider;
    Slider fpsSlider;
    Checkbox fullscreenCheckBox;

};


#endif
