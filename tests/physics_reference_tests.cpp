#include "voxel/core.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace voxel;

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream file(argv[1]);if(!file)return 2;
    std::string row,name;std::getline(file,row);
    Movement movement;Vec3 measured;double altitude=0;int rows=0;
    while(std::getline(file,row)) {
        std::istringstream line(row);std::string field;
        std::getline(line,field,',');const auto scenario=field;
        std::vector<double> values;while(std::getline(line,field,','))values.push_back(std::stod(field));
        if(values.size()!=13)return 2;
        const int tick=int(values[0]),mode=int(values[1]);const bool grounded=values[2]!=0;
        if(tick==0){movement.reset();movement.setMode(Mode(mode));measured={};altitude=grounded?0:1000;name=scenario;}
        Input input;input.forward=values[3];input.strafe=values[4];input.jump=values[5];
        input.descend=values[6];input.sprint=values[7];input.boost=values[8];input.pitch=values[9];
        movement.advance(.05,input,grounded,measured);
        Vec3 actual=movement.state.frameVelocity*.05;
        // The reference world and Havok resolve the floor, outside our model.
        actual.z=std::max(-altitude,actual.z);altitude+=actual.z;measured=actual*20;
        const Vec3 expected{values[10],values[11],values[12]};
        if((actual-expected).length()>2e-5){
            std::cerr<<name<<" tick "<<tick<<": got "<<actual.x<<","<<actual.y<<","<<actual.z
                <<" expected "<<expected.x<<","<<expected.y<<","<<expected.z<<'\n';return 1;
        }
        ++rows;
    }
    if(rows!=900)return 2;
    std::cout<<rows<<" independent Minecraft reference ticks passed\n";
}
