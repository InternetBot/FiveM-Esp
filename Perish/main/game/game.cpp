#include "game.h"
#include "../window/window.hpp"
#include <stdio.h>
#include <string>
#include <thread>
#include <chrono>

std::vector<std::string> processes = {
	"FiveM_GTAProcess.exe"
};

memify mem(processes);

namespace FiveM {
	namespace offset {
		uintptr_t world = 0, replay = 0, viewport = 0, localplayer = 0;
		uintptr_t boneList = 0, boneMatrix = 0x60;
		uintptr_t playerInfo = 0, playerHealth = 0x280, playerPosition = 0x90;
		uintptr_t playerIdOffset = 0x88, armorOffset = 0x1530;
		uintptr_t base = 0;
	}
	bool espEnabled = true;
}

std::unordered_map<uintptr_t, PedData> pedCache;
std::mutex cacheMutex;

struct OffsetSet {
	const char* version;
	uintptr_t world;
	uintptr_t replay;
	uintptr_t viewport;
	uintptr_t playerInfo;
	uintptr_t playerIdOffset;
	uintptr_t armorOffset;
};

static OffsetSet offset_sets[] = {
	{ "b2699", 0x26684D8, 0x20304C8, 0x20D8C90, 0x10C8, 0x88, 0x1530 },
	{ "b2802", 0x254D448, 0x1F5B820, 0x1FBC100, 0x10A8, 0x88, 0x150C },
	{ "b2944", 0x257BEA0, 0x1F42068, 0x1FEAAC0, 0x10C8, 0x88, 0x150C },
	{ "b3095", 0x2593320, 0x1F58B58, 0x20019E0, 0x10A8, 0x88, 0x150C },
	{ "b3258", 0x25B14B0, 0x1FBD4F0, 0x201DBA0, 0x10A8, 0x88, 0x150C },
};

static Vec3 get_bone_position(uintptr_t ped, int bone_position)
{
	Matrix bone_matrix = mem.Read<Matrix>(ped + 0x60);
	Vector3 bone = mem.Read<Vector3>(ped + (0x410 + 0x10 * bone_position));
	DirectX::SimpleMath::Vector3 boneVec(bone.x, bone.y, bone.z);
	DirectX::SimpleMath::Vector3 transformedBoneVec = DirectX::XMVector3Transform(boneVec, bone_matrix);
	return Vec3(transformedBoneVec.x, transformedBoneVec.y, transformedBoneVec.z);
}

static void draw_skeleton(ImDrawList* draw_list, uintptr_t ped, const Matrix& viewport)
{
	int bone_positions[][2] = {
		{ 0, 7 }, { 7, 6 }, { 7, 5 }, { 7, 8 }, { 8, 3 }, { 8, 4 }
	};

	for (int i = 0; i < 6; ++i) {
		Vec2 bone1_screen, bone2_screen;
		Vec3 bone_1 = get_bone_position(ped, bone_positions[i][0]);
		Vec3 bone_2 = get_bone_position(ped, bone_positions[i][1]);

		if (bone_1.IsZero() || bone_2.IsZero())
			continue;

		if (bone_1.world_to_screen(viewport, bone1_screen) && bone_2.world_to_screen(viewport, bone2_screen)) {
			draw_list->AddLine(
				ImVec2(bone1_screen.x, bone1_screen.y),
				ImVec2(bone2_screen.x, bone2_screen.y),
				IM_COL32(255, 255, 255, 255), 2.0f);
		}
	}
}

