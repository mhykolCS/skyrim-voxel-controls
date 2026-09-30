#include "voxel/control_policy.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace voxel;

void check(bool value,const char* message) {
    if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}

ControlContext gameplay() {
    ControlContext c;
    c.enabled=c.world=c.gameplayCamera=c.movement=c.looking=c.pov=true;
    c.jumping=c.sneaking=c.fighting=c.activate=c.movementHandler=true;
    return c;
}

void expectSuspended(const ControlContext& c) {
    constexpr std::array actions{Action::Camera,Action::Creative,Action::Glide,
        Action::ToggleHover,Action::Workbench,Action::Craft,Action::SelectSpell,
        Action::Attack,Action::Cast};
    check(controlMode(c)!=ControlMode::Gameplay,"scripted state cannot drive physics");
    for(auto action:actions)check(!allowsAction(c,action),"scripted state rejects every gameplay shortcut");
    check(allowsAction(c,Action::ToggleDebug),"diagnostics remain available");
    check(allowsAction(c,Action::ToggleEnabled),"F10 remains available");
    check(allowsAction(c,Action::CloseWorkbench),"workbench can always close");
}

int main() {
    const auto free=gameplay();
    check(controlMode(free)==ControlMode::Gameplay,"ordinary gameplay is available");
    for(auto field:{&ControlContext::movement,&ControlContext::looking,&ControlContext::pov,
                   &ControlContext::movementHandler,&ControlContext::gameplayCamera}) {
        auto c=free;c.*field=false;expectSuspended(c);
        check(!allowsWorkbenchMenu(c),"restriction cannot be bypassed by a native menu");
    }
    for(auto field:{&ControlContext::inputBlocked,&ControlContext::scriptedPOV,
                   &ControlContext::aiDriven,&ControlContext::characterSetup,&ControlContext::scene,
                   &ControlContext::actorRestricted,&ControlContext::furniture}) {
        auto c=free;c.*field=true;expectSuspended(c);
        check(!allowsWorkbenchMenu(c),"scene cannot be interrupted by menu replacement");
    }
    auto c=free;c.paused=true;expectSuspended(c);
    check(controlMode(c)==ControlMode::Paused,"ordinary pause is distinct from a scene");
    c.aiDriven=true;
    check(controlMode(c)==ControlMode::Scripted,"pausing must not hide a scene restriction");
    c=free;c.enabled=false;expectSuspended(c);
    c=free;c.world=false;expectSuspended(c);
    c=free;c.jumping=false;
    check(!allowsAction(c,Action::Creative)&&!allowsAction(c,Action::Glide)&&
          !allowsAction(c,Action::ToggleHover),"flight cannot bypass a jump restriction");
    c=free;c.fighting=false;
    check(!allowsAction(c,Action::Attack)&&!allowsAction(c,Action::Cast),"custom combat honors fighting restriction");
    c=free;c.activate=false;
    check(!allowsAction(c,Action::Workbench)&&!allowsAction(c,Action::Craft),"workbench honors activation restriction");
    // A quest takes control after ordinary movement has already been running.
    c=free;check(allowsAction(c,Action::Camera),"camera works before scene");
    c.movement=false;expectSuspended(c);
    c.movement=true;c.aiDriven=true;expectSuspended(c);
    c.aiDriven=false;check(allowsAction(c,Action::Camera),"resume only after all scene restrictions end");
    std::cout<<"Control policy regressions passed\n";
}
