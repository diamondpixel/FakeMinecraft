#include "headers/WorldGen.h"
#include <algorithm>
#include <OpenSimplexNoise.hh>
#include "headers/Blocks.h"
#include "headers/NoiseSettings.h"

void WorldGen::generateChunkData(int chunkX, int chunkY, int chunkZ, int chunkSize,
                                 std::vector<unsigned int> *chunkData) {
    // Init noise
    static OSN::Noise<2> noise2D;
    static OSN::Noise<3> noise3D;

    // Init noise settings
    static NoiseSettings surfaceSettings[]{
        {0.01f, 20.0f, 0},
        {0.1f, 3.0f, 0}
    };
    static int surfaceSettingsLength = sizeof(surfaceSettings) / sizeof(*surfaceSettings);

    static NoiseSettings caveSettings[]{
        {0.05f, 1.0f, 0, .5f, 0, 100}
    };
    static int caveSettingsLength = sizeof(caveSettings) / sizeof(*caveSettings);

    static NoiseSettings oreSettings[]{
        {0.075f, 1.0f, 8.54f, .75f, 1, 0}
    };
    static int oreSettingsLength = sizeof(oreSettings) / sizeof(*oreSettings);

    static SurfaceFeature surfaceFeatures[]{
        // Tree
        {
            {4.23f, 1.0f, 8.54f, .8f, 1, 0},
            {
                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 0, 0, 0, 0,

                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 5, 0,
                0, 0, 0, 5, 5, 5, 5,
                0, 0, 0, 5, 5, 5, 0,
                0, 0, 0, 5, 5, 0, 0,

                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 5, 5,
                1, 4, 4, 4, 4, 5, 5,
                0, 0, 0, 5, 5, 5, 5,
                0, 0, 0, 5, 5, 0, 0,

                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 5, 0,
                0, 0, 0, 5, 5, 5, 5,
                0, 0, 0, 5, 5, 5, 0,
                0, 0, 0, 5, 5, 0, 0,

                0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 5, 5, 0, 0,
                0, 0, 0, 0, 0, 0, 0,
            },
            {
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,

                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,

                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                true, true, true, true, true, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,

                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,

                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
                false, false, false, false, false, false, false,
            },
            5,
            7,
            5,
            -2,
            0,
            -2
        },
        // Grass
        {
            {2.65f, 1.0f, 8.54f, .5f, 1, 0},
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
        // Tall Grass
        {
            {1.23f, 1.0f, 4.34f, .6f, 1, 0},
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
        // Poppy
        {
            {5.32f, 1.0f, 3.67f, .8f, 1, 0},
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
            {5.57f, 1.0f, 7.654f, .8f, 1, 0},
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
            {4.94f, 1.0f, 2.23f, .8f, 1, 0},
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
            {6.32f, 1.0f, 8.2f, .85f, 1, 0},
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
    static int surfaceFeaturesLength = sizeof(surfaceFeatures) / sizeof(*surfaceFeatures);

    // Set vector size
    chunkData->reserve(chunkSize * chunkSize * chunkSize);

    // Account for chunk position
    int startX = chunkX * chunkSize;
    int startY = chunkY * chunkSize;
    int startZ = chunkZ * chunkSize;

    for (int x = 0; x < chunkSize; x++) {
        for (int z = 0; z < chunkSize; z++) {
            // Surface noise
            int noiseY = 20;
            float noiseValue = 0.0f;
            float totalAmplitude = 0.0f;
            for (int i = 0; i < surfaceSettingsLength; i++) {
                float layerNoise = noise2D.eval(
                                       (float) ((x + startX) * surfaceSettings[i].frequency) + surfaceSettings[i].
                                       offset,
                                       (float) ((z + startZ) * surfaceSettings[i].frequency) + surfaceSettings[i].
                                       offset)
                                   * surfaceSettings[i].amplitude;

                noiseValue += layerNoise * surfaceSettings[i].amplitude;
                totalAmplitude += surfaceSettings[i].amplitude;
            }

            // Normalize the noise value for smoother results
            noiseY += noiseValue / totalAmplitude;

            for (int y = 0; y < chunkSize; y++) {
                // Cave noise
                bool cave = false;
                for (int i = 0; i < caveSettingsLength; i++) {
                    if (y + startY > caveSettings[i].maxHeight)
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
                if (y + startY > noiseY)
                    chunkData->push_back(0);
                else if (cave)
                    chunkData->push_back(0);
                // Ground
                else {
                    bool blockSet = false;
                    for (int i = 0; i < oreSettingsLength; i++) {
                        if (y + startY > oreSettings[i].maxHeight)
                            continue;

                        float noiseOre = noise3D.eval(
                                             (float) ((x + startX) * oreSettings[i].frequency) + oreSettings[i].offset,
                                             (float) ((y + startY) * oreSettings[i].frequency) + oreSettings[i].offset,
                                             (float) ((z + startZ) * oreSettings[i].frequency) + oreSettings[i].offset)
                                         * oreSettings[i].amplitude;

                        if (noiseOre > oreSettings[i].chance) {
                            chunkData->push_back(oreSettings[i].block);
                            blockSet = true;
                            break;
                        }
                    }

                    if (!blockSet) {
                        if (y + startY == noiseY)
                            chunkData->push_back(Blocks::GRASS_BLOCK);
                        else if (y + startY > 10)
                            chunkData->push_back(Blocks::DIRT_BLOCK);
                        else
                            chunkData->push_back(Blocks::STONE_BLOCK);
                    }
                }
            }
        }
    }

    // Step 3: Surface Features
    for (int i = 0; i < surfaceFeaturesLength; i++) {
        for (int x = -surfaceFeatures[i].sizeX - surfaceFeatures[i].offsetX; x < chunkSize - surfaceFeatures[i].offsetX;
             x++) {
            for (int z = -surfaceFeatures[i].sizeZ - surfaceFeatures[i].offsetZ;
                 z < chunkSize - surfaceFeatures[i].offsetZ; z++) {
                int noiseY = 20;
                float noiseValue = 0.0f;
                float totalAmplitude = 0.0f;
                for (int i = 0; i < surfaceSettingsLength; i++) {
                    float layerNoise = noise2D.eval(
                                           (float) ((x + startX) * surfaceSettings[i].frequency) + surfaceSettings[i].
                                           offset,
                                           (float) ((z + startZ) * surfaceSettings[i].frequency) + surfaceSettings[i].
                                           offset)
                                       * surfaceSettings[i].amplitude;

                    noiseValue += layerNoise * surfaceSettings[i].amplitude;
                    totalAmplitude += surfaceSettings[i].amplitude;
                }

                // Normalize the noise value for smoother results
                noiseY += noiseValue / totalAmplitude;

                if (noiseY + surfaceFeatures[i].offsetY > startY + 32 || noiseY + surfaceFeatures[i].sizeY +
                    surfaceFeatures[i].offsetY < startY)
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
                                //std::cout << "FeatureX: " << featureX << ", fX: " << fX << ", startX: " << startX << ", localX: " << localX << '\n';
                                int localY = featureY + fY + surfaceFeatures[i].offsetY - startY;
                                //std::cout << "FeatureY: " << featureY << ", fY: " << fY << ", startY: " << startY << ", localY: " << localY << '\n';
                                int localZ = featureZ + fZ + surfaceFeatures[i].offsetZ - startZ;
                                //std::cout << "FeatureZ: " << featureZ << ", fZ: " << fZ << ", startZ: " << startZ << ", localZ: " << localZ << '\n';

                                if (localX >= 32 || localX < 0)
                                    continue;
                                if (localY >= 32 || localY < 0)
                                    continue;
                                if (localZ >= 32 || localZ < 0)
                                    continue;

                                int featureIndex = fX * surfaceFeatures[i].sizeZ * surfaceFeatures[i].sizeY +
                                                   fZ * surfaceFeatures[i].sizeY +
                                                   fY;
                                //std::cout << "Feature Index: " << featureIndex << '\n';
                                int localIndex = localX * chunkSize * chunkSize + localZ * chunkSize + localY;
                                //std::cout << "Local Index: " << localIndex << ", Max Index: " << chunkData->size() << '\n';

                                if (surfaceFeatures[i].replaceBlock[featureIndex] || chunkData->at(localIndex) == 0)
                                    chunkData->at(localIndex) = surfaceFeatures[i].blocks[featureIndex];
                            }
                        }
                    }

                    //int index = x * chunkSize * chunkSize + z * chunkSize + noiseY;
                    //chunkData->at(index) = surfaceFeatures[i].block;
                    //index = x * chunkSize * chunkSize + z * chunkSize + noiseY + 1;
                    //chunkData->at(index) = surfaceFeatures[i].block;
                }
            }
        }
    }
}
