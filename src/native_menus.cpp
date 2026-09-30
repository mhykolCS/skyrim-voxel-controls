#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <chrono>
#include <cmath>

namespace voxel {
namespace {
using Clock=std::chrono::steady_clock;
std::mutex menuMutex;
std::vector<MenuCommand> pending;
NativeMenuView published;
std::atomic<bool> captures{},textFocused{};
RE::GPtr<RE::IMenu> activeMenu;
RE::GFxValue activeRoot;
MenuKind activeKind=MenuKind::None;
std::uint64_t session{};
double savedAlpha=100;
bool textInputOwned=false;
ItemKey selected;
std::string message;
Clock::time_point refreshed{};
NativeMenuView cached;

RE::GFxValue member(const RE::GFxValue& object,const char* name) {
    RE::GFxValue value;if(object.IsObject())object.GetMember(name,&value);return value;
}
double number(const RE::GFxValue& object,const char* name,double fallback=0) {
    auto value=member(object,name);return value.IsNumber()?value.GetNumber():fallback;
}
bool boolean(const RE::GFxValue& object,const char* name,bool fallback=false) {
    auto value=member(object,name);return value.IsBool()?value.GetBool():value.IsNumber()?value.GetNumber()!=0:fallback;
}
std::string string(const RE::GFxValue& object,const char* name) {
    auto value=member(object,name);return value.IsString()?value.GetString():"";
}
RE::GFxValue rootOf(RE::IMenu* menu,MenuKind kind) {
    RE::GFxValue root;if(!menu||!menu->uiMovie)return root;
    if(kind==MenuKind::Dialogue)menu->uiMovie->GetVariable(&root,"_root.DialogueMenu_mc");
    else if(kind==MenuKind::Hub){
        menu->uiMovie->GetVariable(&root,"_root.TweenMenu_mc");
        if(!root.IsObject())menu->uiMovie->GetVariable(&root,"_root.Menu_mc");
    } else menu->uiMovie->GetVariable(&root,"_root.Menu_mc");
    return root;
}
RE::ItemList* listOf(RE::IMenu* menu,MenuKind kind) {
    if(!menu)return nullptr;
    if(kind==MenuKind::Container)return static_cast<RE::ContainerMenu*>(menu)->GetRuntimeData().itemList;
    if(kind==MenuKind::Barter)return static_cast<RE::BarterMenu*>(menu)->GetRuntimeData().itemList;
    if(kind==MenuKind::Inventory)return static_cast<RE::InventoryMenu*>(menu)->GetRuntimeData().itemList;
    return nullptr;
}
bool call(RE::IMenu* menu,const char* name,std::initializer_list<RE::GFxValue> args={}) {
    if(!menu||!menu->fxDelegate||!menu->uiMovie)return false;
    const auto callback=menu->fxDelegate->callbacks.GetAlt(name);
    if(!callback||!callback->callback)return false;
    // Same native callback and argument contract as Scaleform's GameDelegate.
    RE::FxDelegateArgs params(RE::GFxValue(0.0),callback->handler.get(),menu->uiMovie.get(),args.begin(),std::uint32_t(args.size()));
    callback->callback(params);return true;
}
void releaseMenu() {
    captures=false;
    if(activeRoot.IsObject())activeRoot.SetMember("_alpha",RE::GFxValue(savedAlpha));
    if(textInputOwned){RE::ControlMap::GetSingleton()->AllowTextInput(false);textInputOwned=false;}
    activeRoot.SetUndefined();activeMenu.reset();activeKind=MenuKind::None;selected={};refreshed={};cached={};message.clear();
}
bool obscured(RE::UI* ui) {
    for(auto name:{"Console","MessageBoxMenu","Book Menu","Journal Menu","MapMenu","StatsMenu","Training Menu","Crafting Menu","MagicMenu","Lockpicking Menu","Loading Menu","Tutorial Menu"})
        if(ui->IsMenuOpen(name))return true;
    return false;
}
std::pair<RE::GPtr<RE::IMenu>,MenuKind> candidate(RE::UI* ui) {
    if(obscured(ui))return {};
    for(auto [name,kind]:{std::pair{RE::BarterMenu::MENU_NAME,MenuKind::Barter},
        {RE::ContainerMenu::MENU_NAME,MenuKind::Container},{RE::InventoryMenu::MENU_NAME,MenuKind::Inventory},
        {RE::DialogueMenu::MENU_NAME,MenuKind::Dialogue},{RE::TweenMenu::MENU_NAME,MenuKind::Hub}})
        if(ui->IsMenuOpen(name))return {ui->GetMenu(name),kind};
    return {};
}
// StandardItemData is embedded with a derived vtable by the engine. Avoid
// compiler devirtualization to CommonLib declarations without implementations.
__declspec(noinline) bool itemEnabled(RE::StandardItemData* data){return data->GetEnabled();}
__declspec(noinline) unsigned itemCount(RE::StandardItemData* data){return data->GetCount();}
__declspec(noinline) const char* itemName(RE::StandardItemData* data){return data->GetName();}
int quote(const RE::InventoryEntryData* entry,bool player) {
    const auto mult=number(activeRoot,player?"fSellMult":"fBuyMult",-1);
    if(mult<0)return -1; // Unknown UI contract: no guessed merchant prices.
    const double base=entry->GetValue()*mult;
    return int(std::floor((player?base:std::max(1.0,base))+.5));
}
RE::GFxValue itemInfo() {
    return member(member(activeRoot,"ItemCard_mc"),"itemInfo");
}
bool selectItem(RE::ItemList* list,RE::ItemList::Item* item,int index) {
    if(!list||list->updatePending)return false;
    // The C++ menu resolves its selected item through this AS property.
    list->root.SetMember("selectedIndex",RE::GFxValue(double(index)));
    if(list->GetSelectedItem()!=item)return false;
    RE::GFxValue event;activeMenu->uiMovie->CreateObject(&event);event.SetMember("index",RE::GFxValue(double(index)));
    activeRoot.Invoke("onItemHighlightChange",nullptr,&event,1);
    call(activeMenu.get(),"UpdateItem3D",{RE::GFxValue(false)});
    selected={item->data.objDesc->object->GetFormID(),reinterpret_cast<std::uint64_t>(item)};
    return true;
}
void runCommand(const MenuCommand& command) {
    if(!activeMenu||command.session!=session||!captures)return;
    auto ui=RE::UI::GetSingleton();const auto [current,kind]=candidate(ui);
    if(current.get()!=activeMenu.get()||kind!=activeKind)return;
    if(command.action==MenuAction::Close) {
        if(kind==MenuKind::Dialogue){if(boolean(member(activeRoot,"ExitButton"),"_visible"))activeRoot.Invoke("onCancelPress");}
        else if(kind==MenuKind::Hub){if(!boolean(activeRoot,"bClosing")){activeRoot.SetMember("bClosing",RE::GFxValue(true));call(activeMenu.get(),"StartCloseMenu");activeRoot.Invoke("StartCloseMenuAnim");}}
        else call(activeMenu.get(),"CloseMenu");
        return;
    }
    if(kind==MenuKind::Hub&&command.action==MenuAction::HubEntry) {
        if(command.value>=1&&command.value<=4&&!boolean(activeRoot,"bClosing")) {
            RE::GFxValue value(double(command.value));activeRoot.Invoke("onInputRectClick",nullptr,&value,1);
        }
        return;
    }
    if(kind==MenuKind::Dialogue) {
        if(command.action==MenuAction::Skip){activeRoot.Invoke("SkipText");return;}
        if(command.action!=MenuAction::Topic||number(activeRoot,"eMenuState")!=1||!boolean(activeRoot,"bAllowProgress"))return;
        auto topics=member(activeRoot,"TopicList"),entries=member(topics,"entryList");
        if(!entries.IsArray())return;
        for(unsigned i=0;i<entries.GetArraySize();++i) {
            RE::GFxValue entry;entries.GetElement(i,&entry);
            if(int(number(entry,"topicIndex",-1))!=command.value||string(entry,"text")!=command.text)continue;
            topics.SetMember("selectedIndex",RE::GFxValue(double(i)));
            auto chosen=member(topics,"selectedEntry");
            if(int(number(chosen,"topicIndex",-1))!=command.value||string(chosen,"text")!=command.text)return;
            RE::GFxValue event;activeMenu->uiMovie->CreateObject(&event);event.SetMember("keyboardOrMouse",RE::GFxValue(1.0));
            activeRoot.Invoke("onItemSelect",nullptr,&event,1);return;
        }
        return;
    }
    auto list=listOf(activeMenu.get(),kind);if(!list||list->updatePending)return;
    RE::ItemList::Item* item{};int index{};
    for(unsigned i=0;i<list->items.size();++i) {
        auto candidateItem=list->items[i];
        if(candidateItem&&reinterpret_cast<std::uint64_t>(candidateItem)==command.item.instance&&candidateItem->data.objDesc&&
            candidateItem->data.objDesc->object&&candidateItem->data.objDesc->object->GetFormID()==command.item.form){item=candidateItem;index=int(i);break;}
    }
    if(!item||!selectItem(list,item,index)) {message="The item list changed. Select the item again.";return;}
    if(command.action==MenuAction::SelectItem)return;
    if(!itemEnabled(&item->data)){message="Skyrim has disabled this item.";return;}
    const bool playerItem=kind==MenuKind::Inventory||RE::TESObjectREFR::LookupByHandle(item->data.owner).get()==RE::PlayerCharacter::GetSingleton();
    const int count=int(itemCount(&item->data));
    if(count<=0||command.value<1||command.value>count)return;
    auto entry=item->data.objDesc;
    if((command.action==MenuAction::Transfer||command.action==MenuAction::Drop)&&playerItem&&entry->IsQuestObject()){message="Quest item - cannot be transferred.";return;}
    if(command.action==MenuAction::Transfer&&kind==MenuKind::Container) {
        call(activeMenu.get(),"ItemTransfer",{RE::GFxValue(double(command.value)),RE::GFxValue(!playerItem)});
        message=playerItem?"Stored in container":"Taken from container";
    } else if(command.action==MenuAction::Transfer&&kind==MenuKind::Barter) {
        const int price=quote(entry,playerItem);
        auto& data=static_cast<RE::BarterMenu*>(activeMenu.get())->GetRuntimeData();
        if(price<0||price!=command.price){message="The price changed. Review it before trading.";return;}
        const std::int64_t total=std::int64_t(price)*command.value;
        if(total>(playerItem?data.merchantGold:data.playerGold)){message=playerItem?"The merchant cannot afford that quantity":"Not enough gold";return;}
        call(activeMenu.get(),"ItemSelect",{RE::GFxValue(double(command.value)),RE::GFxValue(double(price)),RE::GFxValue(!playerItem)});
        message=playerItem?"Sold to merchant":"Bought from merchant";
    } else if(kind==MenuKind::Inventory&&command.action==MenuAction::Equip) {
        call(activeMenu.get(),"ItemSelect");
    } else if(kind==MenuKind::Inventory&&command.action==MenuAction::Drop) {
        call(activeMenu.get(),"ItemDrop",{RE::GFxValue(double(command.value))});
    }
    refreshed={};
}
NativeMenuView readView() {
    NativeMenuView view;view.kind=activeKind;view.session=session;view.selected=selected;view.message=message;
    auto player=RE::PlayerCharacter::GetSingleton();view.playerName=player->GetName();view.level=player->GetLevel();
    view.carryLimit=player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
    if(auto changes=player->GetInventoryChanges())view.carryWeight=changes->GetInventoryWeight();
    view.gold=player->GetGoldAmount();view.canExit=true;
    if(activeKind==MenuKind::Hub){view.title="Adventure menu";view.levelUp=boolean(activeRoot,"bLevelUp");return view;}
    if(activeKind==MenuKind::Dialogue) {
        view.title=string(member(activeRoot,"SpeakerName"),"text");view.subtitle=string(member(activeRoot,"SubtitleText"),"text");
        view.choicesReady=number(activeRoot,"eMenuState")==1&&boolean(activeRoot,"bAllowProgress");
        view.canExit=boolean(member(activeRoot,"ExitButton"),"_visible");view.canSkip=boolean(activeRoot,"bAllowProgress")&&!view.choicesReady;
        auto entries=member(member(activeRoot,"TopicList"),"entryList");
        if(entries.IsArray())for(unsigned i=0;i<entries.GetArraySize();++i){RE::GFxValue entry;entries.GetElement(i,&entry);view.topics.push_back({int(number(entry,"topicIndex",-1)),string(entry,"text"),boolean(entry,"topicIsNew")});}
        return view;
    }
    view.title="Inventory";
    if(activeKind==MenuKind::Container){view.containerMode=int(RE::ContainerMenu::GetContainerMode());if(auto target=RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle()))view.title=target->GetName();}
    if(activeKind==MenuKind::Barter){auto& data=static_cast<RE::BarterMenu*>(activeMenu.get())->GetRuntimeData();view.gold=data.playerGold;view.merchantGold=data.merchantGold;if(auto target=RE::TESObjectREFR::LookupByHandle(RE::BarterMenu::GetTargetRefHandle()))view.title=target->GetName();}
    auto list=listOf(activeMenu.get(),activeKind);if(!list||list->updatePending)return view;
    for(auto native:list->items) {
        if(!native||!native->data.objDesc||!native->data.objDesc->object)continue;
        auto entry=native->data.objDesc;auto object=entry->object;const int count=int(itemCount(&native->data));if(count<=0)continue;
        RE::ExtraDataList* extra{};if(entry->extraLists&&!entry->extraLists->empty())extra=entry->extraLists->front();
        NativeItemView item;item.item=describeInventoryItem(object,extra,count);item.item.key.instance=reinterpret_cast<std::uint64_t>(native);
        if(auto name=itemName(&native->data))item.item.name=name;
        item.item.quest=entry->IsQuestObject();item.item.droppable=!item.item.quest;
        item.player=activeKind==MenuKind::Inventory||RE::TESObjectREFR::LookupByHandle(native->data.owner).get()==player;
        item.enabled=itemEnabled(&native->data);item.price=activeKind==MenuKind::Barter?quote(entry,item.player):item.item.value;
        view.items.push_back(std::move(item));
    }
    view.pickpocketChance=int(number(itemInfo(),"pickpocketChance",-1));
    return view;
}
using MenuInput=RE::BSEventNotifyControl(*)(RE::MenuControls*,RE::InputEvent* const*,RE::BSTEventSource<RE::InputEvent*>*);
MenuInput originalInput{};
RE::BSEventNotifyControl filterMenuInput(RE::MenuControls* self,RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>* source) {
    if((captures||inventoryIsOpen())&&!obscured(RE::UI::GetSingleton())) {
        // Do not deliver mouse/keyboard to the hidden native movie as well.
        // The input event source continues to our own sink and key-up filters.
        return RE::BSEventNotifyControl::kContinue;
    }
    return originalInput(self,events,source);
}
}
bool menuTextFocused(){return textFocused.load();}
void setMenuTextFocused(bool value){textFocused=value;}
bool nativeMenuCapturesInput(){return captures.load();}
void enqueueMenu(MenuCommand command){std::lock_guard lock(menuMutex);pending.push_back(std::move(command));}
void nativeMenuKey(unsigned key) {
    NativeMenuView view;{std::lock_guard lock(menuMutex);view=published;}
    if(key==0x01||key==0x0F||((view.kind==MenuKind::Inventory||view.kind==MenuKind::Container||view.kind==MenuKind::Barter)&&key==0x12))enqueueMenu({MenuAction::Close,view.session});
    else if(view.kind==MenuKind::Dialogue&&key>=2&&key<=10&&key-2<view.topics.size()&&view.choicesReady){auto& topic=view.topics[key-2];enqueueMenu({MenuAction::Topic,view.session,{},topic.index,0,topic.text});}
    else if(view.kind==MenuKind::Dialogue&&(key==0x39||key==0x1C)&&view.canSkip)enqueueMenu({MenuAction::Skip,view.session});
    else if(view.kind==MenuKind::Hub){int entry=key==0xC8?1:key==0xCB?2:(key==0xCD||key==0x12)?3:key==0xD0?4:0;if(entry)enqueueMenu({MenuAction::HubEntry,view.session,{},entry});}
}
void updateNativeMenus(Snapshot& snapshot) {
    auto ui=RE::UI::GetSingleton();
    auto [menu,kind]=snapshot.enabled&&!snapshot.inventoryOpen&&ui?candidate(ui):std::pair<RE::GPtr<RE::IMenu>,MenuKind>{};
    if(menu.get()!=activeMenu.get()||kind!=activeKind) {
        releaseMenu();
        auto root=rootOf(menu.get(),kind);bool supported=root.IsObject();
        if(supported&&(kind==MenuKind::Inventory||kind==MenuKind::Container||kind==MenuKind::Barter))supported=listOf(menu.get(),kind)&&root.HasMember("ItemCard_mc");
        if(supported){activeMenu=menu;activeRoot=root;activeKind=kind;++session;savedAlpha=number(root,"_alpha",100);captures=true;
            if(kind!=MenuKind::Dialogue){RE::ControlMap::GetSingleton()->AllowTextInput(true);textInputOwned=true;}
            spdlog::info("Themed native menu opened kind={} session={}",int(kind),session);
        }
    }
    if(activeMenu){
        activeRoot.SetMember("_alpha",RE::GFxValue(0.0));
        std::vector<MenuCommand> commands;{std::lock_guard lock(menuMutex);commands.swap(pending);}
        for(const auto& command:commands)runCommand(command);
        // Dialogue timing is sampled every frame; item grids are cached briefly.
        if(activeKind==MenuKind::Dialogue||Clock::now()-refreshed>std::chrono::milliseconds(150)){cached=readView();refreshed=Clock::now();}
        snapshot.nativeMenu=cached;
    } else {std::lock_guard lock(menuMutex);pending.clear();}
    {std::lock_guard lock(menuMutex);published=snapshot.nativeMenu;}
}
void resetNativeMenus(){releaseMenu();std::lock_guard lock(menuMutex);pending.clear();published={};textFocused=false;}
void initNativeMenus() {
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_MenuControls[0]};
    originalInput=reinterpret_cast<MenuInput>(table.write_vfunc(1,filterMenuInput));
}
}
