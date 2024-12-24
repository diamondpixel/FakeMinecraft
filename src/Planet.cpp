#include "headers/Planet.h"
#include <iostream>
#include <GL/glew.h>
#include "headers/WorldGen.h"

Planet *Planet::planet = nullptr;

// Public
Planet::Planet(Shader* solidShader, Shader* waterShader, Shader* billboardShader)
	: solidShader(solidShader), waterShader(waterShader), billboardShader(billboardShader)
{
	chunkThread = std::thread(&Planet::chunkThreadUpdate, this);
}

Planet::~Planet()
{
	shouldEnd = true;
	chunkThread.join();
}

void Planet::update(glm::vec3 cameraPos)
{
	camChunkX = cameraPos.x < 0 ? floor(cameraPos.x / CHUNK_SIZE) : cameraPos.x / CHUNK_SIZE;
	camChunkY = cameraPos.y < 0 ? floor(cameraPos.y / CHUNK_SIZE) : cameraPos.y / CHUNK_SIZE;
	camChunkZ = cameraPos.z < 0 ? floor(cameraPos.z / CHUNK_SIZE) : cameraPos.z / CHUNK_SIZE;

	glDisable(GL_BLEND);

	chunksLoading = 0;
	numChunks = 0;
	numChunksRendered = 0;
	chunkMutex.lock();
	for (auto it = chunks.begin(); it != chunks.end(); )
	{
		numChunks++;

		if (!(*it->second).ready)
			chunksLoading++;

		int chunkX = (*it->second).chunkPos.x;
		int chunkY = (*it->second).chunkPos.y;
		int chunkZ = (*it->second).chunkPos.z;
		if ((*it->second).ready && (abs(chunkX - camChunkX) > renderDistance ||
			abs(chunkY - camChunkY) > renderDistance ||
			abs(chunkZ - camChunkZ) > renderDistance))
		{
			// Out of range
			// Add chunk data to delete queue
			chunkDataDeleteQueue.emplace( chunkX,     chunkY, chunkZ );
			chunkDataDeleteQueue.emplace( chunkX + 1, chunkY, chunkZ );
			chunkDataDeleteQueue.emplace( chunkX - 1, chunkY, chunkZ );
			chunkDataDeleteQueue.emplace( chunkX, chunkY + 1, chunkZ );
			chunkDataDeleteQueue.emplace( chunkX, chunkY - 1, chunkZ );
			chunkDataDeleteQueue.emplace( chunkX, chunkY, chunkZ + 1 );
			chunkDataDeleteQueue.emplace( chunkX, chunkY, chunkZ - 1 );

			// Delete chunk
			delete it->second;
			it = chunks.erase(it);
		}
		else
		{
			numChunksRendered++;
			(*it->second).render(solidShader, billboardShader);
			++it;
		}
	}

	glEnable(GL_BLEND);
	waterShader->use();
	for (auto it = chunks.begin(); it != chunks.end(); )
	{
		int chunkX = (*it->second).chunkPos.x;
		int chunkY = (*it->second).chunkPos.y;
		int chunkZ = (*it->second).chunkPos.z;

		(*it->second).renderWater(waterShader);
		++it;
	}

	chunkMutex.unlock();
}

