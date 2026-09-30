#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <numbers>

namespace voxel {
namespace {
RE::NiPointer<RE::NiNode> parent,model;
std::vector<std::pair<RE::NiPointer<RE::NiAVObject>,bool>> hidden;
double phase{},swingTime{};
bool loadFailed{};
void hideMeshes(RE::NiAVObject* object) {
    if(!object||object==model.get())return;
    if(object->AsGeometry()) {
        auto found=std::find_if(hidden.begin(),hidden.end(),[&](auto& item){return item.first.get()==object;});
        if(found==hidden.end())hidden.emplace_back(RE::NiPointer<RE::NiAVObject>{object},object->GetAppCulled());
        object->SetAppCulled(true);
    }
    if(auto node=object->AsNode())for(auto& child:node->GetChildren())hideMeshes(child.get());
}
void rotate(const char* name,float x,float y=0,float z=0) {
    if(auto part=model->GetObjectByName(name))part->local.rotate.SetEulerAnglesXYZ(x,y,z);
}
}
void resetPlayerModel() {
    if(parent&&model)parent->DetachChild(model.get());
    for(auto& [mesh,culled]:hidden)mesh->SetAppCulled(culled);
    hidden.clear();model.reset();parent.reset();phase=0;swingTime=0;
}
void swingPlayerModel(){swingTime=.28;}
void updatePlayerModel(bool enabled,Mode mode,Vec3 velocity,double dt) {
    auto player=RE::PlayerCharacter::GetSingleton();
    auto root=player?player->Get3D(false):nullptr;
    auto node=root?root->AsNode():nullptr;
    if(!enabled||!node){resetPlayerModel();return;}
    if(parent.get()!=node)resetPlayerModel();
    if(!model&&!loadFailed) {
        RE::NiPointer<RE::NiNode> source;
        auto result=RE::BSModelDB::Demand("VoxelControls\\steve.nif",source,{});
        if(!source){spdlog::error("Steve model load failed: {}",int(result));loadFailed=true;return;}
        auto clone=source->Clone();
        model.reset(clone?clone->AsNode():nullptr);
        if(!model){spdlog::error("Steve model clone failed");loadFailed=true;return;}
        parent.reset(node);parent->AttachChild(model.get(),true);
        spdlog::info("Classic Steve model attached");
    }
    if(!model)return;
    hideMeshes(parent.get());
    auto speed=velocity.horizontal();phase+=std::min(dt,.2)*speed*2.3;
    auto stride=float(std::sin(phase)*std::min(speed/4.317,1.0)*.65);
    const bool glide=mode==Mode::Glide;
    model->local.rotate.SetEulerAnglesXYZ(glide?-float(std::numbers::pi/2):0,0,0);
    model->local.translate.z=glide?60.f:0.f;
    rotate("SteveHead",std::clamp(player->GetAngleX(),-1.1f,1.1f));
    rotate("SteveRightLeg",glide?0:stride);rotate("SteveLeftLeg",glide?0:-stride);
    rotate("SteveLeftArm",glide?-2.8f:stride);
    swingTime=std::max(0.0,swingTime-std::min(dt,.2));
    rotate("SteveRightArm",swingTime>0?float(-1.6*std::sin(swingTime/.28*std::numbers::pi)):(glide?-2.8f:-stride));
    RE::NiUpdateData data{};data.time=float(dt);data.flags.set(RE::NiUpdateData::Flag::kDirty);
    model->Update(data);
}
}
