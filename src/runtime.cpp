#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <chrono>
#include <numbers>
#ifdef VOXEL_PLAYTEST
#include <fstream>
#include <sstream>
#endif

namespace voxel {
#ifdef VOXEL_PLAYTEST
std::atomic<bool> captureRequested=false;
#endif
namespace {
using Clock=std::chrono::steady_clock;
using Flag=RE::UserEvents::USER_EVENT_FLAG;
std::mutex sharedMutex;
Snapshot snapshot;
PointerInput pointer;
std::vector<Command> commands;
std::array<bool,256> keys{};
std::atomic<bool> frameQueued=false;
Movement movement;
Combat combat;
Clock::time_point lastFrame=Clock::now(),lastJump{},lastCast{};
RE::bhkCharacterController* ownedController{};
float savedGravity{};
std::uint32_t ownedFlags{};
std::uint32_t pendingRestoreFlags{};
bool enabled=true,debug=false,workbench=false,creativeArmed=false;
bool requestWorkbench=false;
int cameraMode=0,selectedSpell=0;
bool cameraOwned=false;
float savedZoom{};
RE::NiPoint3 savedCameraOffset{};
std::string status="VoxelControls ready";
std::vector<std::pair<RE::BSFixedString,std::uint16_t>> savedMappings;
std::mutex physicsMutex;
RE::bhkCharacterController* physicsPlayer{};
RE::hkVector4 desiredVelocity{};
using VelocitySetter=void(*)(RE::bhkCharacterController*,const RE::hkVector4&);
VelocitySetter proxyVelocity{},rigidVelocity{};
std::atomic<std::uint64_t> physicsCalls{};

void applyVelocity(RE::bhkCharacterController* controller,const RE::hkVector4& requested,VelocitySetter original) {
    RE::hkVector4 velocity=requested;
    {
        std::lock_guard lock(physicsMutex);
        if(controller==physicsPlayer){velocity=desiredVelocity;controller->outVelocity=velocity;++physicsCalls;}
    }
    original(controller,velocity);
}
void proxySetVelocity(RE::bhkCharacterController* controller,const RE::hkVector4& velocity){applyVelocity(controller,velocity,proxyVelocity);}
void rigidSetVelocity(RE::bhkCharacterController* controller,const RE::hkVector4& velocity){applyVelocity(controller,velocity,rigidVelocity);}
void installPhysics() {
    // bhkCharacterController::SetLinearVelocityImpl is slot 7. The proxy's
    // controller is its second base; its listener is the first vtable.
    REL::Relocation<std::uintptr_t> proxy{RE::VTABLE_bhkCharProxyController[1]};
    REL::Relocation<std::uintptr_t> rigid{RE::VTABLE_bhkCharRigidBodyController[0]};
    proxyVelocity=reinterpret_cast<VelocitySetter>(proxy.write_vfunc(7,proxySetVelocity));
    rigidVelocity=reinterpret_cast<VelocitySetter>(rigid.write_vfunc(7,rigidSetVelocity));
    spdlog::info("Physics velocity adapters installed");
}

Vec3 unpack(const RE::hkVector4& vector) {
    alignas(16) float values[4];_mm_store_ps(values,vector.quad);
    return {values[0],values[1],values[2]};
}
RE::hkVector4 pack(Vec3 v) {return {float(v.x),float(v.y),float(v.z),0};}
void notify(std::string text) {status=std::move(text);RE::SendHUDMessage::ShowHUDMessage(status.c_str());spdlog::info("{}",status);}
void controls(std::uint32_t desired) {
    auto map=RE::ControlMap::GetSingleton();if(!map)return;
    std::uint32_t current,stored;map->GetControlsState(current,stored);
    auto release=ownedFlags&~desired;
    if(release) map->ToggleControls(static_cast<Flag>(release),true,false);
    ownedFlags&=desired;
    auto acquire=desired&current&~ownedFlags;
    if(acquire) {map->ToggleControls(static_cast<Flag>(acquire),false,false);ownedFlags|=acquire;}
}
void releasePhysics(RE::PlayerCharacter* player,bool preserveMode=false) {
    const auto mode=movement.state.mode;const auto velocity=movement.state.velocity;
    {std::lock_guard lock(physicsMutex);physicsPlayer=nullptr;}
    if(ownedController&&player&&player->GetCharController()==ownedController) ownedController->gravity=savedGravity;
    ownedController=nullptr;controls(0);movement.reset();
    if(preserveMode){movement.setMode(mode);movement.state.velocity=velocity;}
}
void reserveKeys(bool reserve) {
    auto map=RE::ControlMap::GetSingleton();
    if(!map||!map->controlMap[0])return;
    auto& mappings=map->controlMap[0]->deviceMappings[0];
    if(reserve&&!savedMappings.empty())return;
    for(auto& mapping:mappings) {
        if(reserve) {
            // F5 is Skyrim's default quicksave. Reserve our keys only in memory.
            if(mapping.inputKey==0x3F||mapping.inputKey==0x40||mapping.inputKey==0x41||mapping.inputKey==0x42||
               (mapping.inputKey>=2&&mapping.inputKey<=4)) {
                savedMappings.emplace_back(mapping.eventID,mapping.inputKey);mapping.inputKey=0xFF;
            }
        } else {
            for(auto& [event,key]:savedMappings) if(mapping.eventID==event&&mapping.inputKey==0xFF) mapping.inputKey=key;
        }
    }
    if(!reserve)savedMappings.clear();
}
RE::ThirdPersonState* thirdPerson() {
    auto camera=RE::PlayerCamera::GetSingleton();if(!camera)return nullptr;
    return static_cast<RE::ThirdPersonState*>(camera->GetRuntimeData().cameraStates[RE::CameraState::kThirdPerson].get());
}
void setCamera(int mode) {
    auto camera=RE::PlayerCamera::GetSingleton();if(!camera)return;
    if(auto third=thirdPerson()){third->freeRotationEnabled=false;third->freeRotation={0,0};}
    if(mode==0)camera->ForceFirstPerson();else camera->ForceThirdPerson();
    if(mode!=0)if(auto third=thirdPerson()) {
        if(!cameraOwned){savedZoom=third->targetZoomOffset;savedCameraOffset=third->posOffsetExpected;cameraOwned=true;}
        third->targetZoomOffset=third->currentZoomOffset=third->savedZoomOffset=.8f;
        third->posOffsetExpected=third->posOffsetActual={0,0,-20};
    }
    cameraMode=mode;
    notify(mode==0?"Camera: first person":mode==1?"Camera: third person":"Camera: front view");
}
void restoreCamera() {
    if(auto third=thirdPerson()) {
        third->freeRotationEnabled=false;third->freeRotation={0,0};
        if(cameraOwned){third->targetZoomOffset=third->currentZoomOffset=third->savedZoomOffset=savedZoom;third->posOffsetExpected=third->posOffsetActual=savedCameraOffset;}
    }
    cameraOwned=false;
    if(cameraMode==2)cameraMode=1;
}
Inventory inventoryOf(RE::PlayerCharacter* player) {
    Inventory inv;
    for(auto& [object,count]:player->GetInventoryCounts())if(object&&count>0)inv[object->GetFormID()]=count;
    return inv;
}
RE::SpellItem* spellAt(int slot) {
    constexpr std::uint32_t ids[]{0x12FD0,0x2B96C,0x2DD29};
    if(slot<0||slot>2)return nullptr;
    return RE::TESForm::LookupByID<RE::SpellItem>(ids[slot]);
}
void craft(RE::PlayerCharacter* player,int index) {
    if(index<0||index>=int(recipes().size()))return;
    const auto& recipe=recipes()[index];
    auto spell=recipe.spell?RE::TESForm::LookupByID<RE::SpellItem>(recipe.output):nullptr;
    auto output=recipe.spell?nullptr:RE::TESForm::LookupByID<RE::TESBoundObject>(recipe.output);
    if((recipe.spell&&!spell)||(!recipe.spell&&!output)){notify("Recipe output unavailable");return;}
    if(!canCraft(recipe,inventoryOf(player),spell&&player->HasSpell(spell))){notify("Missing ingredients or matrix already learned");return;}
    for(auto [id,count]:requirements(recipe))if(!RE::TESForm::LookupByID<RE::TESBoundObject>(id)){notify("Ingredient unavailable");return;}
    // One game-thread transaction: validate every input before removing any item.
    if(spell&&!player->AddSpell(spell)){notify("Could not learn this matrix");return;}
    for(auto [id,count]:requirements(recipe))player->RemoveItem(RE::TESForm::LookupByID<RE::TESBoundObject>(id),count,RE::ITEM_REMOVE_REASON::kRemove,nullptr,nullptr);
    if(output)player->AddObjectToContainer(output,nullptr,recipe.count,nullptr);
    notify("Created "+recipe.name);
}
bool meleeWeapon(RE::PlayerCharacter* player) {
    auto object=player->GetEquippedObject(false);
    if(!object)return true;
    auto weapon=object->As<RE::TESObjectWEAP>();
    if(!weapon)return false;
    auto type=weapon->GetWeaponType();
    return type!=RE::WEAPON_TYPE::kBow&&type!=RE::WEAPON_TYPE::kCrossbow&&type!=RE::WEAPON_TYPE::kStaff;
}
void attack(RE::PlayerCharacter* player) {
    if(!meleeWeapon(player)||combat.elapsed<0.1)return;
    swingPlayerModel();
    double damage=4;
    if(auto object=player->GetEquippedObject(false))if(auto weapon=object->As<RE::TESObjectWEAP>())damage=weapon->GetAttackDamage();
    auto hit=combat.strike(damage,movement.state.velocity.z<-0.5,keys[0x1D]);
    auto crosshair=RE::CrosshairPickData::GetSingleton();if(!crosshair)return;
    auto reference=crosshair->GetActiveTarget().get();if(!reference)return;
    auto victim=reference->As<RE::Actor>();if(!victim||victim==player||victim->IsDead())return;
    auto delta=victim->GetPosition()-player->GetPosition();
    const float scale=RE::bhkWorld::GetWorldScale();
    if(delta.Length()*scale>3.0f)return;
    // Native damage attribution/death handling; native melee input is suppressed.
    float armor=std::max(0.0f,victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDamageResist));
    float actual=float(hit.damage)*(1.0f-std::min(0.8f,armor*0.0012f));
    const auto healthBefore=victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
    victim->DoDamage(actual,player,true);
    const auto healthAfter=victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
    victim->StartCombat(player);
    if(auto controller=victim->GetCharController()) {
        RE::hkVector4 current;controller->GetLinearVelocityImpl(current);
        Vec3 velocity=unpack(current);double length=std::hypot(delta.x,delta.y);
        if(length>0.01){velocity.x+=delta.x/length*hit.knockback;velocity.y+=delta.y/length*hit.knockback;velocity.z=std::max(velocity.z,2.5);controller->SetLinearVelocityImpl(pack(velocity));}
    }
    status=(hit.critical?"Critical hit: ":"Hit: ")+std::to_string(int(actual))+" damage";
    spdlog::info("Combat target {:08X}, damage {:.2f}, charge {:.2f}, health {:.2f} -> {:.2f}",victim->GetFormID(),actual,hit.charge,healthBefore,healthAfter);
}
void cast(RE::PlayerCharacter* player) {
    auto now=Clock::now();if(now-lastCast<std::chrono::milliseconds(650))return;
    auto spell=spellAt(selectedSpell);if(!spell||!player->HasSpell(spell)){notify("Learn this matrix in the workbench first");return;}
    const float cost=spell->CalculateMagickaCost(player);
    auto values=player->AsActorValueOwner();
    if(values->GetActorValue(RE::ActorValue::kMagicka)<cost){notify("Insufficient magicka");return;}
    auto caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);if(!caster)return;
    values->DamageActorValue(RE::ActorValue::kMagicka,cost);
    caster->CastSpellImmediate(spell,false,nullptr,1,false,0,player);
    lastCast=now;status="Cast "+std::string(spell->GetName());
    spdlog::info("Cast matrix {:08X} at cost {:.1f}",spell->GetFormID(),cost);
}
#ifdef VOXEL_PLAYTEST
void playtest() {
    static auto lastPoll=Clock::now();static unsigned heldKey=0;static auto releaseKey=Clock::now();
    if(heldKey&&Clock::now()>=releaseKey){keys[heldKey]=false;heldKey=0;}
    if(Clock::now()-lastPoll<std::chrono::milliseconds(200))return;
    lastPoll=Clock::now();
    const auto path="Data/SKSE/Plugins/VoxelControls.playtest.txt";
    std::ifstream file(path);if(!file)return;
    std::string line;std::getline(file,line);file.close();std::filesystem::remove(path);
    std::istringstream input(line);std::string op;input>>op;
    auto player=RE::PlayerCharacter::GetSingleton();
    static RE::NiPointer<RE::Actor> target;
    spdlog::info("PLAYTEST {}",line);
    if(op=="console") {
        std::string command;std::getline(input>>std::ws,command);
        auto factory=RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
        if(factory)if(auto script=factory->Create()){script->SetCommand(command);script->CompileAndRun(player);delete script;}
    } else if(op=="action") {
        std::string name;int value{};input>>name>>value;
        const std::map<std::string,Action> actions{{"debug",Action::ToggleDebug},{"camera",Action::Camera},{"creative",Action::Creative},{"glide",Action::Glide},{"workbench",Action::Workbench},{"craft",Action::Craft},{"attack",Action::Attack},{"cast",Action::Cast},{"enable",Action::ToggleEnabled}};
        if(auto found=actions.find(name);found!=actions.end())enqueue(found->second,value);
    } else if(op=="hold") {unsigned key;double duration;input>>std::hex>>key>>std::dec>>duration;if(key>0&&key<keys.size()&&duration>0&&duration<=10){heldKey=key;keys[key]=true;releaseKey=Clock::now()+std::chrono::milliseconds(int(duration*1000));}}
    else if(op=="spawn"&&player&&player->GetParentCell()) {
        if(auto base=RE::TESForm::LookupByID<RE::TESBoundObject>(0x1BCD8)) {
            auto reference=player->PlaceObjectAtMe(base,false);
            if(reference){target.reset(reference->As<RE::Actor>());if(target){target->EnableAI(false);auto pos=player->GetPosition();auto yaw=player->GetAngleZ();target->SetPosition({pos.x+std::sin(yaw)*120,pos.y+std::cos(yaw)*120,pos.z},true);spdlog::info("PLAYTEST spawned {:08X}",target->GetFormID());}}
        }
    } else if(op=="aim"&&target){auto delta=target->GetPosition()-player->GetPosition();player->SetAngle({float(std::atan2(50.0,std::hypot(delta.x,delta.y))),0,float(std::atan2(delta.x,delta.y))});}
    else if(op=="inspect") {
        if(target)spdlog::info("PLAYTEST target health {:.3f}",target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));
        if(player){auto inv=inventoryOf(player);for(auto& recipe:recipes())spdlog::info("PLAYTEST inventory {:08X} count {}",recipe.output,inv[recipe.output]);}
        auto ui=RE::UI::GetSingleton();std::uint32_t current{},stored{};RE::ControlMap::GetSingleton()->GetControlsState(current,stored);
        spdlog::info("PLAYTEST paused={} crafting={} tutorial={} occupied={} controls={:X} owned={:X} pendingWorkbench={}",ui->GameIsPaused(),ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME),ui->IsMenuOpen(RE::TutorialMenu::MENU_NAME),bool(player&&player->GetOccupiedFurniture()),current,ownedFlags,requestWorkbench);
    } else if(op=="alchemy"&&player) {
        if(auto base=RE::TESForm::LookupByID<RE::TESBoundObject>(0xBAD0C))if(auto ref=player->PlaceObjectAtMe(base,false)) {
            auto pos=player->GetPosition();auto yaw=player->GetAngleZ();
            ref->SetPosition({pos.x+std::sin(yaw)*140,pos.y+std::cos(yaw)*140,pos.z});
            ref->ActivateRef(player,0,nullptr,1,false);
        }
    } else if(op=="magic")RE::UIMessageQueue::GetSingleton()->AddMessage(RE::MagicMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kShow,nullptr);
    else if(op=="dismiss")RE::UIMessageQueue::GetSingleton()->AddMessage(RE::TutorialMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kHide,nullptr);
    else if(op=="capture")captureRequested=true;
}
#endif
void update() {
    frameQueued=false;
#ifdef VOXEL_PLAYTEST
    playtest();
#endif
    auto now=Clock::now();double dt=std::chrono::duration<double>(now-lastFrame).count();lastFrame=now;
    auto player=RE::PlayerCharacter::GetSingleton();auto ui=RE::UI::GetSingleton();
    bool world=player&&player->GetParentCell()&&player->Is3DLoaded()&&ui&&!ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
    bool paused=!world||ui->GameIsPaused()||ui->IsMenuOpen(RE::Console::MENU_NAME);
    std::vector<Command> pending;
    {std::lock_guard lock(sharedMutex);pending.swap(commands);}
    for(auto command:pending) {
        if(command.action==Action::ToggleDebug){debug=!debug;continue;}
        if(command.action==Action::ToggleEnabled){enabled=!enabled;workbench=false;if(!enabled)restoreCamera();reserveKeys(enabled);notify(enabled?"Voxel controls enabled":"Vanilla controls restored");continue;}
        if(command.action==Action::CloseWorkbench){workbench=false;continue;}
        if(!enabled||!world||paused)continue;
        switch(command.action) {
            case Action::Camera:setCamera((cameraMode+1)%3);break;
            case Action::Creative:creativeArmed=!creativeArmed;movement.setMode(creativeArmed?Mode::Creative:Mode::Survival);notify(creativeArmed?"Creative flight: Space up, Shift down":"Survival movement");break;
            case Action::Glide:if(!movement.state.grounded){creativeArmed=false;movement.setMode(movement.state.mode==Mode::Glide?Mode::Survival:Mode::Glide);notify(movement.state.mode==Mode::Glide?"Gliding: steer with mouse, hold Ctrl to boost":"Glider folded");}break;
            case Action::Workbench:workbench=!workbench;break;
            case Action::Craft:if(workbench)craft(player,command.value);break;
            case Action::SelectSpell:selectedSpell=std::clamp(command.value,0,2);break;
            case Action::Attack:if(!workbench)attack(player);break;
            case Action::Cast:if(!workbench&&meleeWeapon(player))cast(player);break;
            default:break;
        }
    }
    auto controller=world?player->GetCharController():nullptr;
    // Crafting saves the pre-menu controls, including the flags owned by this
    // plugin. Restore only those flags after its asynchronous close finishes.
    if(pendingRestoreFlags&&!paused&&!ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)&&!player->GetOccupiedFurniture()) {
        RE::ControlMap::GetSingleton()->ToggleControls(static_cast<Flag>(pendingRestoreFlags),true,false);
        pendingRestoreFlags=0;
    }
    bool active=enabled&&!paused&&controller&&!player->IsDead()&&!player->IsInKillMove()&&!player->IsInRagdollState()&&!player->IsOnMount()&&!player->AsActorState()->IsSwimming()&&player->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kNormal;
    if(active) {
        auto map=RE::ControlMap::GetSingleton();
        // Respect quests/cutscenes that disabled movement before we acquired it.
        if(!(ownedFlags&1)&&!map->IsMovementControlsEnabled())active=false;
    }
    if(!active) {releasePhysics(player,enabled&&world);workbench=false;keys.fill(false);}
    else {
        if(requestWorkbench){workbench=true;requestWorkbench=false;}
        if(controller!=ownedController){releasePhysics(player,true);ownedController=controller;savedGravity=controller->gravity;}
        std::uint32_t flags=std::uint32_t(Flag::kMovement)|std::uint32_t(Flag::kJumping)|std::uint32_t(Flag::kSneaking)|std::uint32_t(Flag::kPOVSwitch);
        bool melee=meleeWeapon(player);
        if(melee||workbench)flags|=std::uint32_t(Flag::kFighting);
        if(workbench)flags|=std::uint32_t(Flag::kLooking)|std::uint32_t(Flag::kWheelZoom)|std::uint32_t(Flag::kActivate);
        controls(flags);
        RE::hkVector4 engineVelocity;controller->GetLinearVelocityImpl(engineVelocity);
        bool grounded=controller->surfaceInfo.supportedState==RE::hkpSurfaceInfo::SupportedState::kSupported&&movement.state.velocity.z<=0.1;
        Input input;
        input.forward=double(keys[0x11])-double(keys[0x1F]);input.strafe=double(keys[0x20])-double(keys[0x1E]);
        input.yaw=player->GetAngleZ();input.pitch=player->GetAngleX();
        input.jump=keys[0x39];input.descend=keys[0x2A]||keys[0x36];input.sprint=keys[0x1D]||keys[0x9D];input.boost=input.sprint;
        if(workbench)input={};
        if(input.boost&&movement.state.mode==Mode::Glide) {
            auto values=player->AsActorValueOwner();float cost=float(std::min(dt,.2)*15);
            if(values->GetActorValue(RE::ActorValue::kMagicka)>=cost)values->DamageActorValue(RE::ActorValue::kMagicka,cost);
            else input.boost=false;
        }
        controller->gravity=0;
        movement.advance(dt,input,grounded,unpack(engineVelocity));
        if(movement.state.jumped){controller->wantState=RE::hkpCharacterStateType::kInAir;controller->flags.reset(RE::CHARACTER_FLAGS::kSupport);}
        {std::lock_guard lock(physicsMutex);physicsPlayer=controller;desiredVelocity=pack(movement.state.velocity);}
        controller->SetLinearVelocityImpl(pack(movement.state.velocity));
        // Fall damage is disabled only while flying; survival keeps vanilla damage.
        if(movement.state.mode!=Mode::Survival){RE::hkVector4 position;controller->GetPositionImpl(position,false);controller->fallTime=0;controller->fallStartHeight=float(unpack(position).z);}
        if(cameraMode==2)if(auto third=thirdPerson()){third->freeRotationEnabled=true;third->freeRotation={float(std::numbers::pi),0};}
        combat.advance(dt);
    }
    updatePlayerModel(enabled&&world,movement.state.mode,movement.state.velocity,paused?0:dt);
    static auto lastTelemetry=Clock::now();
    if(debug&&world&&now-lastTelemetry>std::chrono::seconds(1)) {
        auto pos=player->GetPosition();RE::hkVector4 measured{};if(controller)controller->GetLinearVelocityImpl(measured);
        auto v=unpack(measured);
        spdlog::info("Live active={} mode={} xyz=({:.1f},{:.1f},{:.1f}) measured=({:.2f},{:.2f},{:.2f}) physicsCalls={} workbench={}",active,int(movement.state.mode),pos.x,pos.y,pos.z,v.x,v.y,v.z,physicsCalls.load(),workbench);
#ifdef VOXEL_PLAYTEST
        RE::Actor* nearest{};float nearestDistance=10000;
        RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* actor){
            if(actor!=player&&actor->GetParentCell()==player->GetParentCell()){
                float distance=(actor->GetPosition()-pos).Length();if(distance<nearestDistance){nearest=actor;nearestDistance=distance;}
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if(nearest){auto np=nearest->GetPosition();spdlog::info("Nearest actor {:08X} {} xyz=({:.1f},{:.1f},{:.1f}) health={:.1f}",nearest->GetFormID(),nearest->GetName(),np.x,np.y,np.z,nearest->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth));}
#endif
        lastTelemetry=now;
    }
    Snapshot next;
    next.enabled=enabled;next.active=active;next.debug=debug;next.workbench=workbench;next.creativeArmed=creativeArmed;
    next.mode=movement.state.mode;next.velocity=movement.state.velocity;next.camera=cameraMode;next.status=status;next.selectedSpell=selectedSpell;next.attackCharge=float(combat.charge());
    if(world) {
        auto pos=player->GetPosition();next.position={pos.x,pos.y,pos.z};
        auto values=player->AsActorValueOwner();next.health=values->GetActorValue(RE::ActorValue::kHealth);next.magicka=values->GetActorValue(RE::ActorValue::kMagicka);next.stamina=values->GetActorValue(RE::ActorValue::kStamina);
        next.location=player->GetParentCell()->GetName();next.melee=meleeWeapon(player);
        if(auto pick=RE::CrosshairPickData::GetSingleton())if(auto target=pick->targetActor.get())next.target=target->GetName();
        for(int i=0;i<3;++i)if(auto spell=spellAt(i))next.spells[i]=player->HasSpell(spell);
        if(workbench) {
            auto inventory=inventoryOf(player);
            for(auto& recipe:recipes()) {
                RecipeView view;auto spell=recipe.spell?RE::TESForm::LookupByID<RE::SpellItem>(recipe.output):nullptr;
                view.learned=spell&&player->HasSpell(spell);view.available=canCraft(recipe,inventory,view.learned);
                for(auto ingredient:recipe.inputs)view.counts.push_back(inventory[ingredient.form]);
                next.recipes.push_back(std::move(view));
            }
        }
    }
    {std::lock_guard lock(sharedMutex);snapshot=std::move(next);}
}
class InputSink final:public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if(!events)return RE::BSEventNotifyControl::kContinue;
        for(auto event=*events;event;event=event->next) {
            if(event->eventType==RE::INPUT_EVENT_TYPE::kMouseMove) {
                auto mouse=static_cast<RE::MouseMoveEvent*>(event);std::lock_guard lock(sharedMutex);pointer.dx+=mouse->mouseInputX;pointer.dy+=mouse->mouseInputY;
            }
            auto button=event->AsButtonEvent();if(!button)continue;
            auto key=button->GetIDCode();bool down=button->IsDown();
            if(button->device==RE::INPUT_DEVICE::kKeyboard) {
                if(key<keys.size())keys[key]=button->IsPressed();
                if(!down)continue;
                switch(key) {
                    case 0x3D:enqueue(Action::ToggleDebug);break;
                    case 0x3F:enqueue(Action::Camera);break;
                    case 0x40:enqueue(Action::Creative);break;
                    case 0x41:enqueue(Action::Glide);break;
                    case 0x42:enqueue(Action::Workbench);break;
                    case 0x44:enqueue(Action::ToggleEnabled);break;
                    case 0x01:enqueue(Action::CloseWorkbench);break;
                    case 2:case 3:case 4:enqueue(Action::SelectSpell,int(key)-2);break;
                    case 0x39:
                        if(creativeArmed) {
                            auto now=Clock::now();
                            if(now-lastJump<std::chrono::milliseconds(280))movement.setMode(movement.state.mode==Mode::Creative?Mode::Survival:Mode::Creative);
                            lastJump=now;
                        }break;
                    default:break;
                }
            } else if(button->device==RE::INPUT_DEVICE::kMouse) {
                {std::lock_guard lock(sharedMutex);if(key<3)pointer.buttons[key]=button->IsPressed();if(down&&key==8)pointer.wheel+=1;if(down&&key==9)pointer.wheel-=1;}
                if(down&&key==0)enqueue(Action::Attack);
                if(down&&key==1)enqueue(Action::Cast);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} inputSink;
class MenuSink final:public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if(!event||!event->opening||!enabled)return RE::BSEventNotifyControl::kContinue;
        if(event->menuName==RE::CraftingMenu::MENU_NAME) {
          const auto inheritedControls=ownedFlags;
          SKSE::GetTaskInterface()->AddTask([inheritedControls]{
            auto menu=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();
            if(menu&&skyrim_cast<RE::CraftingSubMenus::CraftingSubMenus::AlchemyMenu*>(menu->GetCraftingSubMenu())) {
                RE::CraftingMenu::QuitMenu();
                RE::PlayerCharacter::GetSingleton()->StopInteractingQuick(false);
                pendingRestoreFlags|=inheritedControls;
                requestWorkbench=true;spdlog::info("Alchemy station handed off to workbench");
            }
          });
        }
        if(event->menuName==RE::MagicMenu::MENU_NAME)SKSE::GetTaskInterface()->AddTask([]{
            RE::UIMessageQueue::GetSingleton()->AddMessage(RE::MagicMenu::MENU_NAME,RE::UI_MESSAGE_TYPE::kHide,nullptr);
            requestWorkbench=true;
        });
        return RE::BSEventNotifyControl::kContinue;
    }
} menuSink;
}
Snapshot readSnapshot(){std::lock_guard lock(sharedMutex);return snapshot;}
void enqueue(Action action,int value){std::lock_guard lock(sharedMutex);commands.push_back({action,value});}
PointerInput takePointerInput(){std::lock_guard lock(sharedMutex);auto result=pointer;pointer.dx=pointer.dy=pointer.wheel=0;return result;}
void queueFrame(){if(!frameQueued.exchange(true))SKSE::GetTaskInterface()->AddTask(update);}
void resetRuntime(){restoreCamera();resetPlayerModel();releasePhysics(RE::PlayerCharacter::GetSingleton());keys.fill(false);workbench=false;requestWorkbench=false;pendingRestoreFlags=0;creativeArmed=false;cameraMode=0;combat.elapsed=10;lastFrame=Clock::now();}
void startRuntime(){
    installPhysics();
    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&inputSink);
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
    reserveKeys(true);spdlog::info("Input and menu adapters installed");
}
}
