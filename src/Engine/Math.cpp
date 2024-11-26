#include "Math.h"
#include <math.h>

Vec3D Vec3D::add(Vec3D &v1, Vec3D &v2) {
        return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
    }

    Vec3D Vec3D::sub(Vec3D &v1, Vec3D &v2) {
        return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
    }

    Vec3D Vec3D::mul(Vec3D &v1, float k) {
        return {v1.x * k, v1.y * k, v1.z * k};
    }

    Vec3D Vec3D::div(Vec3D &v1, float k) {
        return {v1.x / k, v1.y / k, v1.z / k};
    }

    float Vec3D::dotProduct(Vec3D &v1, Vec3D &v2) {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    float Vec3D::length(Vec3D &v) {
        return sqrtf(dotProduct(v, v));
    }

    Vec3D Vec3D::normalise(Vec3D &v) {
        float l = length(v);
        return {v.x / l, v.y / l, v.z / l};
    }

    Vec3D Vec3D::crossProduct(Vec3D &v1, Vec3D &v2) {
        Vec3D v;
        v.x = v1.y * v2.z - v1.z * v2.y;
        v.y = v1.z * v2.x - v1.x * v2.z;
        v.z = v1.x * v2.y - v1.y * v2.x;
        return v;
    }

    Vec3D Vec3D::intersectPlane(Vec3D &plane_p, Vec3D &plane_n, Vec3D &lineStart, Vec3D &lineEnd, float &t) {
        plane_n = normalise(plane_n);
        float plane_d = -dotProduct(plane_n, plane_p);
        float ad = dotProduct(lineStart, plane_n);
        float bd = dotProduct(lineEnd, plane_n);
        t = (-plane_d - ad) / (bd - ad);
        Vec3D lineStartToEnd = sub(lineEnd, lineStart);
        Vec3D lineToIntersect = mul(lineStartToEnd, t);
        return add(lineStart, lineToIntersect);
    }

    Vec3D Vec3D::calculateNormal(Vec3D &v1, Vec3D &v2, Vec3D &v3) {
        Vec3D edge1 = sub(v2, v1);
        Vec3D edge2 = sub(v3, v1);
        Vec3D normal = crossProduct(edge1, edge2); // Cross product of two edges
        return normalise(normal); // Normalize the result to get the unit normal
    }

    Matrix4x4 Matrix4x4::makeIdentity() {
        Matrix4x4 matrix;
        matrix.m[0][0] = 1.0f;
        matrix.m[1][1] = 1.0f;
        matrix.m[2][2] = 1.0f;
        matrix.m[3][3] = 1.0f;
        return matrix;
    }

    Matrix4x4 Matrix4x4::makeRotationX(float fAngleRad) {
        Matrix4x4 matrix;
        matrix.m[0][0] = 1.0f;
        matrix.m[1][1] = cosf(fAngleRad);
        matrix.m[1][2] = sinf(fAngleRad);
        matrix.m[2][1] = -sinf(fAngleRad);
        matrix.m[2][2] = cosf(fAngleRad);
        matrix.m[3][3] = 1.0f;
        return matrix;
    }

    Matrix4x4 Matrix4x4::makeRotationY(float fAngleRad) {
        Matrix4x4 matrix;
        matrix.m[0][0] = cosf(fAngleRad);
        matrix.m[0][2] = sinf(fAngleRad);
        matrix.m[2][0] = -sinf(fAngleRad);
        matrix.m[1][1] = 1.0f;
        matrix.m[2][2] = cosf(fAngleRad);
        matrix.m[3][3] = 1.0f;
        return matrix;
    }

    Matrix4x4 Matrix4x4::makeRotationZ(float fAngleRad) {
        Matrix4x4 matrix;
        matrix.m[0][0] = cosf(fAngleRad);
        matrix.m[0][1] = sinf(fAngleRad);
        matrix.m[1][0] = -sinf(fAngleRad);
        matrix.m[1][1] = cosf(fAngleRad);
        matrix.m[2][2] = 1.0f;
        matrix.m[3][3] = 1.0f;
        return matrix;
    }

    Matrix4x4 Matrix4x4::makeTranslation(float x, float y, float z) {
        Matrix4x4 matrix;
        matrix.m[0][0] = 1.0f;
        matrix.m[1][1] = 1.0f;
        matrix.m[2][2] = 1.0f;
        matrix.m[3][3] = 1.0f;
        matrix.m[3][0] = x;
        matrix.m[3][1] = y;
        matrix.m[3][2] = z;
        return matrix;
    }

    Matrix4x4 Matrix4x4::makeProjection(float fFovDegrees, float fAspectRatio, float fNear, float fFar) {
        float fFovRad = 1.0f / tanf(fFovDegrees * 0.5f / 180.0f * 3.14159f);
        Matrix4x4 matrix;
        matrix.m[0][0] = fAspectRatio * fFovRad;
        matrix.m[1][1] = fFovRad;
        matrix.m[2][2] = fFar / (fFar - fNear);
        matrix.m[3][2] = (-fFar * fNear) / (fFar - fNear);
        matrix.m[2][3] = 1.0f;
        matrix.m[3][3] = 0.0f;
        return matrix;
    }

    Matrix4x4 Matrix4x4::multiplyMatrix(Matrix4x4 &m1, Matrix4x4 &m2) {
        Matrix4x4 matrix;
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                matrix.m[r][c] = m1.m[r][0] * m2.m[0][c] + m1.m[r][1] * m2.m[1][c] + m1.m[r][2] * m2.m[2][c] + m1.m[r][
                                     3] * m2.m[3][c];
        return matrix;
    }

    Matrix4x4 Matrix4x4::quickInverse(Matrix4x4 &m) {// Only for Rotation/Translation Matrices

     Matrix4x4 matrix;
     matrix.m[0][0] = m.m[0][0];
     matrix.m[0][1] = m.m[1][0];
     matrix.m[0][2] = m.m[2][0];
     matrix.m[0][3] = 0.0f;
     matrix.m[1][0] = m.m[0][1];
     matrix.m[1][1] = m.m[1][1];
     matrix.m[1][2] = m.m[2][1];
     matrix.m[1][3] = 0.0f;
     matrix.m[2][0] = m.m[0][2];
     matrix.m[2][1] = m.m[1][2];
     matrix.m[2][2] = m.m[2][2];
     matrix.m[2][3] = 0.0f;
     matrix.m[3][0] = -(m.m[3][0] * matrix.m[0][0] + m.m[3][1] * matrix.m[1][0] + m.m[3][2] * matrix.m[2][0]);
     matrix.m[3][1] = -(m.m[3][0] * matrix.m[0][1] + m.m[3][1] * matrix.m[1][1] + m.m[3][2] * matrix.m[2][1]);
     matrix.m[3][2] = -(m.m[3][0] * matrix.m[0][2] + m.m[3][1] * matrix.m[1][2] + m.m[3][2] * matrix.m[2][2]);
     matrix.m[3][3] = 1.0f;
     return matrix;
    }

    Vec3D Crossover::multiplyVector(Matrix4x4 &m, Vec3D &i) {
        Vec3D v;
        v.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + i.w * m.m[3][0];
        v.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + i.w * m.m[3][1];
        v.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + i.w * m.m[3][2];
        v.w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + i.w * m.m[3][3];
        return v;
    }

    Matrix4x4 Crossover::pointAt(Vec3D &pos, Vec3D &target, Vec3D &up) {
        // Calculate new forward direction
        Vec3D newForward = Vec3D::sub(target, pos);
        newForward = Vec3D::normalise(newForward);

        // Calculate new Up direction
        Vec3D a = Vec3D::mul(newForward, Vec3D::dotProduct(up, newForward));
        Vec3D newUp = Vec3D::sub(up, a);
        newUp = Vec3D::normalise(newUp);

        // New Right direction is easy, its just cross product
        Vec3D newRight = Vec3D::crossProduct(newUp, newForward);

        // Construct Dimensioning and Translation Matrix
        Matrix4x4 matrix;
        matrix.m[0][0] = newRight.x;
        matrix.m[0][1] = newRight.y;
        matrix.m[0][2] = newRight.z;
        matrix.m[0][3] = 0.0f;
        matrix.m[1][0] = newUp.x;
        matrix.m[1][1] = newUp.y;
        matrix.m[1][2] = newUp.z;
        matrix.m[1][3] = 0.0f;
        matrix.m[2][0] = newForward.x;
        matrix.m[2][1] = newForward.y;
        matrix.m[2][2] = newForward.z;
        matrix.m[2][3] = 0.0f;
        matrix.m[3][0] = pos.x;
        matrix.m[3][1] = pos.y;
        matrix.m[3][2] = pos.z;
        matrix.m[3][3] = 1.0f;
        return matrix;
    }
    graphics::Brush Generic::getColour(float lum) {
        graphics::Brush c;
        int pixel_bw = (int) (13.0f * lum);
        switch (pixel_bw) {
            case 0:
                c.fill_color[0] = 0;
                c.fill_color[1] = 0;
                c.fill_color[2] = 0;
                c.outline_color[0] = 0;
                c.outline_color[1] = 0;
                c.outline_color[2] = 0;
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                c.fill_color[0] = 0.2f;
                c.fill_color[1] = 0.2f;
                c.fill_color[2] = 0.2f;
                c.outline_color[0] = 0.2f;
                c.outline_color[1] = 0.2f;
                c.outline_color[2] = 0.2f;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                c.fill_color[0] = 0.5f;
                c.fill_color[1] = 0.5f;
                c.fill_color[2] = 0.5f;
                c.outline_color[0] = 0.5f;
                c.outline_color[1] = 0.5f;
                c.outline_color[2] = 0.5f;
                break;
            case 9:
            case 10:
            case 11:
            case 12:
                c.fill_color[0] = 1.0f;
                c.fill_color[1] = 1.0f;
                c.fill_color[2] = 1.0f;
                c.outline_color[0] = 1.0f;
                c.outline_color[1] = 1.0f;
                c.outline_color[2] = 1.0f;
                break;
            default:
                c.fill_color[0] = 0;
                c.fill_color[1] = 0;
                c.fill_color[2] = 0;
                c.outline_color[0] = 0.0f;
                c.outline_color[1] = 0.0f;
                c.outline_color[2] = 0.0f;
        }
        return c;
    }