void Planet::chunkThreadUpdate()
{
	while (!shouldEnd)
	{
		for (auto it = chunkData.begin(); it != chunkData.end(); )
		{
			ChunkPos pos = it->first;

			if (chunks.find(pos) == chunks.end() &&
				chunks.find({ pos.x + 1, pos.y, pos.z }) == chunks.end() &&
				chunks.find({ pos.x - 1, pos.y, pos.z }) == chunks.end() &&
				chunks.find({ pos.x, pos.y + 1, pos.z }) == chunks.end() &&
				chunks.find({ pos.x, pos.y - 1, pos.z }) == chunks.end() &&
				chunks.find({ pos.x, pos.y, pos.z + 1 }) == chunks.end() &&
				chunks.find({ pos.x, pos.y, pos.z - 1 }) == chunks.end())
			{
				delete chunkData.at(pos);
				it = chunkData.erase(it);
			}
			else {
				++it;
			}
		}

		// Check if camera moved to new chunk
		if (camChunkX != lastCamX || camChunkY != lastCamY || camChunkZ != lastCamZ)
		{
			// Player moved chunks, start new chunk queue
			lastCamX = camChunkX;
			lastCamY = camChunkY;
			lastCamZ = camChunkZ;

			// Current chunk
			chunkMutex.lock();
			chunkQueue = {};
			if (chunks.find({ camChunkX, camChunkY, camChunkZ }) == chunks.end())
				chunkQueue.emplace( camChunkX, camChunkY, camChunkZ );

			for (int r = 0; r < renderDistance; r++)
			{
				// Add middle chunks
				for (int y = 0; y <= renderHeight; y++)
				{
					chunkQueue.emplace( camChunkX,     camChunkY + y, camChunkZ + r );
					chunkQueue.emplace( camChunkX + r, camChunkY + y, camChunkZ );
					chunkQueue.emplace( camChunkX,     camChunkY + y, camChunkZ - r );
					chunkQueue.emplace( camChunkX - r, camChunkY + y, camChunkZ );

					if (y > 0)
					{
						chunkQueue.emplace( camChunkX,     camChunkY - y, camChunkZ + r );
						chunkQueue.emplace( camChunkX + r, camChunkY - y, camChunkZ );
						chunkQueue.emplace( camChunkX,     camChunkY - y, camChunkZ - r );
						chunkQueue.emplace( camChunkX - r, camChunkY - y, camChunkZ);
					}
				}

				// Add edges
				for (int e = 1; e < r; e++)
				{
					for (int y = 0; y <= renderHeight; y++)
					{
						chunkQueue.emplace( camChunkX + e, camChunkY + y, camChunkZ + r );
						chunkQueue.emplace( camChunkX - e, camChunkY + y, camChunkZ + r );
						chunkQueue.emplace( camChunkX + r, camChunkY + y, camChunkZ + e );
						chunkQueue.emplace( camChunkX + r, camChunkY + y, camChunkZ - e );
						chunkQueue.emplace( camChunkX + e, camChunkY + y, camChunkZ - r );
						chunkQueue.emplace( camChunkX - e, camChunkY + y, camChunkZ - r );
						chunkQueue.emplace( camChunkX - r, camChunkY + y, camChunkZ + e );
						chunkQueue.emplace( camChunkX - r, camChunkY + y, camChunkZ - e );

						if (y > 0)
						{
							chunkQueue.emplace( camChunkX + e, camChunkY - y, camChunkZ + r );
							chunkQueue.emplace( camChunkX - e, camChunkY - y, camChunkZ + r );
							chunkQueue.emplace( camChunkX + r, camChunkY - y, camChunkZ + e );
							chunkQueue.emplace( camChunkX + r, camChunkY - y, camChunkZ - e );
							chunkQueue.emplace( camChunkX + e, camChunkY - y, camChunkZ - r );
							chunkQueue.emplace( camChunkX - e, camChunkY - y, camChunkZ - r );
							chunkQueue.emplace( camChunkX - r, camChunkY - y, camChunkZ + e );
							chunkQueue.emplace( camChunkX - r, camChunkY - y, camChunkZ - e );
						}
					}
				}

				// Add corners
				for (int y = 0; y <= renderHeight; y++)
				{
					chunkQueue.emplace( camChunkX + r, camChunkY + y, camChunkZ + r );
					chunkQueue.emplace( camChunkX + r, camChunkY + y, camChunkZ - r );
					chunkQueue.emplace( camChunkX - r, camChunkY + y, camChunkZ + r );
					chunkQueue.emplace( camChunkX - r, camChunkY + y, camChunkZ - r );

					if (y > 0)
					{
						chunkQueue.emplace( camChunkX + r, camChunkY - y, camChunkZ + r );
						chunkQueue.emplace( camChunkX + r, camChunkY - y, camChunkZ - r );
						chunkQueue.emplace( camChunkX - r, camChunkY - y, camChunkZ + r );
						chunkQueue.emplace( camChunkX - r, camChunkY - y, camChunkZ - r );
					}
				}
			}

			chunkMutex.unlock();
		}
		else
		{
			chunkMutex.lock();
			if (!chunkDataQueue.empty())
			{
				ChunkPos chunkPos = chunkDataQueue.front();

				if (chunkData.find(chunkPos) != chunkData.end())
				{
					chunkDataQueue.pop();
					chunkMutex.unlock();
					continue;
				}

				chunkMutex.unlock();

				uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
				ChunkData* data = new ChunkData(d);

				WorldGen::generateChunkData(chunkPos, d);

				chunkMutex.lock();
				chunkData[chunkPos] = data;
				chunkDataQueue.pop();
				chunkMutex.unlock();
			}
			else
			{
				if (!chunkQueue.empty())
				{
					// Check if chunk exists
					ChunkPos chunkPos = chunkQueue.front();
					if (chunks.find(chunkPos) != chunks.end())
					{
						chunkQueue.pop();
						chunkMutex.unlock();
						continue;
					}

					chunkMutex.unlock();

					// Create chunk object
					Chunk* chunk = new Chunk(chunkPos, solidShader, waterShader);

					// Set chunk data
					{
						chunkMutex.lock();
						if (chunkData.find(chunkPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(chunkPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->chunkData = data;

							chunkMutex.lock();
							chunkData[chunkPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->chunkData = chunkData.at(chunkPos);
							chunkMutex.unlock();
						}
					}

					// Set top data
					{
						ChunkPos topPos(chunkPos.x, chunkPos.y + 1, chunkPos.z);
						chunkMutex.lock();
						if (chunkData.find(topPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(topPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->upData = data;

							chunkMutex.lock();
							chunkData[topPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->upData = chunkData.at(topPos);
							chunkMutex.unlock();
						}
					}

					// Set bottom data
					{
						ChunkPos bottomPos(chunkPos.x, chunkPos.y - 1, chunkPos.z);
						chunkMutex.lock();
						if (chunkData.find(bottomPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(bottomPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->downData = data;

							chunkMutex.lock();
							chunkData[bottomPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->downData = chunkData.at(bottomPos);
							chunkMutex.unlock();
						}
					}

					// Set north data
					{
						ChunkPos northPos(chunkPos.x, chunkPos.y, chunkPos.z - 1);
						chunkMutex.lock();
						if (chunkData.find(northPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(northPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->northData = data;

							chunkMutex.lock();
							chunkData[northPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->northData = chunkData.at(northPos);
							chunkMutex.unlock();
						}
					}

					// Set south data
					{
						ChunkPos southPos(chunkPos.x, chunkPos.y, chunkPos.z + 1);
						chunkMutex.lock();
						if (chunkData.find(southPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(southPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->southData = data;

							chunkMutex.lock();
							chunkData[southPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->southData = chunkData.at(southPos);
							chunkMutex.unlock();
						}
					}

					// Set east data
					{
						ChunkPos eastPos(chunkPos.x + 1, chunkPos.y, chunkPos.z);
						chunkMutex.lock();
						if (chunkData.find(eastPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(eastPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->eastData = data;

							chunkMutex.lock();
							chunkData[eastPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->eastData = chunkData.at(eastPos);
							chunkMutex.unlock();
						}
					}

					// Set west data
					{
						ChunkPos westPos(chunkPos.x - 1, chunkPos.y, chunkPos.z);
						chunkMutex.lock();
						if (chunkData.find(westPos) == chunkData.end())
						{
							chunkMutex.unlock();
							uint16_t* d = new uint16_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

							WorldGen::generateChunkData(westPos, d);

							ChunkData* data = new ChunkData(d);

							chunk->westData = data;

							chunkMutex.lock();
							chunkData[westPos] = data;
							chunkMutex.unlock();
						}
						else
						{
							chunk->westData = chunkData.at(westPos);
							chunkMutex.unlock();
						}
					}

					// Generate chunk mesh
					chunk->generateChunkMesh();

					// Finish
					chunkMutex.lock();
					chunks[chunkPos] = chunk;
					chunkQueue.pop();
					chunkMutex.unlock();
				}
				else
				{
					chunkMutex.unlock();

					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				}
			}
		}
	}
}

Chunk* Planet::getChunk(ChunkPos chunkPos)
{
	chunkMutex.lock();
	if (chunks.find(chunkPos) == chunks.end())
	{
		chunkMutex.unlock();
		return nullptr;
	}
	else
	{
		chunkMutex.unlock();
		return chunks.at(chunkPos);
	}
}

void Planet::clearChunkQueue()
{
	lastCamX++;
}