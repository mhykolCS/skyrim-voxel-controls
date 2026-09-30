#include "voxel/core.hpp"
#include <limits>

namespace voxel {
void Movement::reset(Vec3 velocity) {
    state={}; state.velocity=velocity.finite()?velocity:Vec3{};
    momentum_=state.velocity*.05;tickRemaining_=0;jumpCooldown_=0;
    flightAirborne_=false;jumpWasDown_=false;jumpQueued_=false;
}
void Movement::setMode(Mode mode) {
    if(state.mode==mode) return;
    state.mode=mode;
    tickRemaining_=0;jumpQueued_=false;flightAirborne_=false;
    if(mode==Mode::Creative){state.velocity.z=0;momentum_.z=0;}
}
void Movement::advance(double dt,const Input& input,bool grounded,Vec3 measuredVelocity) {
    if(!std::isfinite(dt)||dt<=0) return;
    if(!std::isfinite(input.yaw)||!std::isfinite(input.pitch)) return;
    state.jumped=false;
    state.grounded=grounded;
    if(input.jump&&!jumpWasDown_) jumpQueued_=true;
    jumpWasDown_=input.jump;
    if(state.mode==Mode::Creative) {
        if(!grounded)flightAirborne_=true;
        else if(flightAirborne_){setMode(Mode::Survival);momentum_.z=0;state.velocity.z=0;}
    }
    // Havok remains responsible for collisions. Do not push through a ceiling
    // or retain downward momentum after contact with the floor.
    if(measuredVelocity.finite()) {
        if((state.velocity.z>.5&&measuredVelocity.z<.05&&!grounded)||
           (grounded&&state.velocity.z<0)){state.velocity.z=0;momentum_.z=0;}
    }
    // Integrate portions of Minecraft ticks, rather than applying the final
    // tick's velocity across the entire render frame. This preserves the jump
    // displacement even when a render frame straddles two or more 50 ms ticks.
    const double duration=std::min(dt,.2);double remaining=duration;Vec3 displacement;
    while(remaining>1e-9) {
        if(tickRemaining_<1e-9){tick(input,grounded);tickRemaining_=.05;}
        if(state.jumped)grounded=false;
        const double slice=std::min(remaining,tickRemaining_);
        displacement=displacement+state.velocity*slice;
        remaining-=slice;tickRemaining_-=slice;
    }
    state.frameVelocity=displacement*(1.0/duration);
    if(!state.velocity.finite()) reset();
}
void Movement::tick(const Input& input,bool grounded) {
    auto v=momentum_;
    const bool flying=state.mode==Mode::Creative;
    if(flying)v.z+=(double(input.jump)-double(input.descend))*settings.flightVerticalAcceleration;
    for(auto component:{&v.x,&v.y,&v.z})if(std::abs(*component)<.003)*component=0;
    const bool sprint=input.sprint&&(flying||(!input.descend&&input.forward>.8));
    if(jumpCooldown_>0)--jumpCooldown_;
    double f=std::clamp(input.forward,-1.0,1.0)*.98,s=std::clamp(input.strafe,-1.0,1.0)*.98;
    if(input.descend&&!flying){f*=settings.sneakMultiplier;s*=settings.sneakMultiplier;}
    double n=std::hypot(f,s); if(n>1) {f/=n;s/=n;}
    const double sy=std::sin(input.yaw),cy=std::cos(input.yaw);
    const Vec3 wish{sy*f+cy*s,cy*f-sy*s,0};
    if(state.mode==Mode::Glide) {
        if(grounded) {state.mode=Mode::Survival;v.z=0;}
        else {
            const double pitch=std::clamp(input.pitch,-1.57079632679,1.57079632679);
            const double cp=std::cos(pitch),sp=std::sin(pitch);
            // A held boost uses the Java firework acceleration rule. The
            // Skyrim adapter supplies its resource cost separately.
            if(input.boost) {
                Vec3 look{sy*cp,cy*cp,-sp};
                v=v+(look*1.5-v)*.5+look*.1;
            }
            const double horizontal=v.horizontal();
            const double lift=cp*cp;
            v.z+=settings.gravity*(-1+lift*.75);
            if(v.z<0&&cp>1e-9) {
                double transfer=-v.z*.1*lift;
                v.z+=transfer;v.x+=sy*transfer;v.y+=cy*transfer;
            }
            if(pitch<0&&cp>1e-9) {
                double climb=horizontal*(-sp)*.04;
                v.z+=climb*3.2;v.x-=sy*climb;v.y-=cy*climb;
            }
            if(cp>1e-9) {
                v.x+=(sy*horizontal-v.x)*.1;
                v.y+=(cy*horizontal-v.y)*.1;
            }
            v.x*=0.99;v.y*=0.99;v.z*=0.98;
            state.velocity=v*20;momentum_=v;
            jumpQueued_=false;
            return;
        }
    }
    if(!flying&&(input.jump||jumpQueued_)) {
        if(grounded&&jumpCooldown_==0){
            v.z=settings.jumpImpulse;state.jumped=true;state.grounded=false;jumpCooldown_=10;
            if(sprint){v.x+=sy*.2;v.y+=cy*.2;}
        }
    } else jumpCooldown_=0;
    const double drag=flying?settings.airDrag:(grounded?settings.groundDrag:settings.airDrag);
    const double acceleration=flying?settings.flightAcceleration*(sprint?settings.flightSprintMultiplier:1):
        (grounded?settings.groundAcceleration:settings.airAcceleration)*(sprint?settings.sprintMultiplier:1);
    v.x+=wish.x*acceleration;v.y+=wish.y*acceleration;
    // Move first, then gravity/drag. Reversing this order shortens the jump.
    state.velocity=v*20;
    momentum_={v.x*drag,v.y*drag,flying?v.z*settings.flightVerticalDrag:(v.z-settings.gravity)*settings.verticalDrag};
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
