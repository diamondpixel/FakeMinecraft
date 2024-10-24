#include "graphics.h"
#include "./graphicsEngine3D/3DEngine.cpp"

// Define engine as a global variable
GraphicsEngine3D *engine = nullptr;

void update(float ms) {
    graphics::MouseState mouse;
    graphics::getMouseState(mouse);

    if (mouse.button_left_released) {
    }
}

int main() {
    engine = new GraphicsEngine3D(); // Initialize the engine
    engine->createWindow(1200, 600, "Hello World");

    // Initially set the draw function
    graphics::setDrawFunction([]() { engine->draw(); });
    graphics::setUpdateFunction(update);

    graphics::setCanvasSize(1200, 600);
    graphics::setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);

    graphics::startMessageLoop();

    delete engine;
    return 0;
}
