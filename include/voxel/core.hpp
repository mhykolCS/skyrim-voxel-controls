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
    double walkSpeed=4.317, sprintSpeed=5.612, sneakSpeed=1.295;
    double jumpSpeed=8.4, gravity=32.0, terminalSpeed=78.4;
    double flightSpeed=10.8, flightBoost=2.0, maxGlideSpeed=55.0;
};
struct Input {
    double forward{}, strafe{}, yaw{}, pitch{};
    bool jump{}, descend{}, sprint{}, boost{};
};
struct Motion {
    Vec3 velocity{};
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
    double accumulator_{};
    bool jumpWasDown_{};
    bool jumpQueued_{};
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
