#include <iostream>
#include <ostream>
#include <thread>
#include "graphics.h"
#include "Engine/GraphicsEngine3D.h"

// Define engine as a global variable
GraphicsEngine3D *engine = nullptr;

int main() {
    engine = new GraphicsEngine3D();  // Initialize the engine

    // Create the window with the engine
    engine->createWindow(1200, 600, "Hello World");

    // Set the functions for drawing and updating
    graphics::setDrawFunction([]() { engine->draw(); });
    graphics::setUpdateFunction([](float deltaTime) { engine->update(deltaTime); });

    // Set the canvas properties
    graphics::setCanvasSize(1200, 600);
    setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);

    // Start the main loop
    graphics::startMessageLoop();

    // Clean up
    delete engine;
    return 0;
}
