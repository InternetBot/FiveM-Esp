#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "../mem/memify.h"
#include "../../math.h"

struct PedData {
	Vec3 position_origin;
	float health;
	float maxHealth;
	float armor;

	PedData() : position_origin(0, 0, 0), health(0), maxHealth(0), armor(0) {}
};

namespace FiveM {
	namespace offset {
		extern uintptr_t world, replay, viewport, localplayer;
		extern uintptr_t boneList, boneMatrix;
		extern uintptr_t playerInfo, playerHealth, playerPosition;
		extern uintptr_t playerIdOffset, armorOffset;
		extern uintptr_t base;
	}

	extern bool espEnabled;

	namespace ESP {
		void RunESP();
	}

	void Setup();
}

extern memify mem;
extern std::unordered_map<uintptr_t, PedData> pedCache;
extern std::mutex cacheMutex;