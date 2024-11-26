#ifndef MESH_H
#define MESH_H

#include "Math.h"
#include <vector>
#include <string>

using namespace std;

struct Triangle {
    Vec3D vert[3];
    Vec3D normal[3];
    Vec2D tex[3];
    graphics::Brush color;

    static int clipAgainstPlane(Vec3D plane_p, Vec3D plane_n, Triangle &in_tri, Triangle &out_tri1, Triangle &out_tri2);

};

class Mesh {
public:
    std::vector<Triangle> tris;
    void loadFromObjectFile(const string filename, bool bHasTexture = false, bool bHasNormals = false);
};

#endif