#include "voxel/inventory.hpp"
#include <cstdlib>
#include <iostream>
using namespace voxel;
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    InventoryView view;
    ItemView sword;sword.key={0x12,10};sword.name="Iron Sword";sword.source="Skyrim.esm";sword.kind=ItemKind::Weapon;sword.weight=9;sword.value=25;
    ItemView enchanted=sword;enchanted.key.instance=11;enchanted.name="Iron Sword of Sparks";enchanted.value=300;
    ItemView potion;potion.key={0x13,0};potion.name="Potion of Healing";potion.source="Alchemy.esp";potion.kind=ItemKind::Potion;potion.weight=.5;potion.value=50;
    view.items={sword,enchanted,potion};
    check(findItem(view,{0x12,11})->name==enchanted.name,"duplicate base forms must keep their individual stack identity");
    check(!findItem(view,{0x12,12}),"stale stack selection must not resolve to a different enchanted item");
    auto matches=inventoryIndices(view.items,ItemFilter::All,"IRON",ItemSort::Value);
    check(matches==std::vector<std::size_t>{1,0},"search is case insensitive and value sorting preserves identities");
    matches=inventoryIndices(view.items,ItemFilter::Supplies,"@alchemy",ItemSort::Name);
    check(matches==std::vector<std::size_t>{2},"source search combines with category filters");
    check(inventoryIndices(view.items,ItemFilter::Gear,"Healing",ItemSort::Name).empty(),"empty results are supported");
    check(inventoryPageCount(0)==1&&inventoryPageCount(27)==1&&inventoryPageCount(28)==2&&inventoryPageCount(1000)==38,"pagination does not hide overflow inventories");
    check(view.items[0].key==sword.key&&view.items[1].key==enchanted.key,"sorting does not mutate game inventory order");
    std::cout<<"Inventory query and identity regressions passed\n";
}
