#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace voxel {
struct ItemKey {
    std::uint32_t form{};
    std::uint64_t instance{};
    bool operator==(const ItemKey&) const = default;
    explicit operator bool() const {return form!=0;}
};
enum class ItemKind { Weapon, Armor, Potion, Food, Ingredient, Book, Scroll, Ammo, Key, Material, SoulGem, Misc };
enum class ItemIcon { Sword,Axe,Mace,Dagger,Bow,Staff,Helmet,Chest,Gloves,Boots,Shield,Ring,Amulet,Potion,Food,Leaf,Book,Scroll,Arrow,Key,Ingot,Gem,Coin,Lockpick,Torch,Bag };
struct ItemView {
    ItemKey key;
    std::string name,source,description,verb;
    ItemKind kind=ItemKind::Misc;
    ItemIcon icon=ItemIcon::Bag;
    int count{},value{},equipmentSlot=-1;
    float weight{},damage{},armor{};
    bool equipped{},enchanted{},quest{},usable{},droppable{};
    std::uint32_t tint=0xffd4d4d4;
};
struct InventoryView {
    std::vector<ItemView> items;
    std::array<ItemKey,8> equipment{};
    std::array<ItemKey,9> quickSlots{};
    std::string playerName,raceName;
    int level{},gold{};
    float carryWeight{},carryLimit{},armor{},damage{},maxHealth{},maxMagicka{},maxStamina{};
};
enum class ItemFilter { All,Gear,Supplies,Books,Materials,Keys };
enum class ItemSort { Name,Weight,Value };
inline std::string lowerText(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return char(std::tolower(c));});
    return result;
}
inline bool itemMatches(const ItemView& item,ItemFilter filter,std::string_view query) {
    const auto kind=item.kind;
    const bool category=filter==ItemFilter::All||
        (filter==ItemFilter::Gear&&(kind==ItemKind::Weapon||kind==ItemKind::Armor||kind==ItemKind::Ammo))||
        (filter==ItemFilter::Supplies&&(kind==ItemKind::Potion||kind==ItemKind::Food||kind==ItemKind::Ingredient))||
        (filter==ItemFilter::Books&&(kind==ItemKind::Book||kind==ItemKind::Scroll))||
        (filter==ItemFilter::Materials&&(kind==ItemKind::Material||kind==ItemKind::SoulGem||kind==ItemKind::Misc))||
        (filter==ItemFilter::Keys&&kind==ItemKind::Key);
    if(!category)return false;
    const auto text=lowerText(query);
    if(text.starts_with('@'))return lowerText(item.source).find(text.substr(1))!=std::string::npos;
    return lowerText(item.name).find(text)!=std::string::npos;
}
inline std::vector<std::size_t> inventoryIndices(const std::vector<ItemView>& items,ItemFilter filter,std::string_view query,ItemSort sort) {
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<items.size();++i)if(itemMatches(items[i],filter,query))result.push_back(i);
    std::stable_sort(result.begin(),result.end(),[&](auto a,auto b){
        if(sort==ItemSort::Weight&&items[a].weight!=items[b].weight)return items[a].weight>items[b].weight;
        if(sort==ItemSort::Value&&items[a].value!=items[b].value)return items[a].value>items[b].value;
        return lowerText(items[a].name)<lowerText(items[b].name);
    });
    return result;
}
inline const ItemView* findItem(const InventoryView& view,ItemKey key) {
    auto found=std::find_if(view.items.begin(),view.items.end(),[key](const auto& item){return item.key==key;});
    return found==view.items.end()?nullptr:&*found;
}
inline std::size_t inventoryPageCount(std::size_t size) {return std::max<std::size_t>(1,(size+26)/27);}
}
