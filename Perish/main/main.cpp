#include "window/window.hpp"
#include "game/game.h"
#include <thread>
#include <iostream>

int main() {
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	Overlay overlay;
	overlay.SetupOverlay(".gg/ perish");

	printf("[>>] Waiting for FiveM...\n");

	while (true) {
		FiveM::Setup();
		if (FiveM::offset::replay != 0) {
			printf("[>>] Game found!\n");
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	while (overlay.shouldRun) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		overlay.StartRender();

		FiveM::ESP::RunESP();

		if (overlay.RenderMenu)
			overlay.Render();
		overlay.EndRender();
	}
}