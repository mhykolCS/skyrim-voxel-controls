#pragma once
#include <cmath>
#include <optional>

namespace voxel {

enum class Action {
    ToggleEnabled, ToggleDebug, Camera, Creative, Glide,
    Workbench, CloseWorkbench, Craft, SelectSpell, Attack, Cast
};

enum class ControlMode { Unavailable, Disabled, Scripted, Paused, Gameplay };

struct ControlContext {
    bool enabled{}, world{}, paused{}, gameplayCamera{};
    bool movement{}, looking{}, pov{}, jumping{}, sneaking{}, fighting{}, activate{};
    bool movementHandler{}, inputBlocked{}, scriptedPOV{}, aiDriven{}, characterSetup{};
    bool scene{}, actorRestricted{}, furniture{};
};

constexpr const char* scriptedControlReason(const ControlContext& context) {
    if(!context.movement)return "Skyrim has locked movement";
    if(!context.looking)return "Skyrim has locked looking";
    if(!context.movementHandler)return "Skyrim has suspended movement input";
    if(context.inputBlocked)return "Skyrim has blocked player input";
    if(context.scriptedPOV)return "a script controls the camera";
    if(context.aiDriven)return "a script controls the player";
    if(context.characterSetup)return "character setup is active";
    if(context.scene)return "the player is participating in a scene";
    if(context.actorRestricted)return "the player is swimming, mounted, or incapacitated";
    if(context.furniture)return "the player is using furniture";
    return "";
}

constexpr ControlMode controlMode(const ControlContext& context) {
    if(!context.enabled)return ControlMode::Disabled;
    if(!context.world)return ControlMode::Unavailable;
    if(*scriptedControlReason(context))return ControlMode::Scripted;
    if(context.paused)return ControlMode::Paused;
    if(!context.gameplayCamera)return ControlMode::Scripted;
    return ControlMode::Gameplay;
}

constexpr bool allowsAction(const ControlContext& context,Action action) {
    if(action==Action::ToggleEnabled||action==Action::ToggleDebug||action==Action::CloseWorkbench)return true;
    if(controlMode(context)!=ControlMode::Gameplay)return false;
    switch(action) {
        // Helgen can keep POV switching locked after releasing movement/jumping.
        // This permission belongs to the camera shortcut, not the physics gate.
        case Action::Camera:return context.pov;
        case Action::Creative:case Action::Glide:return context.jumping;
        case Action::Workbench:case Action::Craft:return context.activate;
        case Action::Attack:case Action::Cast:case Action::SelectSpell:return context.fighting;
        default:return true;
    }
}

constexpr const char* actionBlockedReason(const ControlContext& context,Action action) {
    if(allowsAction(context,action))return "";
    switch(controlMode(context)) {
        case ControlMode::Disabled:return "VoxelControls is disabled";
        case ControlMode::Unavailable:return "no playable world is loaded";
        case ControlMode::Paused:return "a menu or dialogue is open";
        case ControlMode::Scripted:
            return *scriptedControlReason(context)?scriptedControlReason(context):"the camera is in an animation or transition";
        case ControlMode::Gameplay:
            switch(action) {
                case Action::Camera:return "Skyrim has locked camera switching";
                case Action::Creative:case Action::Glide:return "Skyrim has locked jumping";
                case Action::Workbench:case Action::Craft:return "Skyrim has locked activation";
                default:return "Skyrim has locked combat";
            }
    }
    return "";
}

constexpr bool allowsWorkbenchMenu(const ControlContext& context) {
    const auto mode=controlMode(context);
    return context.activate&&(mode==ControlMode::Gameplay||mode==ControlMode::Paused);
}

// Seven Java ticks. Consume a pair so a third press cannot toggle back out.
class FlightDoubleTap {
public:
    bool press(double seconds,bool allowed) {
        if(!allowed||!std::isfinite(seconds)){reset();return false;}
        const bool paired=last_&&seconds>*last_&&seconds-*last_<=.35;
        if(paired)last_.reset();else last_=seconds;
        return paired;
    }
    void reset(){last_.reset();}
private:
    std::optional<double> last_;
};

}
