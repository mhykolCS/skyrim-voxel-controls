#include "voxel/core.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace voxel;
int checks=0;
void check(bool ok,const char* message) {++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
Motion simulate(int fps,Input input,Mode mode,bool ground) {
    Movement m;m.setMode(mode);
    for(int i=0;i<fps*3;++i) m.advance(1.0/fps,input,ground,m.state.velocity);
    return m.state;
}
int main() {
    Input walk;walk.forward=1;
    auto a=simulate(30,walk,Mode::Survival,true),b=simulate(144,walk,Mode::Survival,true);
    check((a.velocity-b.velocity).length()<1e-7,"walking independent of render FPS");
    Input diagonal=walk;diagonal.strafe=1;
    auto d=simulate(60,diagonal,Mode::Survival,true);
    check(std::abs(d.velocity.horizontal()/a.velocity.horizontal()-1/.98)<1e-7,"Java 1.21.1 diagonal input normalization");
    check(std::abs(a.velocity.horizontal()-4.31718061674)<1e-5,"Java normal-block walking speed");
    Input fly;fly.jump=true;
    auto f=simulate(60,fly,Mode::Creative,false);
    check(std::abs(f.velocity.z-7.5)<1e-7,"creative ascends at Java vertical speed");
    auto cruise=simulate(60,walk,Mode::Creative,false);
    Input fast=walk;fast.sprint=true;auto sprintFlight=simulate(60,fast,Mode::Creative,false);
    check(std::abs(cruise.velocity.horizontal()-10.8888888889)<.05,"creative horizontal flight speed");
    check(std::abs(sprintFlight.velocity.horizontal()/cruise.velocity.horizontal()-2)<1e-7,"Ctrl doubles horizontal flight speed");
    auto hover=simulate(60,{},Mode::Creative,false);
    check(hover.velocity.length()==0,"creative hover has no gravity");
    Movement jump;Input j;j.jump=true;jump.advance(0.05,j,true,{});
    check(jump.state.jumped&&jump.state.velocity.z==8.4,"jump impulse");
    double height=0,apex=0;
    for(int i=0;i<40;++i){height+=jump.state.velocity.z*.05;apex=std::max(apex,height);jump.advance(.05,{},false,jump.state.velocity);}
    check(apex>1.0&&apex<1.5,"jump reaches approximately one block");
    check(jump.state.velocity.z<0,"gravity reverses jump");
    jump.advance(0.05,{},true,{});check(jump.state.velocity.z>=-0.15,"landing cancels falling");
    Movement glide;glide.setMode(Mode::Glide);Input boost;boost.boost=true;
    for(int i=0;i<20000;++i){boost.pitch=std::sin(i*.01);glide.advance(.05,boost,false,glide.state.velocity);check(glide.state.velocity.finite()&&glide.state.velocity.length()<=55.00001,"gliding remains finite and bounded");}
    glide.advance(.05,{},true,{});check(glide.state.mode==Mode::Survival,"glider lands into survival mode");
    for(int fps:{15,30,60,144}) {
        Movement arc;double displacement=0;
        for(int frame=0;frame<fps;++frame){Input press;press.jump=frame==0;arc.advance(1.0/fps,press,frame==0,arc.state.velocity);displacement+=arc.state.frameVelocity.z/fps;}
        // Exact Java displacement after twenty airborne ticks, including the
        // initial 0.42-block jump. A render boundary must not shorten the arc.
        check(std::abs(displacement-(-6.2709298708555))<1e-5,"jump displacement invariant from 15 to 144 FPS");
    }
    Movement landed;landed.setMode(Mode::Creative);landed.advance(.05,{},false,{});landed.advance(.05,{},true,{});
    check(landed.state.mode==Mode::Survival,"touching down ends creative flight");
    FlightFov fov;for(int i=0;i<60;++i)fov.advance(1./60,true,true);
    check(std::abs(fov.multiplier-1.265)<1e-5,"sprint-flight FOV matches Java modifier");
    for(int i=0;i<60;++i)fov.advance(1./60,false,false);
    check(std::abs(fov.multiplier-1)<1e-5,"flight FOV returns to baseline");
    Combat c;auto full=c.strike(10,false,false);auto spam=c.strike(10,false,false);
    check(full.damage==10&&spam.damage==2,"attack cooldown limits spam");
    c.advance(1);check(c.strike(10,true,false).damage==15,"charged falling critical");
    c.advance(1);check(!c.strike(10,true,true).critical,"sprinting excludes critical");
    Recipe recipe{"test","",{{1,2},{1,2}},2,1,false};
    check(!canCraft(recipe,{{1,3}}),"duplicate ingredients aggregated");
    check(canCraft(recipe,{{1,4}}),"sufficient inventory accepted");
    recipe.spell=true;check(!canCraft(recipe,{{1,4}},true),"known spell cannot consume materials");
    recipe.inputs[0].count=-1;check(!canCraft(recipe,{{1,4}}),"invalid counts rejected");
    std::cout<<checks<<" checks passed\n";
}
