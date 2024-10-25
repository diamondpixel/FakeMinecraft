#include <chrono>
#include <thread>

#include "graphics.h"
#include "./graphicsEngine3D/3DEngine.cpp"

// Define engine as a global variable
GraphicsEngine3D *engine = nullptr;

int main() {

    engine = new GraphicsEngine3D(); // Initialize the engine
    engine->createWindow(1200,600,"Hello world");
    graphics::setDrawFunction([]() { engine->draw(); });
    graphics::setUpdateFunction([](float deltaTime) { engine->update(deltaTime); });

    graphics::setCanvasSize(1200, 600);
    setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);

    graphics::startMessageLoop();


    delete engine;
    return 0;
}

