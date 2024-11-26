#include <utility>
#include <vector>
#include <string>
#include <strstream>
#include <fstream>
#include <graphics.h>
#include <iostream>

using namespace std;
using namespace graphics;

namespace graphicsEngine3D {
    struct Vec2D {
        float u = 0;
        float v = 0;
        float w = 1;
    };

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
        static Vec3D intersectPlane(Vec3D &plane_p, Vec3D &plane_n, Vec3D &lineStart, Vec3D &lineEnd, float &t);
        static Vec3D calculateNormal(Vec3D &v1, Vec3D &v2, Vec3D &v3);
    };

    struct Triangle {
        Vec3D vert[3]; // Vertices (Array of 3 Vec3D for triangle vertices)
        Vec2D tex[3]; // Texture coordinates (Array of 3 Vec2D for each vertex)
        Vec3D norm[3]; // Normals (Array of 3 Vec3D for each vertex normal)

        // Empty constructor (initializes all to zero)
        Triangle() {
            for (int i = 0; i < 3; i++) {
                vert[i] = Vec3D{0.0f, 0.0f, 0.0f}; // Initialize vertices to (0, 0, 0)
                tex[i] = Vec2D{0.0f, 0.0f}; // Initialize texture coords to (0, 0)
                norm[i] = Vec3D{0.0f, 0.0f, 0.0f}; // Initialize normals to (0, 0, 0)
            }
        }

        // Constructor for vertices only (no texture or normals)
        Triangle(const Vec3D &v1, const Vec3D &v2, const Vec3D &v3) {
            vert[0] = v1;
            vert[1] = v2;
            vert[2] = v3;
        }

        // Constructor for vertices, texture coordinates, and normals
        Triangle(const Vec3D &v1, const Vec3D &v2, const Vec3D &v3,
                 const Vec2D &t1, const Vec2D &t2, const Vec2D &t3,
                 const Vec3D &n1, const Vec3D &n2, const Vec3D &n3) {
            vert[0] = v1;
            vert[1] = v2;
            vert[2] = v3; // Vertices
            tex[0] = t1;
            tex[1] = t2;
            tex[2] = t3; // Texture coordinates
            norm[0] = n1;
            norm[1] = n2;
            norm[2] = n3; // Normals
        }

        static int clipAgainstPlane(Vec3D plane_p, Vec3D plane_n, Triangle &in_tri, Triangle &out_tri1,
                                    Triangle &out_tri2);
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
        static Brush getColour(float lum);
    };

    struct Mesh {
        vector<Triangle> tris;

        void loadFromObjectFile(std::string sFilename, bool bHasTexture = false, bool bHasNormals = false) {
            vector<unsigned int> vertexIndices, uvIndices, normalIndices;
            vector<Vec3D> temp_vertices;
            vector<Vec2D> temp_uvs;
            vector<Vec3D> temp_normals;

            FILE *file = fopen(sFilename.c_str(), "r");
            if (file == NULL) {
                std::cerr << "Impossible to open the file!" << std::endl;
                return;
            }

            while (true) {
                char lineHeader[1024];
                int res = fscanf(file, "%s", lineHeader);
                if (res == EOF)
                    break;

                if (strcmp(lineHeader, "v") == 0) {
                    Vec3D vertex;
                    fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
                    temp_vertices.push_back(vertex);
                } else if (strcmp(lineHeader, "vt") == 0) {
                    Vec2D uv;
                    fscanf(file, "%f %f\n", &uv.u, &uv.v);
                    temp_uvs.push_back(uv);
                } else if (strcmp(lineHeader, "vn") == 0) {
                    Vec3D normal;
                    fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
                    temp_normals.push_back(normal);
                } else if (strcmp(lineHeader, "f") == 0) {
                    char buffer[1024];
                    fgets(buffer, 1024, file); // Read the full line for processing

                    unsigned int vertexIndex[3] = {0}, uvIndex[3] = {0}, normalIndex[3] = {0};
                    int matches = 0;

                    // Determine the format of the face line by analyzing its structure
                    if (strstr(buffer, "//")) {
                        // If "//" is present, it's v//vn format
                        matches = sscanf(buffer, "%d//%d %d//%d %d//%d",
                                         &vertexIndex[0], &normalIndex[0],
                                         &vertexIndex[1], &normalIndex[1],
                                         &vertexIndex[2], &normalIndex[2]);
                        if (matches != 6) {
                            std::cerr << "Error: Line format does not match v//vn.\n";
                            std::cerr << "Line: " << buffer << std::endl;
                            continue;
                        }
                        bHasNormals = true;
                    } else if (strchr(buffer, '/')) {
                        // If "/" is present, it's either v/vt or v/vt/vn
                        matches = sscanf(buffer, "%d/%d/%d %d/%d/%d %d/%d/%d",
                                         &vertexIndex[0], &uvIndex[0], &normalIndex[0],
                                         &vertexIndex[1], &uvIndex[1], &normalIndex[1],
                                         &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
                        if (matches == 9) {
                            bHasTexture = true;
                            bHasNormals = true;
                        } else {
                            matches = sscanf(buffer, "%d/%d %d/%d %d/%d",
                                             &vertexIndex[0], &uvIndex[0],
                                             &vertexIndex[1], &uvIndex[1],
                                             &vertexIndex[2], &uvIndex[2]);
                            if (matches != 6) {
                                std::cerr << "Error: Line format does not match v/vt.\n";
                                std::cerr << "Line: " << buffer << std::endl;
                                continue;
                            }
                            bHasTexture = true;
                        }
                    } else {
                        // If no "/" is present, it's v format
                        matches = sscanf(buffer, "%d %d %d",
                                         &vertexIndex[0],
                                         &vertexIndex[1],
                                         &vertexIndex[2]);
                        if (matches != 3) {
                            std::cerr << "Error: Line format does not match v.\n";
                            std::cerr << "Line: " << buffer << std::endl;
                            continue;
                        }
                    }

                    // Store indices
                    for (int i = 0; i < 3; ++i) {
                        vertexIndices.push_back(vertexIndex[i]);
                        if (bHasTexture) uvIndices.push_back(uvIndex[i]);
                        if (bHasNormals) normalIndices.push_back(normalIndex[i]);
                    }
                }
            }

            fclose(file);

            for (size_t i = 0; i < vertexIndices.size(); i += 3) {
                Triangle tri;

                // Vertices
                tri.vert[0] = temp_vertices[vertexIndices[i] - 1];
                tri.vert[1] = temp_vertices[vertexIndices[i + 1] - 1];
                tri.vert[2] = temp_vertices[vertexIndices[i + 2] - 1];

                // Texture coordinates (if present)
                if (bHasTexture) {
                    tri.tex[0] = temp_uvs[uvIndices[i] - 1];
                    tri.tex[1] = temp_uvs[uvIndices[i + 1] - 1];
                    tri.tex[2] = temp_uvs[uvIndices[i + 2] - 1];
                }

                // Normals (if present)
                if (bHasNormals) {
                    tri.norm[0] = temp_normals[normalIndices[i] - 1];
                    tri.norm[1] = temp_normals[normalIndices[i + 1] - 1];
                    tri.norm[2] = temp_normals[normalIndices[i + 2] - 1];
                }

                tris.push_back(tri);
            }

            // Output or process `triangles` as needed
            std::cout << "Loaded " << tris.size() << " triangles from " << sFilename << std::endl;
        }
    };
}
