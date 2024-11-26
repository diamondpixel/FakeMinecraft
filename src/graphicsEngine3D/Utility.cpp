#include "Utility.h"
#include <graphics.h>

using namespace graphics;

namespace graphicsEngine3D {
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

    Vec3D Crossover::multiplyVector(Matrix4x4 &m, Vec3D &i) {
        Vec3D v;
        v.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + i.w * m.m[3][0];
        v.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + i.w * m.m[3][1];
        v.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + i.w * m.m[3][2];
        v.w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + i.w * m.m[3][3];
        return v;
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

    Matrix4x4 Matrix4x4::quickInverse(Matrix4x4 &m) // Only for Rotation/Translation Matrices
    {
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

    int Triangle::clipAgainstPlane(Vec3D plane_p, Vec3D plane_n, Triangle &in_tri, Triangle &out_tri1,
                                   Triangle &out_tri2) {
        // Make sure plane normal is indeed normal
        plane_n = Vec3D::normalise(plane_n);

        // Return signed shortest distance from point to plane, plane normal must be normalised
        auto dist = [&](Vec3D &p) {
            Vec3D n = Vec3D::normalise(p);
            return (plane_n.x * p.x + plane_n.y * p.y + plane_n.z * p.z - Vec3D::dotProduct(plane_n, plane_p));
        };

        // Create two temporary storage arrays to classify points either side of plane
        // If distance sign is positive, point lies on "inside" of plane
        Vec3D *inside_points[3];
        int nOutsidePointCount = 0;
        Vec3D *outside_points[3];
        int nInsidePointCount = 0;
        Vec2D *inside_tex[3];
        int nInsideTexCount = 0;
        Vec2D *outside_tex[3];
        int nOutsideTexCount = 0;


        // Get signed distance of each point in Triangle to plane
        float d0 = dist(in_tri.vert[0]);
        float d1 = dist(in_tri.vert[1]);
        float d2 = dist(in_tri.vert[2]);

        if (d0 >= 0) {
            inside_points[nInsidePointCount++] = &in_tri.vert[0];
            inside_tex[nInsideTexCount++] = &in_tri.tex[0];
        } else {
            outside_points[nOutsidePointCount++] = &in_tri.vert[0];
            outside_tex[nOutsideTexCount++] = &in_tri.tex[0];
        }
        if (d1 >= 0) {
            inside_points[nInsidePointCount++] = &in_tri.vert[1];
            inside_tex[nInsideTexCount++] = &in_tri.tex[1];
        } else {
            outside_points[nOutsidePointCount++] = &in_tri.vert[1];
            outside_tex[nOutsideTexCount++] = &in_tri.tex[1];
        }
        if (d2 >= 0) {
            inside_points[nInsidePointCount++] = &in_tri.vert[2];
            inside_tex[nInsideTexCount++] = &in_tri.tex[2];
        } else {
            outside_points[nOutsidePointCount++] = &in_tri.vert[2];
            outside_tex[nOutsideTexCount++] = &in_tri.tex[2];
        }


        // Now classify Triangle points, and break the input Triangle into
        // smaller output Triangles if required. There are four possible
        // outcomes...

        if (nInsidePointCount == 0) {
            // All points lie on the outside of plane, so clip whole Triangle
            // It ceases to exist

            return 0; // No returned Triangles are valid
        }

        if (nInsidePointCount == 3) {
            // All points lie on the inside of plane, so do nothing
            // and allow the Triangle to simply pass through
            out_tri1 = in_tri;

            return 1; // Just the one returned original Triangle is valid
        }

        if (nInsidePointCount == 1 && nOutsidePointCount == 2) {
            // Triangle should be clipped. As two points lie outside
            // the plane, the triangle simply becomes a smaller triangle

            // Copy appearance info to new triangle
            //out_tri1.color = in_tri.color;

            // The inside point is valid, so keep that...
            out_tri1.vert[0] = *inside_points[0];
            out_tri1.tex[0] = *inside_tex[0];

            // but the two new points are at the locations where the
            // original sides of the triangle (lines) intersect with the plane
            float t;
            out_tri1.vert[1] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0], t);
            out_tri1.tex[1].u = t * (outside_tex[0]->u - inside_tex[0]->u) + inside_tex[0]->u;
            out_tri1.tex[1].v = t * (outside_tex[0]->v - inside_tex[0]->v) + inside_tex[0]->v;
            out_tri1.tex[1].w = t * (outside_tex[0]->w - inside_tex[0]->w) + inside_tex[0]->w;

            out_tri1.vert[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[1], t);
            out_tri1.tex[2].u = t * (outside_tex[1]->u - inside_tex[0]->u) + inside_tex[0]->u;
            out_tri1.tex[2].v = t * (outside_tex[1]->v - inside_tex[0]->v) + inside_tex[0]->v;
            out_tri1.tex[2].w = t * (outside_tex[1]->w - inside_tex[0]->w) + inside_tex[0]->w;

            return 1; // Return the newly formed single triangle
        }


        if (nInsidePointCount == 2 && nOutsidePointCount == 1) {
            // Triangle should be clipped. As two points lie inside the plane,
            // the clipped triangle becomes a "quad". Fortunately, we can
            // represent a quad with two new triangles

            // Copy appearance info to new triangles
            //out_tri1.color = in_tri.color;
            //out_tri2.color = in_tri.color;

            // The first triangle consists of the two inside points and a new
            // point determined by the location where one side of the triangle
            // intersects with the plane
            out_tri1.vert[0] = *inside_points[0];
            out_tri1.vert[1] = *inside_points[1];
            out_tri1.tex[0] = *inside_tex[0];
            out_tri1.tex[1] = *inside_tex[1];

            float t;
            out_tri1.vert[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0], t);
            out_tri1.tex[2].u = t * (outside_tex[0]->u - inside_tex[0]->u) + inside_tex[0]->u;
            out_tri1.tex[2].v = t * (outside_tex[0]->v - inside_tex[0]->v) + inside_tex[0]->v;
            out_tri1.tex[2].w = t * (outside_tex[0]->w - inside_tex[0]->w) + inside_tex[0]->w;

            // The second triangle is composed of one of he inside points, a
            // new point determined by the intersection of the other side of the
            // triangle and the plane, and the newly created point above
            out_tri2.vert[0] = *inside_points[1];
            out_tri2.tex[0] = *inside_tex[1];
            out_tri2.vert[1] = out_tri1.vert[2];
            out_tri2.tex[1] = out_tri1.tex[2];
            out_tri2.vert[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[1], *outside_points[0], t);
            out_tri2.tex[2].u = t * (outside_tex[0]->u - inside_tex[1]->u) + inside_tex[1]->u;
            out_tri2.tex[2].v = t * (outside_tex[0]->v - inside_tex[1]->v) + inside_tex[1]->v;
            out_tri2.tex[2].w = t * (outside_tex[0]->w - inside_tex[1]->w) + inside_tex[1]->w;
            return 2; // Return two newly formed triangles which form a quad
        }
    }

    Brush Generic::getColour(float lum) {
        Brush c;
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
}
