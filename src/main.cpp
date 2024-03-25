#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <thread>
#include <string>
#include <cmath>

#include "utils/offsets.h"
#define DEBUG(_msg) MessageBoxA(nullptr, _msg, "Debug", MB_OK)

namespace game {

    World* GetServer(uintptr_t baseAddr) {
        const auto serverAddress = baseAddr+offset::serverStatic;
        const auto server = *reinterpret_cast<World**>(serverAddress);
        printf("Server World: %p\n", server);

        return server;
    }

    LocalPlayer* GetLocalPlayer(uintptr_t baseAddr) {
        const auto localPlayerAddress = baseAddr+offset::localPlayerStatic;
        const auto localPlayer = *reinterpret_cast<LocalPlayer**>(localPlayerAddress);
        printf("LocalPlayer: %p\n", localPlayer);

        return localPlayer;
    }

}

namespace g {

    float delta = 0.0f;
    float enlapsTime = 0.0f;

    HMODULE instance;

    World* server;
    LocalPlayer* localPlayer;
    Entity* localPlayerEntity;

    void Setup() {
        const intptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandle(0));

        server = game::GetServer(baseAddr);
        localPlayer = game::GetLocalPlayer(baseAddr);

        if(!server || !localPlayer) {
            DEBUG("Failed to get server or localPlayer");
            return;
        }

        localPlayerEntity = &server->entities[server->idLocalPlayer];

    }
}

namespace u {
    FILE *f;

    float GetDistance(Vector2 a, Vector2 b) {
        return sqrt(
            pow(a.x - b.x, 2) + 
            pow(a.y - b.y, 2)
        );
    }

    Entity GetNearestPlayer(World* server, float maxDistance) {
        int myId = server->idLocalPlayer;
        Entity localPlayerEntity = server->entities[myId];
        Entity *entities = server->entities;
        int onlinePlayers = server->onlinePlayers;
        
        Entity nearestPlayer;
        Entity emptyPlayer;
        float nearestDistance = 9999;
        
        for (int i = 0; i < onlinePlayers; i++) {

            if(i == myId || entities[i].gametick == 0) continue;

            float distance = GetDistance(localPlayerEntity.playerPos, entities[i].playerPos);

            if (distance > maxDistance) continue;

            if(GetDistance(nearestPlayer.playerPos, localPlayerEntity.playerPos) > nearestDistance) {
                nearestPlayer = emptyPlayer;
                nearestDistance = 9999;
            }

            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestPlayer = entities[i];
            }
        }

        if(GetDistance(nearestPlayer.playerPos, localPlayerEntity.playerPos) > maxDistance) return emptyPlayer;

        return nearestPlayer;
    }

    void StartTrampoline(std::string name) {
        AllocConsole();
        freopen_s(&f, "CONOUT$", "w", stdout);
        
        char title[200];
        sprintf_s(title, "%s Injected", name.c_str());
        DEBUG(title);
    }

    void EndTrampoline(){ 
        if (f) fclose(f);
        FreeConsole();

        FreeLibraryAndExitThread(g::instance, 0);
    }
}

namespace stabilizer {
    bool enabled = false;
    bool needReset = false;

    void Reset() {
        if(!needReset) return;

        g::localPlayer->rightDir = KeyStatus::RELEASED;
        g::localPlayer->leftDir = KeyStatus::RELEASED;

        needReset = false;
    }

    void Execute() {
        if(needReset) return;

        Entity nearestPlayer = u::GetNearestPlayer(g::server, 500);

        if(nearestPlayer.playerPos.x == 0.0f && nearestPlayer.playerPos.y == 0.0f) {
            // printf("No nearest player\n");
            Reset();
            return;
        }

        // printf("Nearest player: %f, %f\n", nearestPlayer.playerPos.x, nearestPlayer.playerPos.y);

        if(g::localPlayerEntity->playerPos.x < nearestPlayer.playerPos.x) {
            g::localPlayer->leftDir = KeyStatus::RELEASED;
            g::localPlayer->rightDir = KeyStatus::HOLDING;
        } 

        if(g::localPlayerEntity->playerPos.x > nearestPlayer.playerPos.x) {
            g::localPlayer->rightDir = KeyStatus::RELEASED;
            g::localPlayer->leftDir = KeyStatus::HOLDING;
        }
  
        needReset = true;
    }

}

