#include "Utility.h"

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

     Vec3D Vec3D::intersectPlane(Vec3D &plane_p, Vec3D &plane_n, Vec3D &lineStart, Vec3D &lineEnd) {
      plane_n = normalise(plane_n);
      float plane_d = -dotProduct(plane_n, plane_p);
      float ad = dotProduct(lineStart, plane_n);
      float bd = dotProduct(lineEnd, plane_n);
      float t = (-plane_d - ad) / (bd - ad);
      Vec3D lineStartToEnd = sub(lineEnd, lineStart);
      Vec3D lineToIntersect = mul(lineStartToEnd, t);
      return add(lineStart, lineToIntersect);
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
        int nInsidePointCount = 0;
        Vec3D *outside_points[3];
        int nOutsidePointCount = 0;

        // Get signed distance of each point in Triangle to plane
        float d0 = dist(in_tri.p[0]);
        float d1 = dist(in_tri.p[1]);
        float d2 = dist(in_tri.p[2]);

        if (d0 >= 0) { inside_points[nInsidePointCount++] = &in_tri.p[0]; } else {
            outside_points[nOutsidePointCount++] = &in_tri.p[0];
        }
        if (d1 >= 0) { inside_points[nInsidePointCount++] = &in_tri.p[1]; } else {
            outside_points[nOutsidePointCount++] = &in_tri.p[1];
        }
        if (d2 >= 0) { inside_points[nInsidePointCount++] = &in_tri.p[2]; } else {
            outside_points[nOutsidePointCount++] = &in_tri.p[2];
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
            // the plane, the Triangle simply becomes a smaller Triangle

            // Copy appearance info to new Triangle
            out_tri1.col = in_tri.col;
            out_tri1.sym = in_tri.sym;

            // The inside point is valid, so keep that...
            out_tri1.p[0] = *inside_points[0];

            // but the two new points are at the locations where the
            // original sides of the Triangle (lines) intersect with the plane
            out_tri1.p[1] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);
            out_tri1.p[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[1]);

            return 1; // Return the newly formed single Triangle
        }

        if (nInsidePointCount == 2 && nOutsidePointCount == 1) {
            // Triangle should be clipped. As two points lie inside the plane,
            // the clipped Triangle becomes a "quad". Fortunately, we can
            // represent a quad with two new Triangles

            // Copy appearance info to new Triangles
            out_tri1.col = in_tri.col;
            out_tri1.sym = in_tri.sym;

            out_tri2.col = in_tri.col;
            out_tri2.sym = in_tri.sym;

            // The first Triangle consists of the two inside points and a new
            // point determined by the location where one side of the Triangle
            // intersects with the plane
            out_tri1.p[0] = *inside_points[0];
            out_tri1.p[1] = *inside_points[1];
            out_tri1.p[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);

            // The second Triangle is composed of one of he inside points, a
            // new point determined by the intersection of the other side of the
            // Triangle and the plane, and the newly created point above
            out_tri2.p[0] = *inside_points[1];
            out_tri2.p[1] = out_tri1.p[2];
            out_tri2.p[2] = Vec3D::intersectPlane(plane_p, plane_n, *inside_points[1], *outside_points[0]);

            return 2; // Return two newly formed Triangles which form a quad
        }
    }
}