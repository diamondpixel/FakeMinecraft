#include "mesh.h"
#include <fstream>
#include <iostream>

using namespace std;

int Triangle::clipAgainstPlane(Vec3D plane_p, Vec3D plane_n, Triangle &in_tri, Triangle &out_tri1, Triangle &out_tri2) {
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

void Mesh::loadFromObjectFile(std::string sFilename, bool bHasTexture, bool bHasNormals) {
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
                    tri.normal[0] = temp_normals[normalIndices[i] - 1];
                    tri.normal[1] = temp_normals[normalIndices[i + 1] - 1];
                    tri.normal[2] = temp_normals[normalIndices[i + 2] - 1];
                }

                tris.push_back(tri);
            }

            // Output or process `triangles` as needed
            std::cout << "Loaded " << tris.size() << " triangles from " << sFilename << std::endl;
        }