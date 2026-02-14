/**
 * @file Planet.h
 * @brief World management system for chunked voxel data.
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <queue>
#include <glm/glm.hpp>
#include <thread>
#include <shared_mutex>
#include <atomic>
#include <memory_resource>

#include "ChunkPos.h"
#include "ChunkData.h"
#include "Chunk.h"
#include "ChunkPosHash.h"
#include "Frustum.h"
#include "ThreadPool.h"

/**
 * @enum OcclusionMethod
 * @brief Strategy for processing Hardware Occlusion Query results.
 */
enum class OcclusionMethod : uint8_t {
    VOTING,  ///< Discrete voting: requires N consecutive failures to hide.
    EMA      ///< Exponential Moving Average: smooth visibility interpolation.
};

/**
 * @class Planet
 * @brief The central coordinator for world generation, chunk management, and rendering.
 * 
 * Planet handles the lifecycle of voxel data, including background generation threads,
 * frustum culling, hardware occlusion queries, and parallel mesh reconstruction.
 * It uses a shared-mutex and atomic-based concurrency model to allow high-performance
 * background updates while minimizing render-thread stalls.
 */
class Planet
{
public:
    /**
     * @brief Constructor. Initializes shaders and reserves memory for chunk lists.
     */
    Planet(Shader* solidShader, Shader* waterShader, Shader* billboardShader, Shader* bboxShader);
    
    /**
     * @brief Destructor. Stops background threads and cleans up GPU resources.
     */
    ~Planet();

    /**
     * @brief The main per-frame logic.
     * 
     * Handles chunk loading/unloading, mesh uploads, and visibility sorting.
     * @param cameraPos Current world-space position of the player.
     * @param updateOcclusion If true, issues new hardware occlusion queries.
     */
    void update(const glm::vec3& cameraPos, bool updateOcclusion = true);

    /**
     * @brief Updates the frustum state for culling.
     * @param frustumVP The view-projection matrix used for culling (can be frozen).
     * @param renderingVP The current view-projection matrix used for rendering.
     */
    void updateFrustum(const glm::mat4& frustumVP, const glm::mat4& renderingVP);

    /**
     * @brief Retrieves a chunk at the given position.
     * @return Pointer to the chunk, or nullptr if not loaded.
     */
    Chunk* getChunk(ChunkPos chunkPos);

    /**
     * @brief Forces a rebuild of the chunk loading queue next frame.
     */
    void clearChunkQueue();

private:
    /**
     * @brief Entry point for the background world-processing thread.
     */
    void chunkThreadUpdate();

    /** @name Internal Visibility Helpers @{ */
    [[nodiscard]] inline bool isOutOfRenderDistance(const ChunkPos& pos) const noexcept;
    [[nodiscard]] static std::pair<glm::vec3, glm::vec3> getChunkBounds(const ChunkPos& pos) noexcept;
    void cleanupUnusedChunkData();
    /** @} */

    /** @name Queue Logic @{ */
    void updateChunkQueue();
    void buildChunkQueue();
    void addChunkRing(int radius);
    inline void addChunkEdges(int radius, int edge, int y) noexcept;
    inline void addChunkCorners(int radius, int y) noexcept;
    inline void addChunkIfMissing(int x, int y, int z);
    /** @} */

    /** @name Background Tasks @{ */
    void processChunkDataQueue();
    void processChunkQueue();
    void processRegenQueue();
    bool populateChunkData(Chunk* chunk, const ChunkPos& chunkPos);
    std::shared_ptr<ChunkData> getOrCreateChunkData(const ChunkPos& pos);
    /** @} */

public:
    static Planet* planet; ///< Singleton-like access point for global queries.

    /** @name Render Statistics @{ */
    std::atomic<unsigned int> numChunks{0};         ///< Total chunks currently tracking in memory.
    std::atomic<unsigned int> numChunksRendered{0}; ///< Chunks that passed culling this frame.
    /** @} */

    int renderDistance = 30; ///< Radius of loaded chunks around the player.
    int renderHeight = 2;   ///< Vertical extent of chunk loading.

private:
    /** @name Data Structures
     * Thread-safe maps for managing chunk lifecycle.
     * @{
     */
    std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> chunks;         ///< Master map of active chunks.
    std::vector<Chunk*> chunkList;                                    ///< Contiguous list for fast iteration.
    std::unordered_map<ChunkPos, std::shared_ptr<ChunkData>, ChunkPosHash> chunkData; ///< Shared voxel data buffers.
    /** @} */

    /** @name Queues
     * Work-queues processed by background threads.
     * @{
     */
    std::queue<ChunkPos> chunkQueue;     ///< Positions awaiting object allocation.
    std::queue<ChunkPos> chunkDataQueue; ///< Positions awaiting noise generation.
    std::queue<ChunkPos> regenQueue;    ///< Chunks requiring mesh reconstruction.
    std::atomic<unsigned int> chunksLoading{0}; ///< Current count of background tasks.
    /** @} */

    /** @name Synchronization & State @{ */
    std::atomic<int> lastCamX{-100}, lastCamY{-100}, lastCamZ{-100};
    std::atomic<int> camChunkX{-100}, camChunkY{-100}, camChunkZ{-100};
    std::atomic<long> last_seed{0};

    Shader* solidShader;
    Shader* waterShader;
    Shader* billboardShader;
    Shader* bboxShader;

    std::thread chunkThread;                   ///< Background world-worker.
    mutable std::shared_mutex chunkMutex;      ///< Mutex for reader-writer pattern on chunk maps.
    std::atomic<bool> shouldEnd{false};        ///< Exit signal for threads.

    Frustum frustum;                           ///< View frustum for culling.
    glm::mat4 lastViewProjection = glm::mat4(1.0f);
    ThreadPool chunkGenPool;                   ///< Parallel generator pool.
    int visibleCount = 0;
    /** @} */

    /** @name Render Caching
     * Pre-allocated lists to avoid per-frame allocations.
     * @{
     */
    int prevSortCamX = -1000, prevSortCamZ = -1000;
    std::vector<Chunk*> solidChunks;
    std::vector<Chunk*> billboardChunks;
    std::vector<Chunk*> waterChunks;
    std::vector<Chunk*> frustumVisibleChunks;
    std::vector<ChunkPos> toDeleteList;

    std::vector<Chunk*> chunkPool;             ///< Recycle bin for deleted chunks.
    std::vector<Chunk*> renderChunks;          ///< Staged list for thread-safe rendering.
    std::atomic<bool> renderChunksDirty{false};
    /** @} */

    OcclusionMethod occlusionMethod = OcclusionMethod::VOTING;

    /** @name Hardware Occlusion Resources @{ */
    unsigned int bboxVAO = 0, bboxVBO = 0, bboxEBO = 0;
    void initBoundingBoxMesh();
    /** @} */

    /**
     * @brief Caches computed values to avoid redundant per-chunk recomputation.
     */
    struct CameraCache {
        float maxDistSq{0.0f};
        int framesSinceMove{0};
    } cameraCache;
    
public:
    /**
     * @brief Renders a debug bounding box.
     */
    void drawBoundingBox(const glm::vec3& center, const glm::vec3& extents);
};
