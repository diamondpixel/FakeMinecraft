#include "headers/WorldGen.h"

#include <OpenSimplexNoise.hh>
#include <random>

#include "headers/Blocks.h"
#include "headers/Planet.h"


void WorldGen::generateChunkData(ChunkPos chunkPos, uint32_t *chunkData, long seed) {
    static int chunkSize = CHUNK_SIZE;

    static OSN::Noise<2> noise2D;
    static OSN::Noise<3> noise3D;

    noise2D = OSN::Noise<2>(seed);
    noise3D = OSN::Noise<3>(seed);

    static NoiseSettings surfaceSettings[]{
        {0.01f, 20.0f, 0},
        {0.05f, 3.0f, 0}
    };
    static int surfaceSettingsLength = std::size(surfaceSettings);

    static NoiseSettings caveSettings[]{
        {0.05f, 1.0f, 0, .5f, 0, MAX_HEIGHT-10}
    };
    static int caveSettingsLength = std::size(caveSettings);

    static NoiseSettings oreSettings[]{
        {0.075f, 1.0f, 8.54f, .75f, 16, MAX_HEIGHT-20}
    };
    static int oreSettingsLength = std::size(oreSettings);

    static NoiseSettings lavaPocketSettings[]{
        {0.075f, 1.5f, 5.54f, .79f, 14, MAX_HEIGHT-20}
    };
    static int lavaPocketSettingsLength = std::size(lavaPocketSettings);

    static SurfaceFeature surfaceFeatures[]{
        // Pond
        {
            {0.43f, 1.0f, 2.35f, .85f, 1, 0}, // Noise
            {
                // Blocks
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 13, 13, 0, 0,
                0, 0, 13, 13, 13, 0, 0,
                0, 0, 0, 13, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,

                0, 2, 13, 13, 2, 0, 0,
                0, 2, 13, 13, 13, 2, 0,
                2, 13, 13, 13, 13, 13, 2,
                2, 13, 13, 13, 13, 13, 2,
                2, 13, 13, 13, 13, 13, 2,
                0, 2, 13, 13, 13, 2, 0,
                0, 0, 2, 13, 2, 0, 0,

                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
            },
            {
                // Replace?
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, true, true, false, false,
                false, false, true, true, true, false, false,
                false, false, false, true, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,

                false, false, true, true, false, false, false,
                false, false, true, true, true, false, false,
                false, true, true, true, true, true, false,
                false, true, true, true, true, true, false,
                false, true, true, true, true, true, false,
                false, false, true, true, true, false, false,
                false, false, false, true, false, false, false,

                false, false, true, true, false, false, false,
                false, false, true, true, true, true, false,
                false, true, true, true, true, true, false,
                false, true, true, true, true, true, false,
                false, true, true, true, true, true, false,
                false, false, true, true, true, false, false,
                false, false, false, true, false, false, false,
            },
            7, 3, 7, // Size
            -3, -2, -3 // Offset
        },
        // Big Tree
        {
            {
                5.23f, 1.0f, 0.54f, .8f, 1, 0
            },
            {
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 1, 1, 0, 0,
                0, 0, 1, 1, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 4, 4, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

                5, 5, 5, 5, 5, 5,
                5, 5, 5, 5, 5, 5,
                5, 5, 4, 4, 5, 5,
                5, 5, 4, 4, 5, 5,
                5, 5, 5, 5, 5, 5,
                5, 5, 5, 5, 5, 5,

                0, 0, 0, 0, 0, 0,
                0, 5, 5, 5, 5, 0,
                0, 5, 4, 4, 5, 0,
                0, 5, 4, 4, 5, 0,
                0, 5, 5, 5, 5, 0,
                0, 0, 0, 0, 0, 0,

                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 5, 5, 0, 0,
                0, 0, 5, 5, 0, 0,
                0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0,

            },
            {
                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,

                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,

                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,

                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,

                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,

                true, true, true, true, true, true,
                true, true, true, true, true, true,
                true, true, true, true, true, true,
                true, true, true, true, true, true,
                true, true, true, true, true, true,
                true, true, true, true, true, true,

                false, false, false, false, false, false,
                false, true, true, true, true, false,
                false, true, true, true, true, false,
                false, true, true, true, true, false,
                false, true, true, true, true, false,
                false, false, false, false, false, false,


                false, false, false, false, false, false,
                false, false, false, false, false, false,
                false, false, true, true, false, false,
                false, false, true, true, false, false,
                false, false, false, false, false, false,
                false, false, false, false, false, false,
            },
            6,
            8,
            6,
            -2,
            0,
            -2
        },
        // Tree
        {
            {
                17.23f, 1.0f, 8.54f, .75f, 1, 0
            },
            {
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 1, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,

                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 4, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,

                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 4, 0, 0,
                0, 0, 0, 0, 0,
                0, 0, 0, 0, 0,

                5, 5, 5, 5, 5,
                5, 5, 5, 5, 5,
                5, 5, 4, 5, 5,
                5, 5, 5, 5, 5,
                5, 5, 5, 5, 5,

                0, 5, 5, 5, 0,
                5, 5, 5, 5, 5,
                5, 5, 4, 5, 5,
                5, 5, 5, 5, 5,
                0, 5, 5, 5, 0,

                0, 0, 0, 0, 0,
                0, 0, 5, 0, 0,
                0, 5, 5, 5, 0,
                0, 0, 5, 0, 0,
                0, 0, 0, 0, 0,

                0, 0, 0, 0, 0,
                0, 0, 5, 0, 0,
                0, 5, 5, 5, 0,
                0, 0, 5, 0, 0,
                0, 0, 0, 0, 0,
            },
            {
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, true, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, true, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, true, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, true, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, true, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,

                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
                false, false, false, false, false,
            },
            5,
            7,
            5,
            -2,
            0,
            -2
        },
        // Tall Grass
        {
            {
                1.23f, 1.0f, 4.34f, .4f, 1, 0
            },
            {
                2, 7, 8
            },
            {
                false, false, false
            },
            1,
            3,
            1,
            0,
            0,
            0
        },
        // Grass
        {
            {
                2.65f, 1.0f, 8.54f, .3f, 1, 0
            },
            {
                2, 6
            },
            {
                false, false
            },
            1,
            2,
            1,
            0,
            0,
            0
        },
        // Poppy
        {
            {
                5.32f, 1.0f, 3.67f, .2f, 1, 0
            },
            {
                2, 9
            },
            {
                false, false
            },
            1,
            2,
            1,
            0,
            0,
            0
        },
        // White Tulip
        {
            {
                5.57f, 1.0f, 7.654f, .2f, 1, 0
            },
            {
                2, 10
            },
            {
                false, false
            },
            1,
            2,
            1,
            0,
            0,
            0
        },
        // Pink Tulip
        {
            {
                4.94f, 1.0f, 2.23f, .2f, 1, 0
            },
            {
                2, 11
            },
            {
                false, false
            },
            1,
            2,
            1,
            0,
            0,
            0
        },
        // Orange Tulip
        {
            {
                6.32f, 1.0f, 8.2f, .2f, 1, 0
            },
            {
                2, 12
            },
            {
                false, false
            },
            1,
            2,
            1,
            0,
            0,
            0
        },
    };
    static int surfaceFeaturesLength = std::size(surfaceFeatures);

    // Account for chunk position
    int startX = chunkPos.x * chunkSize;
    int startY = chunkPos.y * chunkSize;
    int startZ = chunkPos.z * chunkSize;

    int currentIndex = 0;
    for(int x = 0;x < chunkSize;x++) {
        for (int z = 0; z < chunkSize; z++) {
            // Surface noise
            int noiseY = MAX_HEIGHT;
            for (int i = 0; i < surfaceSettingsLength; i++) {
                noiseY += noise2D.eval(
                            (float) ((x + startX) * surfaceSettings[i].frequency) + surfaceSettings[i].offset,
                            (float) ((z + startZ) * surfaceSettings[i].frequency) + surfaceSettings[i].offset)
                        * surfaceSettings[i].amplitude;
            }

            for (int y = 0; y < chunkSize; y++) {
                // Cave noise
                bool cave = false;
                for (int i = 0; i < caveSettingsLength; i++) {
                    if (y + startY > caveSettings[i].maxHeight || y + startY < MIN_HEIGHT)
                        continue;

                    float noiseCaves = noise3D.eval(
                                           (float) ((x + startX) * caveSettings[i].frequency) + caveSettings[i].offset,
                                           (float) ((y + startY) * caveSettings[i].frequency) + caveSettings[i].offset,
                                           (float) ((z + startZ) * caveSettings[i].frequency) + caveSettings[i].offset)
                                       * caveSettings[i].amplitude;

                    if (noiseCaves > caveSettings[i].chance) {
                        cave = true;
                        break;
                    }
                }

                // Step 1: Terrain Shape (surface and caves) and Ores

                // Sky and Caves

                if (y + startY > noiseY) {
                    if (y + startY <= WATER_LEVEL)
                        chunkData[currentIndex] = Blocks::WATER;
                    else
                        chunkData[currentIndex] = Blocks::AIR;
                } else if (cave)
                    y + startY == MIN_HEIGHT
                        ? chunkData[currentIndex] = Blocks::BEDROCK
                        : chunkData[currentIndex] = Blocks::AIR;
                // Ground
                else {
                    bool blockSet = false;
                    for (int i = 0; i < oreSettingsLength; i++) {
                        if (y + startY > oreSettings[i].maxHeight || y + startY < MIN_HEIGHT + 2)
                            continue;

                        float noiseOre = noise3D.eval(
                                             (float) ((x + startX) * oreSettings[i].frequency) + oreSettings[i].offset,
                                             (float) ((y + startY) * oreSettings[i].frequency) + oreSettings[i].offset,
                                             (float) ((z + startZ) * oreSettings[i].frequency) + oreSettings[i].offset)
                                         * oreSettings[i].amplitude;

                        if (noiseOre > oreSettings[i].chance) {
                            chunkData[currentIndex] = oreSettings[i].block;
                            blockSet = true;
                            break;
                        }
                    }


                    for (int i = 0; i < lavaPocketSettingsLength; i++) {
                        if (y + startY > lavaPocketSettings[i].maxHeight || y + startY < MIN_HEIGHT + 2)
                            continue;

                        float noiseLava = noise3D.eval(
                                              (float) ((x + startX) * lavaPocketSettings[i].frequency) +
                                              lavaPocketSettings[i].offset,
                                              (float) ((y + startY) * lavaPocketSettings[i].frequency) +
                                              lavaPocketSettings[i].offset,
                                              (float) ((z + startZ) * lavaPocketSettings[i].frequency) +
                                              lavaPocketSettings[i].offset)
                                          * lavaPocketSettings[i].amplitude;

                        if (noiseLava > lavaPocketSettings[i].chance) {
                            chunkData[currentIndex] = lavaPocketSettings[i].block;
                            blockSet = true;
                            break;
                        }
                    }

                    if (!blockSet) {
                        if (y + startY == noiseY)
                            if (noiseY > WATER_LEVEL + 1)
                                chunkData[currentIndex] = Blocks::GRASS_BLOCK;
                            else
                                chunkData[currentIndex] = Blocks::SAND;
                        else if (y + startY > WATER_LEVEL - 5)
                            if (noiseY > WATER_LEVEL + 1)
                                chunkData[currentIndex] = Blocks::DIRT_BLOCK;
                            else
                                chunkData[currentIndex] = Blocks::SAND;
                        else if (y + startY > MIN_HEIGHT)
                            chunkData[currentIndex] = Blocks::STONE_BLOCK;
                        else
                            y + startY == MIN_HEIGHT
                                ? chunkData[currentIndex] = Blocks::BEDROCK
                                : chunkData[currentIndex] = Blocks::AIR;
                    }
                }
                currentIndex++;
            }
        }
    }

    // Step 3: Surface Features
    for (int i = 0; i < surfaceFeaturesLength; i++) {
        for (int x = -surfaceFeatures[i].sizeX - surfaceFeatures[i].offsetX; x < chunkSize - surfaceFeatures[i].offsetX;
             x++) {
            for (int z = -surfaceFeatures[i].sizeZ - surfaceFeatures[i].offsetZ;
                 z < chunkSize - surfaceFeatures[i].offsetZ; z++) {
                int noiseY = MAX_HEIGHT;
                for (int s = 0; s < surfaceSettingsLength; s++) {
                    noiseY += noise2D.eval(
                                (float) ((x + startX) * surfaceSettings[s].frequency) + surfaceSettings[s].offset,
                                (float) ((z + startZ) * surfaceSettings[s].frequency) + surfaceSettings[s].offset)
                            * surfaceSettings[s].amplitude;
                }

                if (noiseY + surfaceFeatures[i].offsetY > startY + chunkSize ||
                    noiseY + surfaceFeatures[i].sizeY + surfaceFeatures[i].offsetY < startY)
                    continue;

                // Check if it's in water or on sand
                if (noiseY < WATER_LEVEL + 2)
                    continue;

                // Check if it's in a cave
                bool cave = false;
                for (int i = 0; i < caveSettingsLength; i++) {
                    if (noiseY + startY > caveSettings[i].maxHeight)
                        continue;

                    float noiseCaves = noise3D.eval(
                                           (float) ((x + startX) * caveSettings[i].frequency) + caveSettings[i].offset,
                                           (float) ((noiseY) * caveSettings[i].frequency) + caveSettings[i].offset,
                                           (float) ((z + startZ) * caveSettings[i].frequency) + caveSettings[i].offset)
                                       * caveSettings[i].amplitude;

                    if (noiseCaves > caveSettings[i].chance) {
                        cave = true;
                        break;
                    }
                }

                if (cave)
                    continue;

                float noise = noise2D.eval(
                    (float) ((x + startX) * surfaceFeatures[i].noiseSettings.frequency) + surfaceFeatures[i].
                    noiseSettings.offset,
                    (float) ((z + startZ) * surfaceFeatures[i].noiseSettings.frequency) + surfaceFeatures[i].
                    noiseSettings.offset);

                if (noise > surfaceFeatures[i].noiseSettings.chance) {
                    int featureX = x + startX;
                    int featureY = noiseY;
                    int featureZ = z + startZ;

                    for (int fX = 0; fX < surfaceFeatures[i].sizeX; fX++) {
                        for (int fY = 0; fY < surfaceFeatures[i].sizeY; fY++) {
                            for (int fZ = 0; fZ < surfaceFeatures[i].sizeZ; fZ++) {
                                int localX = featureX + fX + surfaceFeatures[i].offsetX - startX;
                                int localY = featureY + fY + surfaceFeatures[i].offsetY - startY;
                                int localZ = featureZ + fZ + surfaceFeatures[i].offsetZ - startZ;

                                if (localX >= chunkSize || localX < 0 ||
                                    localY >= chunkSize || localY < 0 ||
                                    localZ >= chunkSize || localZ < 0)
                                    continue;

                                int featureIndex = fY * surfaceFeatures[i].sizeX * surfaceFeatures[i].sizeZ +
                                                   fX * surfaceFeatures[i].sizeZ + fZ;
                                int localIndex = localX * chunkSize * chunkSize +
                                                 localZ * chunkSize +
                                                 localY;

                                if (surfaceFeatures[i].replaceBlock[featureIndex] || chunkData[localIndex] == 0) {
                                    chunkData[localIndex] = surfaceFeatures[i].blocks[featureIndex];
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