namespace aimbot {
    bool enabled = false;
    
    void AimAt(Entity target) {
        if(target.playerPos.x == 0.0f && target.playerPos.y == 0.0f) return;

        g::localPlayer->aimPosScreen.x = target.playerPos.x - g::localPlayerEntity->playerPos.x;
        g::localPlayer->aimPosScreen.y = target.playerPos.y - g::localPlayerEntity->playerPos.y;
    }

    void Execute() {
        Entity nearestPlayer = u::GetNearestPlayer(g::server, 500);

        if(nearestPlayer.playerPos.x == 0.0f && nearestPlayer.playerPos.y == 0.0f) return;

        printf("Aiming at: %f, %f\n", nearestPlayer.playerPos.x, nearestPlayer.playerPos.y);

        // if(g::localPlayerEntity->selectedWeapon != Weapons::HAMMER && g::localPlayerEntity->selectedWeapon != Weapons::LASER) return;

        // printf("Aiming at: %f, %f\n", nearestPlayer.playerPos.x, nearestPlayer.playerPos.y);

        AimAt(nearestPlayer);
    }
}

namespace spinbot {
    bool enabled = false;
    bool autofireEnabled = false;
    float spinbotSpeed = 10.f;
    int spinbotRange = 100;

    void Execute() {
        if(spinbotSpeed == 0.0f) return;

        g::localPlayer->aimPosScreen.x = sin(spinbotSpeed * g::enlapsTime) * spinbotRange;
        g::localPlayer->aimPosScreen.y = cos(spinbotSpeed * g::enlapsTime) * spinbotRange;

        if(autofireEnabled) {
            g::localPlayer->localPlayerData.fire++;
        }
    }
}

DWORD WINAPI Trampoline() noexcept
{
    u::StartTrampoline("DPerX");

    g::Setup();

	while (!GetAsyncKeyState(VK_F6)) {

		std::this_thread::sleep_for(std::chrono::milliseconds(1));

        g::delta = 1.0f / 60.0f;
        g::enlapsTime += g::delta;

        GameStatus gameStatus = g::localPlayer->localPlayerData.gameStatus;
        if(
            gameStatus == GameStatus::MENU          || 
            gameStatus == GameStatus::CHAT          || 
            gameStatus == GameStatus::HOOKLINE_CHAT || 
            gameStatus == GameStatus::HOOKLINE_MENU   
        ) continue; 

        // printf("%d, %d\n", g::server->entities[g::server->idLocalPlayer].aimCoords.x, g::server->entities[g::server->idLocalPlayer].aimCoords.y);
        // printf("%f, %f\n", g::server->entities[1].playerPos.x, g::server->entities[1].playerPos.y);
        // printf("%d\n", static_cast<int32_t>(g::localPlayer->rightDir));

        // Stabilizer
        if(stabilizer::enabled && GetAsyncKeyState(VK_SPACE)) {
            if(g::server->onlinePlayers <= 1) break;
            stabilizer::Execute();
        } else if (stabilizer::enabled) {
            stabilizer::Reset();
        }

        // Aimbot
        if(aimbot::enabled && GetAsyncKeyState(VK_LCONTROL)) {
            if(g::server->onlinePlayers <= 1) break;
            aimbot::Execute();
        }

        // Spinbot
        if(spinbot::enabled) {
            spinbot::Execute();
        }

	}
    
    u::EndTrampoline();

    return FALSE;
}


int __stdcall DllMain(const HMODULE module, const std::uintptr_t reason, const void* reserved) {

	if (reason == DLL_PROCESS_ATTACH)
	{
        g::instance = module;

        DisableThreadLibraryCalls(g::instance);

        const auto thread = CreateThread(
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(Trampoline),
            nullptr,
            0,
            nullptr
        );

        if (thread) CloseHandle(thread);    
    }

	return TRUE;
}