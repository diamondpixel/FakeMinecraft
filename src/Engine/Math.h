#ifndef MATH_H
#define MATH_H

#include "graphics.h"

struct Vec2D {
    float u, v;
    float w = 1;
};


struct Vec3D {
    float x, y, z;
    float w = 1;

    static Vec3D add(Vec3D &v1, Vec3D &v2);
    static Vec3D sub(Vec3D &v1, Vec3D &v2);
    static Vec3D mul(Vec3D &v1, float k);
    static Vec3D div(Vec3D &v1, float k);
    static float dotProduct(Vec3D &v1, Vec3D &v2);
    static float length(Vec3D &v);
    static Vec3D normalise(Vec3D &v);
    static Vec3D crossProduct(Vec3D &v1, Vec3D &v2);
    static Vec3D intersectPlane(Vec3D &plane_p, Vec3D &plane_n, Vec3D &lineStart, Vec3D &lineEnd, float &t);
    static Vec3D calculateNormal(Vec3D &v1, Vec3D &v2, Vec3D &v3);

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
    static Vec3D multiplyVector(Matrix4x4 &m, Vec3D &i);
    static Matrix4x4 pointAt(Vec3D &pos, Vec3D &target, Vec3D &up);
};

struct Generic {
    static graphics::Brush getColour(float lum);
};

#endif