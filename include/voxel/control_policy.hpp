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

constexpr ControlMode controlMode(const ControlContext& context) {
    if(!context.enabled)return ControlMode::Disabled;
    if(!context.world)return ControlMode::Unavailable;
    if(!context.movement||!context.looking||!context.pov||!context.movementHandler||
       context.inputBlocked||context.scriptedPOV||context.aiDriven||context.characterSetup||
       context.scene||context.actorRestricted||context.furniture)return ControlMode::Scripted;
    if(context.paused)return ControlMode::Paused;
    if(!context.gameplayCamera)return ControlMode::Scripted;
    return ControlMode::Gameplay;
}

constexpr bool allowsAction(const ControlContext& context,Action action) {
    if(action==Action::ToggleEnabled||action==Action::ToggleDebug||action==Action::CloseWorkbench)return true;
    if(controlMode(context)!=ControlMode::Gameplay)return false;
    switch(action) {
        case Action::Creative:case Action::Glide:return context.jumping;
        case Action::Workbench:case Action::Craft:return context.activate;
        case Action::Attack:case Action::Cast:case Action::SelectSpell:return context.fighting;
        default:return true;
    }
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
