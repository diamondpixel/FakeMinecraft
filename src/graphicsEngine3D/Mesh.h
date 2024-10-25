#pragma once
#include <vector>
#include "Triangle.h"
using namespace std;

namespace graphicsEngine3D {
    struct Mesh {
        vector<Triangle> tris;
    };
}
