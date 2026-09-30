#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <chrono>

namespace voxel {
namespace {
constexpr std::string_view menuName="VoxelInventory";
bool requested=false;
InventoryView cached;
using Clock=std::chrono::steady_clock;
Clock::time_point refreshed{};
struct Binding {std::uint32_t form{};std::string name;};
std::array<Binding,9> bindings;
class InventoryMenu final:public RE::IMenu {
public:
    InventoryMenu(){menuFlags.set(Flag::kPausesGame,Flag::kCustomRendering,Flag::kDisablePauseMenu);depthPriority=10;}
    static RE::IMenu* create(){return new InventoryMenu;}
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage&) override{return RE::UI_MESSAGE_RESULTS::kHandled;}
    void AdvanceMovie(float,std::uint32_t) override{}
    void PostDisplay() override{}
};
std::string effectsOf(RE::MagicItem* magic) {
    std::string result;
    if(magic)for(auto effect:magic->effects)if(effect&&effect->baseEffect) {
        const auto name=effect->baseEffect->GetName();if(!name||!*name)continue;
        if(!result.empty())result+='\n';result+=name;
        if(effect->effectItem.magnitude)result+="  "+std::to_string(int(effect->effectItem.magnitude));
        if(effect->effectItem.duration)result+=" / "+std::to_string(effect->effectItem.duration)+"s";
        if(result.size()>700)break;
    }
    return result;
}
ItemView describe(RE::PlayerCharacter* player,RE::TESBoundObject* object,RE::ExtraDataList* extra,int count) {
    RE::InventoryEntryData entry(object,count);if(extra)entry.AddExtraList(extra);
    ItemView item;item.key={object->GetFormID(),reinterpret_cast<std::uint64_t>(extra)};
    item.name=entry.GetDisplayName();item.count=count;item.weight=std::max(0.f,entry.GetWeight());item.value=entry.GetValue();
    item.equipped=entry.IsWorn();item.quest=entry.IsQuestObject();item.enchanted=entry.IsEnchanted();
    item.droppable=!item.quest;item.source=object->GetFile(0)?std::string(object->GetFile(0)->GetFilename()):"Skyrim";
    if(auto weapon=object->As<RE::TESObjectWEAP>()) {
        item.kind=ItemKind::Weapon;item.usable=true;item.equipmentSlot=entry.IsWorn(true)?7:6;
        item.damage=player->GetDamage(&entry);
        switch(weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kOneHandDagger:item.icon=ItemIcon::Dagger;break;
            case RE::WEAPON_TYPE::kOneHandAxe:case RE::WEAPON_TYPE::kTwoHandAxe:item.icon=ItemIcon::Axe;break;
            case RE::WEAPON_TYPE::kOneHandMace:item.icon=ItemIcon::Mace;break;
            case RE::WEAPON_TYPE::kBow:case RE::WEAPON_TYPE::kCrossbow:item.icon=ItemIcon::Bow;break;
            case RE::WEAPON_TYPE::kStaff:item.icon=ItemIcon::Staff;break;
            default:item.icon=ItemIcon::Sword;break;
        }
    } else if(auto armor=object->As<RE::TESObjectARMO>()) {
        item.kind=ItemKind::Armor;item.usable=true;item.armor=armor->GetArmorRating();
        const auto mask=armor->GetSlotMask();using Slot=RE::BGSBipedObjectForm::BipedObjectSlot;
        if(mask.any(Slot::kHead,Slot::kHair,Slot::kCirclet)){item.icon=ItemIcon::Helmet;item.equipmentSlot=0;}
        else if(mask.any(Slot::kBody)){item.icon=ItemIcon::Chest;item.equipmentSlot=1;}
        else if(mask.any(Slot::kHands)){item.icon=ItemIcon::Gloves;item.equipmentSlot=2;}
        else if(mask.any(Slot::kFeet)){item.icon=ItemIcon::Boots;item.equipmentSlot=3;}
        else if(mask.any(Slot::kAmulet)){item.icon=ItemIcon::Amulet;item.equipmentSlot=4;}
        else if(mask.any(Slot::kRing)){item.icon=ItemIcon::Ring;item.equipmentSlot=5;}
        else if(mask.any(Slot::kShield)){item.icon=ItemIcon::Shield;item.equipmentSlot=7;}
        else item.icon=ItemIcon::Chest;
    } else if(auto potion=object->As<RE::AlchemyItem>()) {
        item.kind=potion->IsFood()?ItemKind::Food:ItemKind::Potion;
        item.icon=potion->IsFood()?ItemIcon::Food:ItemIcon::Potion;
        item.usable=!potion->IsPoison();item.verb=potion->IsFood()?"Eat":"Drink";
        item.description=effectsOf(potion);
        const auto name=lowerText(item.name);item.tint=0xffbe5fbe;
        if(name.find("health")!=std::string::npos||name.find("healing")!=std::string::npos)item.tint=0xff6262d9;
        else if(name.find("magicka")!=std::string::npos)item.tint=0xffdf9958;
        else if(name.find("stamina")!=std::string::npos||potion->IsPoison())item.tint=0xff79bb6d;
        if(potion->IsPoison())item.description+="\nApply through the Skyrim inventory.";
    } else if(object->As<RE::IngredientItem>()) {
        item.kind=ItemKind::Ingredient;item.icon=ItemIcon::Leaf;item.usable=true;item.verb="Eat";item.description="Alchemy reagent. Eating reveals effects through Skyrim's alchemy system.";
    } else if(object->As<RE::TESObjectBOOK>()) {
        item.kind=ItemKind::Book;item.icon=ItemIcon::Book;item.usable=true;item.verb="Read";
    } else if(auto scroll=object->As<RE::ScrollItem>()) {
        item.kind=ItemKind::Scroll;item.icon=ItemIcon::Scroll;item.usable=true;item.description=effectsOf(scroll);
    } else if(object->As<RE::TESAmmo>()) {item.kind=ItemKind::Ammo;item.icon=ItemIcon::Arrow;item.usable=true;}
    else if(object->As<RE::TESKey>()) {item.kind=ItemKind::Key;item.icon=ItemIcon::Key;}
    else if(object->As<RE::TESSoulGem>()) {item.kind=ItemKind::SoulGem;item.icon=ItemIcon::Gem;}
    else if(object->As<RE::TESObjectLIGH>()) {item.icon=ItemIcon::Torch;item.usable=true;item.equipmentSlot=7;}
    else {
        const auto name=lowerText(item.name);
        if(object->GetFormID()==0xf)item.icon=ItemIcon::Coin;
        else if(object->GetFormID()==0xa)item.icon=ItemIcon::Lockpick;
        else if(name.find("ingot")!=std::string::npos||name.find("ore")!=std::string::npos){item.kind=ItemKind::Material;item.icon=ItemIcon::Ingot;}
        else if(name.find("gem")!=std::string::npos||name.find("diamond")!=std::string::npos||name.find("ruby")!=std::string::npos||name.find("sapphire")!=std::string::npos||name.find("amethyst")!=std::string::npos){item.kind=ItemKind::Material;item.icon=ItemIcon::Gem;}
    }
    if(item.verb.empty()&&item.usable)item.verb=item.equipped?"Unequip":"Equip";
    if(auto enchantment=entry.GetEnchantment()) {auto effect=effectsOf(enchantment);if(!effect.empty())item.description+=(item.description.empty()?"":"\n")+effect;}
    return item;
}
void refresh() {
    auto player=RE::PlayerCharacter::GetSingleton();if(!player)return;
    InventoryView view;view.playerName=player->GetName();view.level=player->GetLevel();
    if(auto race=player->GetRace())view.raceName=race->GetName();
    auto av=player->AsActorValueOwner();
    if(auto changes=player->GetInventoryChanges())view.carryWeight=changes->GetInventoryWeight();
    view.carryLimit=av->GetActorValue(RE::ActorValue::kCarryWeight);
    view.armor=av->GetActorValue(RE::ActorValue::kDamageResist);view.damage=player->GetEquippedWeaponsDamage();
    view.maxHealth=av->GetPermanentActorValue(RE::ActorValue::kHealth);view.maxMagicka=av->GetPermanentActorValue(RE::ActorValue::kMagicka);view.maxStamina=av->GetPermanentActorValue(RE::ActorValue::kStamina);
    for(auto& [object,data]:player->GetInventory()) {
        const auto [count,entry]=std::pair{data.first,data.second.get()};
        if(!object||!entry||count<=0||!object->GetPlayable())continue;
        if(object->GetFormID()==0xf)view.gold=count;
        int remaining=count;
        if(entry->extraLists)for(auto extra:*entry->extraLists)if(extra&&remaining>0) {
            int stack=std::clamp(extra->GetCount(),1,remaining);remaining-=stack;
            view.items.push_back(describe(player,object,extra,stack));
        }
        if(remaining>0)view.items.push_back(describe(player,object,nullptr,remaining));
    }
    for(const auto& item:view.items)if(item.equipped&&item.equipmentSlot>=0)view.equipment[item.equipmentSlot]=item.key;
    for(std::size_t i=0;i<bindings.size();++i)for(const auto& item:view.items)
        if(item.key.form==bindings[i].form&&item.name==bindings[i].name){view.quickSlots[i]=item.key;break;}
    cached=std::move(view);refreshed=Clock::now();
}
bool resolve(ItemKey key,RE::TESBoundObject*& object,RE::ExtraDataList*& extra) {
    auto player=RE::PlayerCharacter::GetSingleton();object=RE::TESForm::LookupByID<RE::TESBoundObject>(key.form);extra=nullptr;
    if(!player||!object)return false;
    auto inventory=player->GetInventory();const auto found=inventory.find(object);
    if(found==inventory.end()||found->second.first<=0||!found->second.second)return false;
    int plain=found->second.first;
    if(found->second.second->extraLists)for(auto candidate:*found->second.second->extraLists)if(candidate) {
        if(key.instance&&reinterpret_cast<std::uint64_t>(candidate)==key.instance){extra=candidate;return true;}
        plain-=std::max(1,candidate->GetCount());
    }
    return !key.instance&&plain>0;
}
void saveBindings(SKSE::SerializationInterface* serial) {
    if(!serial->OpenRecord(0x51424152,1))return;
    for(const auto& binding:bindings){serial->WriteRecordData(binding.form);std::uint32_t length=std::uint32_t(std::min<std::size_t>(binding.name.size(),512));serial->WriteRecordData(length);serial->WriteRecordData(binding.name.data(),length);}
}
void loadBindings(SKSE::SerializationInterface* serial) {
    bindings={};std::uint32_t type{},version{},length{};
    while(serial->GetNextRecordInfo(type,version,length))if(type==0x51424152&&version==1) {
        std::array<Binding,9> loaded;bool valid=true;
        for(auto& binding:loaded) {
            std::uint32_t form{},size{};
            if(serial->ReadRecordData(form)!=sizeof(form)||serial->ReadRecordData(size)!=sizeof(size)||size>512){valid=false;break;}
            binding.name.resize(size);if(serial->ReadRecordData(binding.name.data(),size)!=size){valid=false;break;}
            if(form)serial->ResolveFormID(form,binding.form);
        }
        if(valid)bindings=std::move(loaded);
    }
    refreshed={};
}
}
ItemView describeInventoryItem(RE::TESBoundObject* object,RE::ExtraDataList* extra,int count){return describe(RE::PlayerCharacter::GetSingleton(),object,extra,count);}
bool inventoryIsOpen(){return requested;}
bool inventoryOwnsPause() {
    const auto ui=RE::UI::GetSingleton();return requested&&ui&&ui->IsMenuOpen(menuName)&&ui->numPausesGame==1;
}
void setInventoryOpen(bool open) {
    if(open==requested)return;
    requested=open;refreshed={};
    if(auto map=RE::ControlMap::GetSingleton())map->AllowTextInput(open);
    if(auto queue=RE::UIMessageQueue::GetSingleton())queue->AddMessage(menuName,open?RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kHide,nullptr);
    spdlog::info("Inventory {}",open?"opened":"closed");
}
void resetInventory(){setInventoryOpen(false);cached={};bindings={};refreshed={};}
void updateInventoryView(Snapshot& snapshot) {
    if(Clock::now()-refreshed>std::chrono::milliseconds(250))refresh();
    snapshot.inventory=cached;
    auto player=RE::PlayerCharacter::GetSingleton();
    for(const auto& recipe:recipes()) {
        if(auto object=RE::TESForm::LookupByID<RE::TESBoundObject>(recipe.output)){auto item=describe(player,object,nullptr,0);if(recipe.spell)item.icon=ItemIcon::Scroll;snapshot.recipeItems.push_back(std::move(item));}
        else {ItemView item;item.key.form=recipe.output;item.name=recipe.name;item.icon=ItemIcon::Scroll;snapshot.recipeItems.push_back(std::move(item));}
        for(const auto& ingredient:recipe.inputs)if(auto object=RE::TESForm::LookupByID<RE::TESBoundObject>(ingredient.form))snapshot.recipeItems.push_back(describe(player,object,nullptr,0));
    }
}
std::string inventoryCommand(const Command& command) {
    if(!requested)return {};
    if(command.action==Action::NativeInventory) {
        setInventoryOpen(false);RE::UIMessageQueue::GetSingleton()->AddMessage(RE::InventoryMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kShow,nullptr);return {};
    }
    if(command.action==Action::PinItem&&command.value>=0&&command.value<9&&!command.item) {bindings[command.value]={};refreshed={};return "Quick slot cleared";}
    RE::TESBoundObject* object{};RE::ExtraDataList* extra{};
    if(!resolve(command.item,object,extra)){refreshed={};return "That item is no longer in your inventory";}
    auto player=RE::PlayerCharacter::GetSingleton();auto item=describe(player,object,extra,1);
    if(command.action==Action::PinItem&&command.value>=0&&command.value<9) {
        bindings[command.value]={item.key.form,item.name};refreshed={};return "Pinned "+item.name;
    }
    if(command.action==Action::DropItem) {
        // Re-check quest ownership on the live aggregate, as well as the chosen stack.
        auto inv=player->GetInventory();auto found=inv.find(object);
        if(item.quest||found==inv.end()||found->second.second->IsQuestObject())return "Quest items cannot be dropped";
        player->RemoveItem(object,1,RE::ITEM_REMOVE_REASON::kDropping,extra,nullptr);
        refreshed={};return "Dropped one "+item.name;
    }
    if(command.action==Action::UseItem&&item.usable) {
        if(auto book=object->As<RE::TESObjectBOOK>()){setInventoryOpen(false);RE::BookMenu::OpenMenuFromBaseForm(book);}
        else if(auto manager=RE::ActorEquipManager::GetSingleton()) {
            if(item.equipped)manager->UnequipObject(player,object,extra,1,nullptr,false,false,true,true);
            else manager->EquipObject(player,object,extra,1,nullptr,false,false,true,true);
        }
        refreshed={};return item.verb+": "+item.name;
    }
    return "This item has no direct use. See its recipe uses or open Skyrim inventory.";
}
void initInventory() {
    RE::UI::GetSingleton()->Register(menuName,InventoryMenu::create);
    auto serial=SKSE::GetSerializationInterface();serial->SetUniqueID(0x564F5849);serial->SetSaveCallback(saveBindings);serial->SetLoadCallback(loadBindings);
    serial->SetRevertCallback([](SKSE::SerializationInterface*){resetInventory();});
}
}
