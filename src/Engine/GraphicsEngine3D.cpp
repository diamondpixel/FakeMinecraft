#include "GraphicsEngine3D.h"
#include "graphics.h"
#include <string>

// Constructor
GraphicsEngine3D::GraphicsEngine3D() : vCamera{0, 0, 0}, vLookDir{0, 0, 1}, fYaw(0), fTheta(0) {}

// Function to create a window and initialize the projection matrix
void GraphicsEngine3D::createWindow(int width, int height, std::string windowName) {
    this->width = width;
    this->height = height;

    // Load the mesh from an object file
    meshCube.loadFromObjectFile(R"(../assets/mountains.obj)");

    // Setup the projection matrix with field of view, aspect ratio, near, and far planes
    matProj = Matrix4x4::makeProjection(90.0f, (float)height / (float)width, 0.1f, 1000.0f);

    // Set the window title
    graphics::createWindow(width, height, std::move(windowName));
}

// Function to handle the drawing logic
void GraphicsEngine3D::draw() {
    // Set the window name to show the FPS
    graphics::setWindowName(std::to_string(graphics::getFPS()).c_str());

    // Apply the world transformation (identity matrix for now)
    Matrix4x4 matWorld = Matrix4x4::makeIdentity();

    // Set up the camera
    Vec3D vUp = {0, 1, 0};
    Vec3D vTarget = {0, 0, 1};

    // Rotate the camera by the yaw value
    Matrix4x4 matCameraRot = Matrix4x4::makeRotationY(fYaw);
    vLookDir = Crossover::multiplyVector(matCameraRot, vTarget);

    // Define the target position by adding the look direction to the camera position
    vTarget = Vec3D::add(vCamera, vLookDir);

    // Create the camera "point-at" matrix (from camera position to target, with up vector)
    Matrix4x4 matCamera = Crossover::pointAt(vCamera, vTarget, vUp);

    // Calculate the view matrix by inverting the camera matrix
    matView = Matrix4x4::quickInverse(matCamera);

    // Render the mesh using the world, view, and projection matrices
    Renderer::drawMesh(meshCube, matWorld, matView, matProj, vCamera, width, height);
}

// Function to handle the camera and input updates
void GraphicsEngine3D::update(float ms) {
    // Move forward in the direction of the look direction
    Vec3D vForward = Vec3D::mul(vLookDir, 0.5f);

    // Camera movement with arrow keys or W/A/S/D
    if (getKeyState(graphics::SCANCODE_UP)) vCamera.y += 1.0f;
    if (getKeyState(graphics::SCANCODE_DOWN)) vCamera.y -= 1.0f;
    if (getKeyState(graphics::SCANCODE_LEFT)) vCamera.x -= 1.0f;
    if (getKeyState(graphics::SCANCODE_RIGHT)) vCamera.x += 1.0f;
    if (getKeyState(graphics::SCANCODE_W)) vCamera = Vec3D::add(vCamera, vForward);
    if (getKeyState(graphics::SCANCODE_S)) vCamera = Vec3D::sub(vCamera, vForward);
    if (getKeyState(graphics::SCANCODE_A)) fYaw += 2.0f; // Rotate left
    if (getKeyState(graphics::SCANCODE_D)) fYaw -= 2.0f; // Rotate right
}
