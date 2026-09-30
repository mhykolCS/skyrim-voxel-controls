#include <NifFile.hpp>
#include <array>
#include <filesystem>
#include <iostream>
using namespace nifly;
struct SkinRect {float x,y,w,h;};
using Faces=std::array<SkinRect,6>;
void box(NifFile& nif,const char* name,Vector3 pivot,Vector3 lo,Vector3 hi,const Faces& faces) {
    MatTransform transform;transform.translation=pivot;
    auto node=nif.AddNode(name,transform);
    std::vector<Vector3> vertices,normals;
    std::vector<Vector2> uvs;
    std::vector<Triangle> triangles;
    // Each face runs top-left, top-right, bottom-right, bottom-left as viewed
    // from outside. Skyrim faces +Y; Minecraft's skin atlas uses top-origin UVs.
    std::array<std::array<Vector3,4>,6> corners{{
      {{{lo.x,hi.y,hi.z},{hi.x,hi.y,hi.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z}}},
      {{{hi.x,hi.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z}}},
      {{{hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z}}},
      {{{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{lo.x,hi.y,lo.z},{lo.x,lo.y,lo.z}}},
      {{{lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}}},
      {{{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z}}}
    }};
    const std::array<Vector3,6> normal{{{0,1,0},{1,0,0},{0,-1,0},{-1,0,0},{0,0,1},{0,0,-1}}};
    for(int face=0;face<6;++face){
        auto r=faces[face];auto base=uint16_t(vertices.size());
        for(auto v:corners[face])vertices.push_back(v);
        for(int i=0;i<4;++i)normals.push_back(normal[face]);
        uvs.insert(uvs.end(),{{r.x/64,r.y/64},{(r.x+r.w)/64,r.y/64},{(r.x+r.w)/64,(r.y+r.h)/64},{r.x/64,(r.y+r.h)/64}});
        triangles.emplace_back(base,base+1,base+2);triangles.emplace_back(base,base+2,base+3);
    }
    auto shape=nif.CreateShapeFromData(std::string(name)+"Mesh",&vertices,&triangles,&uvs,&normals);
    nif.SetParentNode(shape,node);
    std::string diffuse="textures\\voxelcontrols\\steve.dds",normalMap="textures\\voxelcontrols\\steve_n.dds";
    nif.SetTextureSlot(shape,diffuse,0);nif.SetTextureSlot(shape,normalMap,1);
    auto shader=dynamic_cast<BSLightingShaderProperty*>(nif.GetShader(shape));
    shader->specularStrength=0;shader->specularColor={0,0,0};
}
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"Usage: make-steve OUTPUT.nif\n";return 2;}
    NifFile nif;nif.Create(NiVersion::getSSE());nif.GetRootNode()->name.get()="VoxelSteve";
    box(nif,"SteveHead",{0,0,96},{-16,-16,0},{16,16,32},{{{8,8,8,8},{0,8,8,8},{24,8,8,8},{16,8,8,8},{8,0,8,8},{16,0,8,8}}});
    box(nif,"SteveTorso",{0,0,48},{-16,-8,0},{16,8,48},{{{20,20,8,12},{16,20,4,12},{32,20,8,12},{28,20,4,12},{20,16,8,4},{28,16,8,4}}});
    box(nif,"SteveRightArm",{24,0,96},{-8,-8,-48},{8,8,0},{{{44,20,4,12},{40,20,4,12},{52,20,4,12},{48,20,4,12},{44,16,4,4},{48,16,4,4}}});
    box(nif,"SteveLeftArm",{-24,0,96},{-8,-8,-48},{8,8,0},{{{36,52,4,12},{32,52,4,12},{44,52,4,12},{40,52,4,12},{36,48,4,4},{40,48,4,4}}});
    box(nif,"SteveRightLeg",{8,0,48},{-8,-8,-48},{8,8,0},{{{4,20,4,12},{0,20,4,12},{12,20,4,12},{8,20,4,12},{4,16,4,4},{8,16,4,4}}});
    box(nif,"SteveLeftLeg",{-8,0,48},{-8,-8,-48},{8,8,0},{{{20,52,4,12},{16,52,4,12},{28,52,4,12},{24,52,4,12},{20,48,4,4},{24,48,4,4}}});
    auto result=nif.Save(std::filesystem::path(argv[1]));
    std::cout<<"Saved six-part classic player model, result "<<result<<"\n";return result;
}