static void DrawHealthBar(ImDrawList* draw_list, float x, float y, float width, float height, float health, float maxHealth)
{
	float healthPercent = health / maxHealth;
	if (healthPercent > 1.0f) healthPercent = 1.0f;
	if (healthPercent < 0.0f) healthPercent = 0.0f;

	draw_list->AddRectFilled({ x, y }, { x + width, y + height }, IM_COL32(0, 0, 0, 200));

	int r = (int)((1.0f - healthPercent) * 255);
	int g = (int)(healthPercent * 255);

	draw_list->AddRectFilled({ x + 1, y + 1 }, { x + 1 + (width - 2) * healthPercent, y + height - 1 }, IM_COL32(r, g, 0, 255));
	draw_list->AddRect({ x, y }, { x + width, y + height }, IM_COL32(255, 255, 255, 255));
}

static float GetDistance(Vec3 a, Vec3 b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return sqrt(dx * dx + dy * dy + dz * dz);
}

void FiveM::Setup()
{
	auto process_name = mem.GetProcessName();
	auto game_base = mem.GetBase(process_name);

	if (game_base == 0)
		return;

	offset::base = game_base;

	WORD dos_header = mem.Read<WORD>(game_base);
	if (dos_header != 0x5A4D)
		return;

	for (int i = 0; i < 5; i++) {
		offset::world = mem.Read<uintptr_t>(game_base + offset_sets[i].world);
		offset::replay = mem.Read<uintptr_t>(game_base + offset_sets[i].replay);
		offset::viewport = mem.Read<uintptr_t>(game_base + offset_sets[i].viewport);

		if (offset::world != 0 && offset::replay != 0 && offset::viewport != 0) {
			offset::localplayer = mem.Read<uintptr_t>(offset::world + 0x8);
			offset::playerInfo = offset_sets[i].playerInfo;
			offset::playerIdOffset = offset_sets[i].playerIdOffset;
			offset::armorOffset = offset_sets[i].armorOffset;
			printf("[>>] Version: %s\n", offset_sets[i].version);
			printf("[>>] Base: %llX\n", game_base);
			printf("[>>] World: %llX\n", offset::world);
			printf("[>>] Replay: %llX\n", offset::replay);
			printf("[>>] Viewport: %llX\n", offset::viewport);
			break;
		}
	}
}

