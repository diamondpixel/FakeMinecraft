#include <vector>
#include <string>
#include <strstream>
#include <fstream>
#include <graphics.h>

using namespace std;
using namespace graphics;

namespace graphicsEngine3D {
struct Vec3D {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;

    static Vec3D add(Vec3D &v1, Vec3D &v2);
    static Vec3D sub(Vec3D &v1, Vec3D &v2);
    static Vec3D mul(Vec3D &v1, float k);
    static Vec3D div(Vec3D &v1, float k);
    static float dotProduct(Vec3D &v1, Vec3D &v2);
    static float length(Vec3D &v);
    static Vec3D normalise(Vec3D &v);
    static Vec3D crossProduct(Vec3D &v1, Vec3D &v2);
    static Vec3D intersectPlane(Vec3D &plane_p, Vec3D &plane_n, Vec3D &lineStart, Vec3D &lineEnd);
};

struct Triangle {
    Vec3D p[3];
    Brush color;
    static int clipAgainstPlane(Vec3D plane_p, Vec3D plane_n, Triangle &in_tri, Triangle &out_tri1,Triangle &out_tri2);
};

struct Matrix4x4 {
    float m[4][4] = {0};

    static Matrix4x4 makeIdentity();
    static Matrix4x4 makeRotationX(float fAngleRad);
    static Matrix4x4 makeRotationY(float fAngleRad);
    static Matrix4x4 makeRotationZ(float fAngleRad);
    static Matrix4x4 makeTranslation(float x, float y, float z);
    static Matrix4x4 makeProjection(float fFovDegrees, float fAspectRatio, float fNear, float fFar);
    static Matrix4x4 multiplyMatrix(Matrix4x4 &m1, Matrix4x4 &m2);
    static Matrix4x4 quickInverse(Matrix4x4 &m);

};

struct Crossover {
    static Brush getColour(float lum);
    static Vec3D multiplyVector(Matrix4x4 &m, Vec3D &i);
    static Matrix4x4 pointAt(Vec3D &pos, Vec3D &target, Vec3D &up);
};


struct Mesh {
    vector<Triangle> tris;
    bool LoadFromObjectFile(std::string sFilename) {
        ifstream f(sFilename);
        if (!f.is_open())
            return false;

        // Local cache of verts
        vector<Vec3D> verts;

        while (!f.eof()) {
            char line[128];
            f.getline(line, 128);

            strstream s;
            s << line;

            char junk;

            if (line[0] == 'v') {
                Vec3D v;
                s >> junk >> v.x >> v.y >> v.z;
                verts.push_back(v);
            }

            if (line[0] == 'f') {
                int f[3];
                s >> junk >> f[0] >> f[1] >> f[2];
                tris.push_back({verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1]});
            }
        }
        return true;
    }
  };
}