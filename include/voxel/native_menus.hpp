#pragma once
#include "voxel/inventory.hpp"

namespace voxel {
enum class MenuKind { None, Hub, Inventory, Container, Barter, Dialogue };
enum class MenuAction { Close, HubEntry, SelectItem, Transfer, Equip, Drop, Topic, Skip };
struct NativeItemView {
    ItemView item;
    bool player{},enabled{};
    int price{};
};
struct TopicView { int index{};std::string text;bool fresh{}; };
struct NativeMenuView {
    MenuKind kind=MenuKind::None;
    std::uint64_t session{};
    std::string title,subtitle,message,playerName;
    std::vector<NativeItemView> items;
    std::vector<TopicView> topics;
    ItemKey selected;
    int gold{},merchantGold{},level{},containerMode{},pickpocketChance=-1;
    float carryWeight{},carryLimit{};
    bool choicesReady{},canExit{},canSkip{},levelUp{};
};
struct MenuCommand {
    MenuAction action{};
    std::uint64_t session{};
    ItemKey item;
    int value{},price{};
    std::string text;
};
struct Snapshot;
void initNativeMenus();
void resetNativeMenus();
void updateNativeMenus(Snapshot& snapshot);
void enqueueMenu(MenuCommand command);
bool nativeMenuCapturesInput();
void nativeMenuKey(unsigned key);
void drawNativeMenus(const Snapshot& snapshot);
// Published by the renderer; E remains ordinary text while a search has focus.
bool menuTextFocused();
void setMenuTextFocused(bool value);
}
