#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace voxel {
struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double s) const { return {x*s,y*s,z*s}; }
    double horizontal() const { return std::hypot(x,y); }
    double length() const { return std::sqrt(x*x+y*y+z*z); }
    bool finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
enum class Mode { Survival, Creative, Glide };
struct Settings {
    // Java Edition, normal ground. Acceleration/impulses use blocks per tick.
    double groundAcceleration=.1, groundDrag=.6*.91;
    double airAcceleration=.02, airDrag=.91, verticalDrag=.98;
    double jumpImpulse=.42, gravity=.08, sprintMultiplier=1.3, sneakMultiplier=.3;
    double flightAcceleration=.05, flightVerticalAcceleration=.15;
    double flightSprintMultiplier=2, flightVerticalDrag=.6;
};
struct Input {
    double forward{}, strafe{}, yaw{}, pitch{};
    bool jump{}, descend{}, sprint{}, boost{};
};
struct Motion {
    Vec3 velocity{};
    Vec3 frameVelocity{};
    Mode mode=Mode::Survival;
    bool grounded{}, jumped{};
};
// The engine still resolves collisions. All velocities here are metres/second.
// A fixed 20 Hz simulation keeps jumps and drag independent of render rate.
class Movement {
public:
    Settings settings;
    Motion state;
    void reset(Vec3 velocity={});
    void setMode(Mode mode);
    void advance(double dt, const Input& input, bool grounded, Vec3 measuredVelocity);
private:
    void tick(const Input& input, bool grounded);
    Vec3 momentum_{}; // blocks/tick, after drag; distinct from the move this tick
    double tickRemaining_{};
    int jumpCooldown_{};
    bool flightAirborne_{};
    bool jumpWasDown_{};
    bool jumpQueued_{};
};
// Minecraft's camera modifier blends halfway to its target every game tick.
class FlightFov {
public:
    double multiplier=1;
    double advance(double dt,bool flying,bool sprinting) {
        if(std::isfinite(dt)&&dt>0) {
            const double target=flying?(sprinting?1.1*1.15:1.1):1;
            multiplier=target+(multiplier-target)*std::pow(.5,dt/.05);
        }
        return multiplier;
    }
    void reset(){multiplier=1;}
};
struct Strike {
    double damage{}, charge{}, knockback{};
    bool critical{};
};
class Combat {
public:
    double attackSpeed=1.6;
    double elapsed=10.0;
    void advance(double dt) { if(std::isfinite(dt)) elapsed=std::clamp(elapsed+std::max(0.0,dt),0.0,10.0); }
    double charge() const { return std::clamp(elapsed*attackSpeed,0.0,1.0); }
    Strike strike(double baseDamage, bool falling, bool sprinting);
};
struct Ingredient { std::uint32_t form{}; int count{}; };
struct Recipe {
    std::string name, description;
    std::vector<Ingredient> inputs;
    std::uint32_t output{};
    int count=1;
    bool spell{};
};
using Inventory=std::map<std::uint32_t,int>;
// One preflight accounts for repeated ingredients and rejects invalid recipes.
bool canCraft(const Recipe& recipe,const Inventory& inventory,bool alreadyLearned=false);
std::map<std::uint32_t,int> requirements(const Recipe& recipe);
const std::vector<Recipe>& recipes();
}
