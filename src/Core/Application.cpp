/**
 * @file Application.cpp
 * @brief Entry point for the game.
 * 
 * This file contains the main function which starts our engine by
 * initializing the central GameObject.
 */
#include "GameObject.h"

int main() {
    GameObject& gObject = GameObject::getInstance(1280,720, "");
    gObject.init();
    return 0;
}