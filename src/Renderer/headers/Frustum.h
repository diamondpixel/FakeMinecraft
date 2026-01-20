#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cmath>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86_FP)
    #include <immintrin.h>
    #define FRUSTUM_USE_SIMD 1
#endif

// High-performance frustum culling with SIMD optimization
class Frustum {
public:
    // Update frustum planes from view-projection matrix
    void update(const glm::mat4 &vp) {
        // Extract planes using Gribb/Hartmann method
        // Store in SoA layout for SIMD processing

        // Left, Right, Bottom, Top, Near, Far
        const glm::vec4 rawPlanes[6] = {
            glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]),
            glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]),
            glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]),
            glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]),
            glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]),
            glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2])
        };

        // Normalize and convert to SoA layout
        for (int i = 0; i < 6; ++i) {
            float invLen = 1.0f / glm::length(glm::vec3(rawPlanes[i]));
            planesX[i] = rawPlanes[i].x * invLen;
            planesY[i] = rawPlanes[i].y * invLen;
            planesZ[i] = rawPlanes[i].z * invLen;
            planesW[i] = rawPlanes[i].w * invLen;
        }
    }

    // SIMD-optimized visibility test
    [[nodiscard]] inline bool isBoxVisible(const glm::vec3 &center, const glm::vec3 &extents) const {
#ifdef FRUSTUM_USE_SIMD
        // Load center and extents into SIMD registers
        const __m128 cx = _mm_set1_ps(center.x);
        const __m128 cy = _mm_set1_ps(center.y);
        const __m128 cz = _mm_set1_ps(center.z);
        const __m128 ex = _mm_set1_ps(extents.x);
        const __m128 ey = _mm_set1_ps(extents.y);
        const __m128 ez = _mm_set1_ps(extents.z);

        // Process 4 planes at once (Indices 0-3)
        // Explicitly unrolled or limited to safe count to avoid reading past valid data (planes 4-5 are handled scalar)
        for (int i = 0; i < 4; i += 4) {
            // Load plane data (4 planes)
            __m128 px = _mm_loadu_ps(&planesX[i]);
            __m128 py = _mm_loadu_ps(&planesY[i]);
            __m128 pz = _mm_loadu_ps(&planesZ[i]);
            __m128 pw = _mm_loadu_ps(&planesW[i]);

            // Compute radius: r = dot(extents, abs(plane.xyz))
            // Use max(x, -x) for absolute value
            __m128 abs_px = _mm_max_ps(px, _mm_sub_ps(_mm_setzero_ps(), px));
            __m128 abs_py = _mm_max_ps(py, _mm_sub_ps(_mm_setzero_ps(), py));
            __m128 abs_pz = _mm_max_ps(pz, _mm_sub_ps(_mm_setzero_ps(), pz));

            __m128 r = _mm_mul_ps(ex, abs_px);
            r = _mm_add_ps(r, _mm_mul_ps(ey, abs_py));
            r = _mm_add_ps(r, _mm_mul_ps(ez, abs_pz));

            // Compute distance: d = dot(center, plane.xyz) + plane.w
            __m128 d = _mm_mul_ps(cx, px);
            d = _mm_add_ps(d, _mm_mul_ps(cy, py));
            d = _mm_add_ps(d, _mm_mul_ps(cz, pz));
            d = _mm_add_ps(d, pw);

            // Check if d < -r for any plane
            __m128 neg_r = _mm_sub_ps(_mm_setzero_ps(), r);
            __m128 cmp = _mm_cmplt_ps(d, neg_r);

            // If any plane rejects, return false
            if (_mm_movemask_ps(cmp) != 0) {
                return false;
            }
        }

        // Handle remaining planes (5th and 6th)
        for (int i = 4; i < 6; ++i) {
            const float r = extents.x * std::abs(planesX[i]) +
                           extents.y * std::abs(planesY[i]) +
                           extents.z * std::abs(planesZ[i]);
            const float d = center.x * planesX[i] +
                           center.y * planesY[i] +
                           center.z * planesZ[i] + planesW[i];
            if (d < -r) return false;
        }

        return true;
#else
        // Scalar fallback
        for (int i = 0; i < 6; ++i) {
            const float r = extents.x * std::abs(planesX[i]) +
                           extents.y * std::abs(planesY[i]) +
                           extents.z * std::abs(planesZ[i]);
            const float d = center.x * planesX[i] +
                           center.y * planesY[i] +
                           center.z * planesZ[i] + planesW[i];
            if (d < -r) return false;
        }
        return true;
#endif
    }

private:
    // SoA layout for SIMD efficiency (better cache locality)
    alignas(16) float planesX[6] = {};
    alignas(16) float planesY[6] = {};
    alignas(16) float planesZ[6] = {};
    alignas(16) float planesW[6] = {};
};