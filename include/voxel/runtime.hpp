#pragma once
#include "voxel/core.hpp"
#include "voxel/control_policy.hpp"
#include "voxel/inventory.hpp"
#include <array>
#include <atomic>
#include <mutex>

namespace voxel {
struct RecipeView { bool available{},learned{}; std::vector<int> counts; };
struct Snapshot {
    bool enabled=true, active{}, debug{}, workbench{}, inventoryOpen{}, melee=true;
    Mode mode=Mode::Survival;
    ControlMode controlState=ControlMode::Unavailable;
    Vec3 position{},velocity{};
    float health{},magicka{},stamina{},attackCharge=1;
    float flightFov=1;
    int camera{},selectedSpell{};
    std::string location="Main menu",status="VoxelControls ready",target,controlReason;
    std::array<bool,3> spells{};
    std::vector<RecipeView> recipes;
    InventoryView inventory;
    std::vector<ItemView> recipeItems;
};
struct Command {Action action;int value{};ItemKey item;};
struct PointerInput {float dx{},dy{},wheel{};std::array<bool,3> buttons{};std::vector<std::pair<unsigned,bool>> keyboard;std::vector<unsigned> characters;};
Snapshot readSnapshot();
void enqueue(Action action,int value=0,ItemKey item={});
PointerInput takePointerInput();
void startRuntime();
void resetRuntime();
void queueFrame();
bool installOverlay();
void updatePlayerModel(bool enabled,Mode mode,Vec3 velocity,double dt);
void resetPlayerModel();
void swingPlayerModel();
void initInventory();
void resetInventory();
void setInventoryOpen(bool open);
bool inventoryIsOpen();
bool inventoryOwnsPause();
void updateInventoryView(Snapshot& snapshot);
std::string inventoryCommand(const Command& command);
void drawInventory(const Snapshot& snapshot,void* device);
#ifdef VOXEL_PLAYTEST
extern std::atomic<bool> captureRequested;
#endif
}
