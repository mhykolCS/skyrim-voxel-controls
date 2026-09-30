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
    check(std::abs(d.velocity.horizontal()-a.velocity.horizontal())<1e-7,"no diagonal speed advantage");
    Input fly;fly.jump=true;
    auto f=simulate(60,fly,Mode::Creative,false);
    check(f.velocity.z>10.7&&f.velocity.z<10.9,"creative ascends at configured speed");
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
