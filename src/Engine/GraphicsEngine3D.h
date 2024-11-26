#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include "mesh.h"
#include "renderer.h"
#include "math.h"

class GraphicsEngine3D {
public:
    GraphicsEngine3D();
    void createWindow(int width, int height, std::string windowName);
    void draw();
    void update(float ms);

private:
    Mesh meshCube;
    Vec3D vCamera;     // Camera position
    Vec3D vLookDir;    // Camera look direction
    Matrix4x4 matProj; // Projection matrix
    Matrix4x4 matView; // View matrix
    float fYaw;        // Yaw (horizontal rotation)
    float fTheta;      // Spin world transform
    int width, height; // Window dimensions
};

#endif // GRAPHICS_ENGINE_H
