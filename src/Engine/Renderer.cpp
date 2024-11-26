#include "renderer.h"
#include <algorithm>
#include <list>

using namespace std;

void Renderer::drawMesh(Mesh &mesh, Matrix4x4 &matWorld, Matrix4x4 &matView,  Matrix4x4 &matProj, Vec3D &vCamera, int width, int height) {
    std::vector<Triangle> vecTrianglesToRaster;

    // Draw Triangles
    for (auto &tri : mesh.tris) {
        Triangle triProjected, triTransformed, triViewed;

        // World Matrix Transform (Position the triangle in the world)
        triTransformed.vert[0] = Crossover::multiplyVector(matWorld, tri.vert[0]);
        triTransformed.vert[1] = Crossover::multiplyVector(matWorld, tri.vert[1]);
        triTransformed.vert[2] = Crossover::multiplyVector(matWorld, tri.vert[2]);

        // Transform normals too
        triTransformed.normal[0] = Crossover::multiplyVector(matWorld, tri.normal[0]);
        triTransformed.normal[1] = Crossover::multiplyVector(matWorld, tri.normal[1]);
        triTransformed.normal[2] = Crossover::multiplyVector(matWorld, tri.normal[2]);

        // Texture coordinates remain the same
        triTransformed.tex[0] = tri.tex[0];
        triTransformed.tex[1] = tri.tex[1];
        triTransformed.tex[2] = tri.tex[2];

        // Calculate Triangle Normal (for back-face culling)
        Vec3D normal, line1, line2;

        line1 = Vec3D::sub(triTransformed.vert[1], triTransformed.vert[0]);
        line2 = Vec3D::sub(triTransformed.vert[2], triTransformed.vert[0]);

        Vec3D crossProduct = Vec3D::crossProduct(line1, line2);
        normal = Vec3D::normalise(crossProduct);

        Vec3D vCameraRay = Vec3D::sub(triTransformed.vert[0], vCamera);

        // Only render the triangle if it's facing the camera (back-face culling)
        if (Vec3D::dotProduct(normal, vCameraRay) < 0.0f) {
            // Calculate lighting (simple diffuse lighting)
            Vec3D lightDirection = {0.0f, 1.0f, 1.0f};
            lightDirection = Vec3D::normalise(lightDirection);
            float dp = max(0.1f, Vec3D::dotProduct(lightDirection, normal));
            triTransformed.color = Generic::getColour(dp);

            // Convert World Space --> View Space
            triViewed.vert[0] = Crossover::multiplyVector(matView, triTransformed.vert[0]);
            triViewed.vert[1] = Crossover::multiplyVector(matView, triTransformed.vert[1]);
            triViewed.vert[2] = Crossover::multiplyVector(matView, triTransformed.vert[2]);
            triViewed.color = triTransformed.color;
            triViewed.tex[0] = triTransformed.tex[0];
            triViewed.tex[1] = triTransformed.tex[1];
            triViewed.tex[2] = triTransformed.tex[2];

            // Clip the triangle against the near plane (Z > 0.1)
            Triangle clipped[2];
            int nClippedTriangles = Triangle::clipAgainstPlane({0.0f, 0.0f, 0.1f}, {0.0f, 0.0f, 1.0f}, triViewed, clipped[0], clipped[1]);

            // Project triangles from 3D to 2D
            for (int n = 0; n < nClippedTriangles; n++) {
                triProjected.vert[0] = Crossover::multiplyVector(matProj, clipped[n].vert[0]);
                triProjected.vert[1] = Crossover::multiplyVector(matProj, clipped[n].vert[1]);
                triProjected.vert[2] = Crossover::multiplyVector(matProj, clipped[n].vert[2]);

                triProjected.color = clipped[n].color;
                triProjected.tex[0] = clipped[n].tex[0];
                triProjected.tex[1] = clipped[n].tex[1];
                triProjected.tex[2] = clipped[n].tex[2];

                // Normalize texture coordinates by depth (w)
                for (int i = 0; i < 3; i++) {
                    triProjected.tex[i].u /= triProjected.vert[i].w;
                    triProjected.tex[i].v /= triProjected.vert[i].w;
                    triProjected.tex[i].w = 1.0f / triProjected.vert[i].w;

                    // Scale vertices into normalized space
                    triProjected.vert[i] = Vec3D::div(triProjected.vert[i], triProjected.vert[i].w);
                    triProjected.vert[i].x *= -1.0f;
                    triProjected.vert[i].y *= -1.0f;

                    // Offset and scale into screen space
                    Vec3D vOffsetView = {1, 1, 0};
                    triProjected.vert[i] = Vec3D::add(triProjected.vert[i], vOffsetView);
                    triProjected.vert[i].x *= 0.5f * (float)width;
                    triProjected.vert[i].y *= 0.5f * (float)height;
                }

                vecTrianglesToRaster.push_back(triProjected);
            }
        }
    }

    // Sort triangles from back to front based on average depth (Z)
    sort(vecTrianglesToRaster.begin(), vecTrianglesToRaster.end(), [](Triangle &t1, Triangle &t2) {
        float z1 = (t1.vert[0].z + t1.vert[1].z + t1.vert[2].z) / 3.0f;
        float z2 = (t2.vert[0].z + t2.vert[1].z + t2.vert[2].z) / 3.0f;
        return z1 > z2;
    });

    // Rasterize triangles
    for (auto &triToRaster : vecTrianglesToRaster) {
        Triangle clipped[2];
        std::list<Triangle> listTriangles;

        // Add the initial triangle
        listTriangles.push_back(triToRaster);
        int nNewTriangles = 1;

        // Clip triangles against the screen edges
        for (int p = 0; p < 4; p++) {
            int nTrisToAdd = 0;
            while (nNewTriangles > 0) {
                Triangle test = listTriangles.front();
                listTriangles.pop_front();
                nNewTriangles--;

                switch (p) {
                    case 0: nTrisToAdd = Triangle::clipAgainstPlane({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, test, clipped[0], clipped[1]); break;
                    case 1: nTrisToAdd = Triangle::clipAgainstPlane({0.0f, (float)height - 1, 0.0f}, {0.0f, -1.0f, 0.0f}, test, clipped[0], clipped[1]); break;
                    case 2: nTrisToAdd = Triangle::clipAgainstPlane({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, test, clipped[0], clipped[1]); break;
                    case 3: nTrisToAdd = Triangle::clipAgainstPlane({(float)width - 1, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, test, clipped[0], clipped[1]); break;
                }

                for (int w = 0; w < nTrisToAdd; w++)
                    listTriangles.push_back(clipped[w]);
            }
            nNewTriangles = listTriangles.size();
        }

        // Draw the final clipped triangles
        for (auto &t : listTriangles) {
            drawTriangle(t.vert[0].x, t.vert[0].y, t.vert[1].x, t.vert[1].y, t.vert[2].x, t.vert[2].y, t.color);
        }
    }
}
