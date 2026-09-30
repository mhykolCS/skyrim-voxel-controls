#include "voxel/core.hpp"
#include <limits>

namespace voxel {
void Movement::reset(Vec3 velocity) {
    state={}; state.velocity=velocity.finite()?velocity:Vec3{};
    accumulator_=0; jumpWasDown_=false; jumpQueued_=false;
}
void Movement::setMode(Mode mode) {
    if(state.mode==mode) return;
    state.mode=mode;
    if(mode==Mode::Creative) state.velocity.z=0;
    if(mode==Mode::Glide && state.velocity.horizontal()<4) state.velocity.y=4;
}
void Movement::advance(double dt,const Input& input,bool grounded,Vec3 measuredVelocity) {
    if(!std::isfinite(dt)||dt<=0) return;
    if(!std::isfinite(input.yaw)||!std::isfinite(input.pitch)) return;
    state.jumped=false;
    state.grounded=grounded;
    if(input.jump&&!jumpWasDown_) jumpQueued_=true;
    jumpWasDown_=input.jump;
    // Ceiling contacts must cancel upward momentum. Floor contacts clear falling.
    if(measuredVelocity.finite() && state.mode!=Mode::Creative) {
        if(state.velocity.z>0.5&&measuredVelocity.z<0.05&&!grounded) state.velocity.z=0;
        if(grounded&&state.velocity.z<0) state.velocity.z=0;
    }
    accumulator_+=std::min(dt,0.2);
    while(accumulator_>=0.05-1e-9) {
        tick(input,grounded);
        accumulator_-=0.05;
        if(state.jumped) grounded=false;
    }
    if(!state.velocity.finite()) reset();
}
void Movement::tick(const Input& input,bool grounded) {
    auto& v=state.velocity;
    double f=std::clamp(input.forward,-1.0,1.0),s=std::clamp(input.strafe,-1.0,1.0);
    double n=std::hypot(f,s); if(n>1) {f/=n;s/=n;}
    const double sy=std::sin(input.yaw),cy=std::cos(input.yaw);
    const Vec3 wish{sy*f+cy*s,cy*f-sy*s,0};
    if(state.mode==Mode::Creative) {
        double speed=settings.flightSpeed*(input.sprint?settings.flightBoost:1);
        Vec3 target=wish*speed;
        target.z=(double(input.jump)-double(input.descend))*speed;
        if(target.length()>speed) target=target*(speed/target.length());
        v=v*0.35+target*0.65;
        jumpQueued_=false;
        return;
    }
    if(state.mode==Mode::Glide) {
        if(grounded) {state.mode=Mode::Survival;v.z=0;}
        else {
            // Independent glider model: diving trades height for horizontal speed;
            // pitching up converts horizontal momentum into lift, with drag.
            const double pitch=std::clamp(input.pitch,-1.45,1.45);
            const double horizontal=v.horizontal();
            const double lift=std::cos(pitch)*std::cos(pitch);
            v.z+=(-9.8+lift*7.2)*0.05;
            if(v.z<0) {
                double transfer=-v.z*0.08*lift;
                v.z+=transfer;v.x+=sy*transfer;v.y+=cy*transfer;
            }
            if(pitch<0) {
                double climb=horizontal*(-std::sin(pitch))*0.035;
                v.z+=climb*2.4;v.x-=sy*climb;v.y-=cy*climb;
            }
            v.x+=(sy*horizontal-v.x)*0.12;
            v.y+=(cy*horizontal-v.y)*0.12;
            if(input.boost) {
                v.x+=sy*std::cos(pitch)*0.9;
                v.y+=cy*std::cos(pitch)*0.9;
                v.z-=std::sin(pitch)*0.9;
            }
            v.x*=0.99;v.y*=0.99;v.z*=0.98;
            if(v.length()>settings.maxGlideSpeed) v=v*(settings.maxGlideSpeed/v.length());
            jumpQueued_=false;
            return;
        }
    }
    const double speed=input.descend?settings.sneakSpeed:(input.sprint?settings.sprintSpeed:settings.walkSpeed);
    const double response=grounded?0.72:0.12;
    v.x+=(wish.x*speed-v.x)*response;
    v.y+=(wish.y*speed-v.y)*response;
    if(grounded&&jumpQueued_) {
        v.z=settings.jumpSpeed;state.jumped=true;state.grounded=false;
        if(input.sprint) {v.x+=sy*1.3;v.y+=cy*1.3;}
    } else if(grounded) v.z=-0.15;
    else v.z=std::max(-settings.terminalSpeed,(v.z-settings.gravity*0.05)*0.98);
    jumpQueued_=false;
}
Strike Combat::strike(double baseDamage,bool falling,bool sprinting) {
    Strike hit;
    hit.charge=charge();hit.critical=falling&&!sprinting&&hit.charge>0.9;
    hit.damage=std::max(0.0,baseDamage)*(0.2+0.8*hit.charge*hit.charge)*(hit.critical?1.5:1.0);
    hit.knockback=hit.charge>0.9?(sprinting?6.0:3.0):1.0;
    elapsed=0;return hit;
}
std::map<std::uint32_t,int> requirements(const Recipe& recipe) {
    std::map<std::uint32_t,int> needed;
    for(auto item:recipe.inputs) {
        if(item.form==0||item.count<=0||needed[item.form]>std::numeric_limits<int>::max()-item.count) return {};
        needed[item.form]+=item.count;
    }
    return needed;
}
bool canCraft(const Recipe& recipe,const Inventory& inventory,bool alreadyLearned) {
    if(!recipe.output||recipe.count<=0||recipe.inputs.empty()||(recipe.spell&&alreadyLearned)) return false;
    auto needed=requirements(recipe);if(needed.empty()) return false;
    for(auto [id,count]:needed) {
        auto it=inventory.find(id);if(it==inventory.end()||it->second<count) return false;
    }
    return true;
}
// Vanilla Skyrim.esm IDs; tools/inspect_game.py verifies them from the local game.
const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> book{
        {"Healing mixture","Blue mountain flower + wheat",{{0x77E1C,1},{0x4B0BA,1}},0x3EADD,1,false},
        {"Magicka mixture","Red mountain flower + mora tapinella",{{0x77E1D,1},{0xEC870,1}},0x3EAE0,1,false},
        {"Stamina mixture","Purple mountain flower + thistle",{{0x77E1E,1},{0x134AA,1}},0x3EAE5,1,false},
        {"Firebolt matrix","Fire salts + filled petty soul gem",{{0x3AD5E,1},{0x2E4E3,1}},0x12FD0,1,true},
        {"Ice spike matrix","Frost salts + filled petty soul gem",{{0x3AD5F,1},{0x2E4E3,1}},0x2B96C,1,true},
        {"Lightning matrix","Void salts + filled petty soul gem",{{0x3AD60,1},{0x2E4E3,1}},0x2DD29,1,true}
    };
    return book;
}
}