void FiveM::ESP::RunESP()
{
	if (GetAsyncKeyState(VK_F11) & 1) {
		espEnabled = !espEnabled;
	}

	if (!espEnabled)
		return;

	ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

	uintptr_t game_base = offset::base;
	if (game_base == 0)
		return;

	uintptr_t viewport_ptr = 0, world_ptr = 0, replay_ptr = 0;

	for (int i = 0; i < 5; i++) {
		world_ptr = mem.Read<uintptr_t>(game_base + offset_sets[i].world);
		replay_ptr = mem.Read<uintptr_t>(game_base + offset_sets[i].replay);
		viewport_ptr = mem.Read<uintptr_t>(game_base + offset_sets[i].viewport);
		if (world_ptr != 0 && replay_ptr != 0 && viewport_ptr != 0)
			break;
	}

	if (viewport_ptr == 0 || world_ptr == 0 || replay_ptr == 0)
		return;

	Matrix view_matrix = mem.Read<Matrix>(viewport_ptr + 0x24C);

	uintptr_t currentLocalPlayer = mem.Read<uintptr_t>(world_ptr + 0x8);
	if (!currentLocalPlayer)
		return;

	Vec3 localPos = mem.Read<Vec3>(currentLocalPlayer + 0x90);

	std::vector<uintptr_t> drawnPeds;

	auto DrawPed = [&](uintptr_t ped) {
		if (!ped || ped == currentLocalPlayer)
			return;
		if (ped < 0x10000 || ped > 0x7FFFFFFFFFFF)
			return;

		for (auto& p : drawnPeds) {
			if (p == ped) return;
		}

		Vec3 pedPos = mem.Read<Vec3>(ped + 0x90);
		if (pedPos.x == 0 && pedPos.y == 0 && pedPos.z == 0)
			return;

		float health = mem.Read<float>(ped + 0x280);
		if (health <= 0)
			return;

		float maxHealth = mem.Read<float>(ped + 0x2A0);
		float armor = mem.Read<float>(ped + offset::armorOffset);

		float distance = GetDistance(localPos, pedPos);
		if (distance > 2000)
			return;

		drawnPeds.push_back(ped);

		Vec3 headBone = get_bone_position(ped, 0);
		bool hasValidBones = !headBone.IsZero();

		if (hasValidBones) {
			draw_skeleton(draw_list, ped, view_matrix);
		}

		Vec3 finalPos = hasValidBones ? headBone : Vec3(pedPos.x, pedPos.y, pedPos.z + 1.8f);

		Vec2 screen;
		if (finalPos.world_to_screen(view_matrix, screen)) {
			ImU32 dotColor = hasValidBones ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 165, 0, 255);
			draw_list->AddCircleFilled({ screen.x, screen.y }, 4.0f, dotColor);

			float barWidth = 50.0f;
			float barHeight = 5.0f;
			float barX = screen.x - barWidth / 2;
			float barY = screen.y - 20.0f;

			float displayHealth = health - 100.0f;
			float displayMaxHealth = maxHealth - 100.0f;
			if (displayMaxHealth <= 0) displayMaxHealth = 100.0f;
			if (displayHealth < 0) displayHealth = 0;

			DrawHealthBar(draw_list, barX, barY, barWidth, barHeight, displayHealth, displayMaxHealth);

			if (armor > 0) {
				float armorBarY = barY + barHeight + 1;
				draw_list->AddRectFilled({ barX, armorBarY }, { barX + barWidth, armorBarY + 3 }, IM_COL32(0, 0, 0, 200));
				float armorPercent = armor / 100.0f;
				if (armorPercent > 1.0f) armorPercent = 1.0f;
				draw_list->AddRectFilled({ barX + 1, armorBarY + 1 }, { barX + 1 + (barWidth - 2) * armorPercent, armorBarY + 2 }, IM_COL32(0, 150, 255, 255));
			}

			char infoText[32];
			sprintf_s(infoText, "%.0fm", distance);
			ImVec2 textSize = ImGui::CalcTextSize(infoText);

			draw_list->AddText({ screen.x - textSize.x / 2 + 1, barY - 14 }, IM_COL32(0, 0, 0, 255), infoText);
			draw_list->AddText({ screen.x - textSize.x / 2, barY - 15 }, IM_COL32(255, 255, 255, 255), infoText);
		}
		};

	// Scan ped list
	uintptr_t ped_interface = mem.Read<uintptr_t>(replay_ptr + 0x18);
	if (ped_interface) {
		uintptr_t pedList = mem.Read<uintptr_t>(ped_interface + 0x100);
		int pedCount = mem.Read<int>(ped_interface + 0x108);
		if (pedCount > 256) pedCount = 256;
		if (pedCount < 0) pedCount = 256;

		if (pedList) {
			for (int i = 0; i < pedCount; i++) {
				uintptr_t ped = mem.Read<uintptr_t>(pedList + (i * 0x10));
				DrawPed(ped);
			}
		}
	}

	// Scan vehicles
	uintptr_t veh_interface = mem.Read<uintptr_t>(replay_ptr + 0x10);
	if (veh_interface) {
		uintptr_t vehList = mem.Read<uintptr_t>(veh_interface + 0x180);
		int vehCount = mem.Read<int>(veh_interface + 0x188);
		if (vehCount > 256) vehCount = 256;
		if (vehCount < 0) vehCount = 128;

		if (vehList) {
			for (int i = 0; i < vehCount; i++) {
				uintptr_t veh = mem.Read<uintptr_t>(vehList + (i * 0x10));
				if (!veh || veh < 0x10000 || veh > 0x7FFFFFFFFFFF)
					continue;

				for (int seat = 0; seat < 16; seat++) {
					uintptr_t ped = mem.Read<uintptr_t>(veh + 0x1890 + (seat * 0x8));
					DrawPed(ped);
				}
			}
		}
	}
}