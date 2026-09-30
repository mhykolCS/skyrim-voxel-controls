#pragma once
#include "voxel/core.hpp"
#include "voxel/control_policy.hpp"
#include <array>
#include <atomic>
#include <mutex>

namespace voxel {
struct RecipeView { bool available{},learned{}; std::vector<int> counts; };
struct Snapshot {
    bool enabled=true, active{}, debug{}, workbench{}, creativeArmed{}, melee=true;
    Mode mode=Mode::Survival;
    ControlMode controlState=ControlMode::Unavailable;
    Vec3 position{},velocity{};
    float health{},magicka{},stamina{},attackCharge=1;
    int camera{},selectedSpell{};
    std::string location="Main menu",status="VoxelControls ready",target;
    std::array<bool,3> spells{};
    std::vector<RecipeView> recipes;
};
struct Command {Action action;int value{};};
struct PointerInput {float dx{},dy{},wheel{};std::array<bool,3> buttons{};};
Snapshot readSnapshot();
void enqueue(Action action,int value=0);
PointerInput takePointerInput();
void startRuntime();
void resetRuntime();
void queueFrame();
bool installOverlay();
void updatePlayerModel(bool enabled,Mode mode,Vec3 velocity,double dt);
void resetPlayerModel();
void swingPlayerModel();
#ifdef VOXEL_PLAYTEST
extern std::atomic<bool> captureRequested;
#endif
}
