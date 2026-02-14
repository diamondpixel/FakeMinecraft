#include "Chunk.h"
#include "Blocks.h"
#include "BlockRegistry.h"
#include "Planet.h"
#include <queue>
#include <cstring>

// BFS light propagation from emissive blocks (lava)
// BFS light propagation from emissive blocks (lava) and Sky Light Raycast
void Chunk::computeLightMap() {
    // Clear light map
    memset(lightMap, 0, sizeof(lightMap));

    if (!chunkData) return;

    const BlockRegistry& registry = BlockRegistry::getInstance();
    const int CW = CHUNK_WIDTH;
    const int CH = CHUNK_HEIGHT;
    const int strideX = CW * CH;
    const int strideZ = CH;

    // --- PASS 1: BLOCK LIGHT (Lower 4 bits) ---
    struct LightNode {
        int16_t x, y, z;
        uint8_t level;
    };
    std::queue<LightNode> lightQueue;

    // Seed: find all lava blocks
    // Seed: find all lava blocks
    for (int x = 0; x < CW; x++) {
        for (int z = 0; z < CW; z++) {
            for (int y = 0; y < CH; y++) {
                int idx = x * strideX + z * strideZ + y;
                uint8_t blockId = chunkData->data[idx];
                
                if (blockId == Blocks::LAVA) {
                    // Set Block Light to 15 (preserve existing upper bits, though here it's 0)
                    lightMap[idx] = (lightMap[idx] & 0xF0) | 15;
                    lightQueue.push({(int16_t)x, (int16_t)y, (int16_t)z, 15});
                }
            }
        }
    }

    // Seed from Neighbor Chunks (Propagate incoming light)
    Chunk* nChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z - 1});
    Chunk* sChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z + 1});
    Chunk* wChunk = Planet::planet->getChunk({chunkPos.x - 1, chunkPos.y, chunkPos.z});
    Chunk* eChunk = Planet::planet->getChunk({chunkPos.x + 1, chunkPos.y, chunkPos.z});

    // North (My Z=0 touches Neighbor Z=CW-1)
    if (nChunk && nChunk->generated) {
        for (int x = 0; x < CW; x++) {
            for (int y = 0; y < CH; y++) {
                uint8_t nLight = nChunk->getLightLevel(x, y, CW - 1) & 0x0F;
                if (nLight > 1) {
                    uint8_t localLight = lightMap[x * strideX + 0 * strideZ + y] & 0x0F;
                    if (nLight - 1 > localLight) {
                        lightMap[x * strideX + 0 * strideZ + y] = (lightMap[x * strideX + 0 * strideZ + y] & 0xF0) | (nLight - 1);
                        lightQueue.push({(int16_t)x, (int16_t)y, (int16_t)0, (uint8_t)(nLight - 1)});
                    }
                }
            }
        }
    }

    // South (My Z=CW-1 touches Neighbor Z=0)
    if (sChunk && sChunk->generated) {
        for (int x = 0; x < CW; x++) {
            for (int y = 0; y < CH; y++) {
                uint8_t nLight = sChunk->getLightLevel(x, y, 0) & 0x0F;
                if (nLight > 1) {
                    int z = CW - 1;
                    uint8_t localLight = lightMap[x * strideX + z * strideZ + y] & 0x0F;
                    if (nLight - 1 > localLight) {
                        lightMap[x * strideX + z * strideZ + y] = (lightMap[x * strideX + z * strideZ + y] & 0xF0) | (nLight - 1);
                        lightQueue.push({(int16_t)x, (int16_t)y, (int16_t)z, (uint8_t)(nLight - 1)});
                    }
                }
            }
        }
    }

    // West (My X=0 touches Neighbor X=CW-1)
    if (wChunk && wChunk->generated) {
        for (int z = 0; z < CW; z++) {
            for (int y = 0; y < CH; y++) {
                uint8_t nLight = wChunk->getLightLevel(CW - 1, y, z) & 0x0F;
                if (nLight > 1) {
                    uint8_t localLight = lightMap[0 * strideX + z * strideZ + y] & 0x0F;
                    if (nLight - 1 > localLight) {
                        lightMap[0 * strideX + z * strideZ + y] = (lightMap[0 * strideX + z * strideZ + y] & 0xF0) | (nLight - 1);
                        lightQueue.push({(int16_t)0, (int16_t)y, (int16_t)z, (uint8_t)(nLight - 1)});
                    }
                }
            }
        }
    }

    // East (My X=CW-1 touches Neighbor X=0)
    if (eChunk && eChunk->generated) {
        for (int z = 0; z < CW; z++) {
            for (int y = 0; y < CH; y++) {
                uint8_t nLight = eChunk->getLightLevel(0, y, z) & 0x0F;
                if (nLight > 1) {
                    int x = CW - 1;
                    uint8_t localLight = lightMap[x * strideX + z * strideZ + y] & 0x0F;
                    if (nLight - 1 > localLight) {
                        lightMap[x * strideX + z * strideZ + y] = (lightMap[x * strideX + z * strideZ + y] & 0xF0) | (nLight - 1);
                        lightQueue.push({(int16_t)x, (int16_t)y, (int16_t)z, (uint8_t)(nLight - 1)});
                    }
                }
            }
        }
    }

    const int dx[] = {1, -1, 0, 0, 0, 0};
    const int dy[] = {0, 0, 1, -1, 0, 0};
    const int dz[] = {0, 0, 0, 0, 1, -1};

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        if (node.level <= 1) continue;
        uint8_t newLevel = node.level - 1;

        for (int d = 0; d < 6; d++) {
            int nx = node.x + dx[d];
            int ny = node.y + dy[d];
            int nz = node.z + dz[d];

            if (nx < 0 || nx >= CW || ny < 0 || ny >= CH || nz < 0 || nz >= CW) continue;

            int nIdx = nx * strideX + nz * strideZ + ny;
            
            // Check existing Block Light level (lower 4 bits)
            uint8_t currentBlockLight = lightMap[nIdx] & 0x0F;
            if (currentBlockLight >= newLevel) continue;

            uint8_t neighborBlock = chunkData->data[nIdx];
            const Block& block = registry.getBlock(neighborBlock);

            if (block.blockType == Block::SOLID) continue; // Leaves/Water pass light? Treating leaves as solid for BFS? 
            // Original code treated SOLID as blocking.

            // Update Block Light (keep Sky Light)
            lightMap[nIdx] = (lightMap[nIdx] & 0xF0) | newLevel;
            lightQueue.push({(int16_t)nx, (int16_t)ny, (int16_t)nz, newLevel});
        }
    }

    // --- PASS 2: SKY LIGHT (Upper 4 bits) ---
    // Simple vertical raycast for now (Sunlight Occlusion)
    // For full filtered propagation, we'd need another BFS.
    // Here: X, Z columns. Fill 15 down to first occluder.
    
    for (int x = 0; x < CW; x++) {
        for (int z = 0; z < CW; z++) {
            // Start from sky (Light 15)
            // Go down until we hit SOLID or LIQUID (water attenuates, but for simple occlusion we can stop or reduce)
            // Let's stop at SOLID. Leaves? Stop.
             
             bool exposed = true;
             for (int y = CH - 1; y >= 0; y--) {
                 int idx = x * strideX + z * strideZ + y;
                 uint8_t blockId = chunkData->data[idx];
                 const Block& block = registry.getBlock(blockId);

                 if (exposed) {
                     // Set Sky Light to 15
                     lightMap[idx] = (lightMap[idx] & 0x0F) | (15 << 4);
                 } else {
                     // Shadow (0)
                     lightMap[idx] = (lightMap[idx] & 0x0F) | (0 << 4);
                 }

                 // Occlusion check
                 if (block.blockType == Block::SOLID || block.blockType == Block::LEAVES || block.blockType == Block::LIQUID) {
                     exposed = false;
                 }
             }
        }
    }
}
