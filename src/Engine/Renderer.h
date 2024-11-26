#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"
#include "Math.h"
#include "graphics.h"

class Renderer {
public:
    static void drawMesh(Mesh &mesh, Matrix4x4 &world, Matrix4x4 &view, Matrix4x4 &proj,Vec3D &vCamera, int width, int height);
};

#endif