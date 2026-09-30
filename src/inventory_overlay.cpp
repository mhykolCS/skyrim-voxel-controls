#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <imgui.h>
#include <d3d11.h>
#include <DDSTextureLoader.h>
#include <cmath>
namespace voxel {
namespace {
constexpr ImU32 ink=IM_COL32(46,43,40,255),dim=IM_COL32(94,91,85,255),white=IM_COL32(255,255,255,255);
constexpr ImU32 panel=IM_COL32(198,198,198,255),slotFill=IM_COL32(139,139,139,255),accent=IM_COL32(104,168,133,255);
float scale=1,originX{},originY{};
ItemKey selected;
ItemFilter filter=ItemFilter::All;
ItemSort sorting=ItemSort::Name;
int page=0,chosenRecipe=0;
char search[128]{},recipeSearch[128]{};
bool showIds=false,wasOpen=false;
ID3D11ShaderResourceView* steve{};
float skinWidth=64,skinHeight=64;
bool triedSkin=false;
ImVec2 point(float x,float y){return {originX+x*scale,originY+y*scale};}
void box(float x,float y,float w,float h,ImU32 color){ImGui::GetWindowDrawList()->AddRectFilled(point(x,y),point(x+w,y+h),color);}
void text(float x,float y,std::string_view value,ImU32 color=ink,float size=18,float wrap=0) {
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),size*scale,point(x,y),color,value.data(),value.data()+value.size(),wrap*scale);
}
void bevel(float x,float y,float w,float h,ImU32 fill=panel,bool inset=false) {
    box(x,y,w,h,IM_COL32(30,30,30,255));box(x+2,y+2,w-4,h-4,fill);
    const auto bright=inset?IM_COL32(63,63,63,255):IM_COL32(246,246,246,255);
    const auto dark=inset?IM_COL32(238,238,238,255):IM_COL32(86,86,86,255);
    box(x+2,y+2,w-4,3,bright);box(x+2,y+2,3,h-4,bright);
    box(x+3,y+h-5,w-5,3,dark);box(x+w-5,y+3,3,h-5,dark);
}
bool hit(const char* id,float x,float y,float w,float h) {
    ImGui::SetCursorScreenPos(point(x,y));return ImGui::InvisibleButton(id,{w*scale,h*scale},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
}
bool button(const char* id,std::string_view label,float x,float y,float w,float h,bool enabled=true,bool on=false) {
    ImGui::PushID(id);const bool pressed=hit("button",x,y,w,h);const bool hover=ImGui::IsItemHovered();
    bevel(x,y,w,h,!enabled?IM_COL32(148,148,148,255):on?IM_COL32(144,184,155,255):hover?IM_COL32(213,217,226,255):IM_COL32(172,172,172,255),pressed);
    const auto size=ImGui::GetFont()->CalcTextSizeA(17*scale,10000,0,label.data(),label.data()+label.size());
    text(x+(w-size.x/scale)/2,y+(h-17)/2,label,enabled?ink:dim,17);
    ImGui::PopID();return enabled&&pressed&&ImGui::IsMouseReleased(ImGuiMouseButton_Left);
}
bool confirmation(const char* title) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x*.5f,ImGui::GetIO().DisplaySize.y*.5f),ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({440*scale,0},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18*scale,14*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12*scale,7*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,2*scale);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{.776f,.776f,.776f,1});
    ImGui::PushStyleColor(ImGuiCol_TitleBg,{.57f,.57f,.57f,1});ImGui::PushStyleColor(ImGuiCol_TitleBgActive,{.57f,.57f,.57f,1});
    ImGui::PushStyleColor(ImGuiCol_Text,{.18f,.17f,.16f,1});ImGui::PushStyleColor(ImGuiCol_Border,{.18f,.18f,.18f,1});
    ImGui::PushStyleColor(ImGuiCol_Button,{.65f,.65f,.65f,1});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.84f,.87f,.84f,1});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.56f,.72f,.6f,1});
    if(ImGui::BeginPopupModal(title,nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove))return true;
    ImGui::PopStyleColor(8);ImGui::PopStyleVar(4);return false;
}
void endConfirmation(){ImGui::EndPopup();ImGui::PopStyleColor(8);ImGui::PopStyleVar(4);}
void icon(ItemIcon type,float x,float y,float size,ImU32 tint=IM_COL32(180,195,205,255)) {
    const float u=size/24;
    auto p=[&](float a,float b,float w,float h,ImU32 c){box(x+a*u,y+b*u,w*u,h*u,c);};
    const auto edge=IM_COL32(46,43,44,255),steel=IM_COL32(207,221,221,255),steelDark=IM_COL32(116,140,152,255),wood=IM_COL32(127,85,49,255),gold=IM_COL32(240,189,60,255);
    switch(type) {
        case ItemIcon::Sword:case ItemIcon::Dagger:
            for(int i=0;i<14;i+=2){p(4+i,17-i,4,4,edge);p(5+i,17-i,2,3,i>4?steel:wood);}p(3,15,8,2,steelDark);p(8,18,2,3,steelDark);break;
        case ItemIcon::Axe:case ItemIcon::Mace:
            p(10,3,4,19,edge);p(11,4,2,17,wood);p(5,2,14,8,edge);p(6,3,11,6,steelDark);p(6,3,9,3,steel);if(type==ItemIcon::Axe){p(4,7,6,5,steel);p(17,2,2,9,edge);}break;
        case ItemIcon::Bow:
            p(7,2,3,3,wood);p(10,4,3,4,wood);p(12,8,3,8,wood);p(10,16,3,4,wood);p(7,19,3,3,wood);p(7,3,1,18,steel);p(3,11,17,2,wood);p(18,10,3,4,steel);break;
        case ItemIcon::Staff:
            p(10,5,4,18,edge);p(11,6,2,17,wood);p(7,1,10,8,edge);p(8,2,8,6,IM_COL32(150,101,214,255));p(9,2,3,3,IM_COL32(218,178,249,255));break;
        case ItemIcon::Helmet:
            p(3,5,18,15,edge);p(4,4,16,12,steelDark);p(6,4,12,5,steel);p(7,13,10,7,slotFill);p(5,10,14,3,edge);p(10,10,3,10,steel);break;
        case ItemIcon::Chest:
            p(3,3,18,7,edge);p(5,5,14,16,edge);p(4,4,16,5,steelDark);p(6,8,12,12,steelDark);p(7,7,9,8,steel);p(9,3,6,4,slotFill);p(10,16,2,4,steel);break;
        case ItemIcon::Gloves:
            p(3,8,8,13,edge);p(13,8,8,13,edge);p(4,9,6,11,wood);p(14,9,6,11,wood);p(4,7,6,4,steel);p(14,7,6,4,steel);p(8,12,4,5,wood);p(12,12,4,5,wood);break;
        case ItemIcon::Boots:
            p(3,3,7,18,edge);p(14,3,7,18,edge);p(1,16,8,5,edge);p(14,16,9,5,edge);p(4,4,5,14,wood);p(15,4,5,14,wood);p(2,17,7,3,steelDark);p(15,17,7,3,steelDark);break;
        case ItemIcon::Shield:
            p(3,3,18,14,edge);p(6,17,12,3,edge);p(9,20,6,2,edge);p(4,4,16,12,wood);p(7,16,10,3,wood);p(9,7,6,7,steel);p(10,5,3,13,steelDark);break;
        case ItemIcon::Ring:case ItemIcon::Amulet:
            p(5,6,14,14,edge);p(7,5,10,17,gold);p(4,8,17,10,gold);p(8,9,9,9,slotFill);p(9,4,7,7,IM_COL32(89,192,186,255));p(10,5,3,3,white);if(type==ItemIcon::Amulet){p(5,1,2,7,gold);p(17,1,2,7,gold);}break;
        case ItemIcon::Potion:
            p(8,1,8,4,edge);p(9,1,6,3,wood);p(8,4,8,6,steel);p(4,10,16,10,edge);p(6,8,12,14,edge);p(5,11,14,8,tint);p(7,9,10,12,tint);p(7,10,3,8,IM_COL32(241,231,244,255));p(9,5,5,4,IM_COL32(202,231,224,255));break;
        case ItemIcon::Food:
            p(4,8,16,12,edge);p(6,5,12,17,edge);p(5,9,14,10,IM_COL32(192,93,60,255));p(7,6,10,15,IM_COL32(214,129,72,255));p(8,8,3,7,IM_COL32(245,175,103,255));p(12,3,2,5,wood);break;
        case ItemIcon::Leaf:
            p(11,7,2,16,IM_COL32(57,95,39,255));p(4,8,7,5,IM_COL32(74,137,48,255));p(13,13,7,5,IM_COL32(106,157,62,255));p(7,3,10,7,IM_COL32(157,172,215,255));p(10,1,4,11,IM_COL32(125,147,202,255));p(10,5,4,4,gold);break;
        case ItemIcon::Book:case ItemIcon::Scroll:
            p(4,3,17,19,edge);p(3,2,16,18,type==ItemIcon::Book?IM_COL32(133,73,52,255):IM_COL32(231,210,155,255));p(6,4,12,14,IM_COL32(225,207,164,255));p(8,6,8,1,wood);p(8,9,6,1,wood);p(8,12,8,1,wood);p(4,3,2,17,IM_COL32(102,67,49,255));break;
        case ItemIcon::Arrow:
            for(int i=0;i<18;++i)p(3+i,20-i,2,2,wood);p(17,2,5,5,steel);p(17,2,6,2,steel);p(2,16,4,6,steel);break;
        case ItemIcon::Key:case ItemIcon::Lockpick:
            p(4,12,17,3,type==ItemIcon::Key?gold:steel);p(4,7,8,10,type==ItemIcon::Key?gold:steel);p(6,9,4,5,slotFill);p(16,14,2,5,gold);p(20,14,2,4,gold);break;
        case ItemIcon::Ingot:
            p(2,12,20,8,edge);p(5,7,14,12,steelDark);p(3,13,18,5,steel);p(7,7,10,5,steel);p(5,10,14,3,white);break;
        case ItemIcon::Gem:
            p(5,3,14,4,edge);p(2,7,20,9,edge);p(7,16,10,5,edge);p(10,20,4,3,edge);p(6,4,12,4,IM_COL32(190,151,223,255));p(3,8,18,7,IM_COL32(137,101,195,255));p(8,15,8,5,IM_COL32(100,75,154,255));p(6,6,5,7,IM_COL32(224,199,242,255));break;
        case ItemIcon::Coin:
            p(5,3,14,19,edge);p(3,6,18,13,edge);p(6,4,12,17,gold);p(4,7,16,11,gold);p(8,6,8,2,IM_COL32(255,238,132,255));p(10,9,3,9,wood);break;
        case ItemIcon::Torch:
            p(10,9,4,14,wood);p(6,4,12,8,IM_COL32(234,122,41,255));p(9,1,6,13,IM_COL32(255,205,63,255));p(10,5,4,5,white);break;
        default:p(6,3,12,4,edge);p(8,7,8,3,wood);p(4,10,16,11,edge);p(6,9,12,13,wood);p(7,11,5,8,IM_COL32(175,139,85,255));break;
    }
}
void tooltip(const ItemView& item,bool inventoryActions=true) {
    ImGui::PushStyleColor(ImGuiCol_PopupBg,{.075f,.025f,.12f,.98f});ImGui::PushStyleColor(ImGuiCol_Border,{.35f,.18f,.65f,1});
    ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);
    ImGui::TextColored(item.enchanted?ImVec4(.8f,.58f,1,1):ImVec4(1,1,1,1),"%s",item.name.c_str());ImGui::TextColored({.5f,.6f,1,1},"%s",item.source.c_str());
    if(item.damage>0)ImGui::Text("Damage %.1f",item.damage);if(item.armor>0)ImGui::Text("Base armor %.1f",item.armor);
    ImGui::Text("Weight %.1f   Value %d   Count %d",item.weight,item.value,item.count);
    if(item.equipped)ImGui::TextColored({.5f,1,.55f,1},"Equipped");if(item.quest)ImGui::TextColored({1,.8f,.35f,1},"Quest item - protected");
    if(!item.description.empty())ImGui::TextWrapped("%s",item.description.c_str());if(showIds)ImGui::TextDisabled("Form %08X",item.key.form);
    if(inventoryActions){if(item.usable)ImGui::TextDisabled("Right click: %s",item.verb.c_str());ImGui::TextDisabled("Drag to quick bar / R: recipe uses");}else ImGui::TextDisabled("Click to inspect and choose an action");
    ImGui::PopTextWrapPos();ImGui::EndTooltip();ImGui::PopStyleColor(2);
}
void chooseUses(const ItemView& item) {
    selected=item.key;
    for(std::size_t i=0;i<recipes().size();++i){const auto& recipe=recipes()[i];if(recipe.output==item.key.form||std::any_of(recipe.inputs.begin(),recipe.inputs.end(),[&](auto v){return v.form==item.key.form;})){chosenRecipe=int(i);return;}}
}
void slot(const char* id,const ItemView* item,float x,float y,float size=60,int quick=-1,int ghost=-1,bool actionable=true) {
    ImGui::PushID(id);bool clicked=hit("slot",x,y,size,size);bool hovered=ImGui::IsItemHovered();
    bevel(x,y,size,size,hovered?IM_COL32(174,174,174,255):slotFill,true);
    if(item) {
        icon(item->icon,x+9,y+7,size-18,item->tint);
        if(item->equipped)box(x+5,y+5,7,7,IM_COL32(97,223,124,255));if(item->enchanted)box(x+size-9,y+5,4,4,IM_COL32(205,133,255,255));if(item->quest)box(x+size-15,y+5,4,4,IM_COL32(255,219,113,255));
        if(item->count>1){const auto count=std::to_string(item->count);float width=float(count.size())*9;text(x+size-width-5,y+size-21,count,IM_COL32(40,40,40,255),18);text(x+size-width-6,y+size-22,count,white,18);}
        if(item->key==selected)ImGui::GetWindowDrawList()->AddRect(point(x+2,y+2),point(x+size-2,y+size-2),IM_COL32(255,236,154,255),0,0,2*scale);
    } else if(ghost>=0){icon(ItemIcon(ghost),x+12,y+12,size-24);box(x+6,y+6,size-12,size-12,IM_COL32(139,139,139,135));}
    if(quick>=0)text(x+7,y+size-19,std::to_string(quick+1),IM_COL32(228,228,228,255),13);
    if(actionable&&item) {
        if(clicked&&ImGui::IsMouseReleased(ImGuiMouseButton_Left))selected=item->key;
        if(hovered&&item->usable&&(ImGui::IsMouseClicked(ImGuiMouseButton_Right)||ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))enqueue(Action::UseItem,0,item->key);
        if(hovered&&ImGui::IsKeyPressed(ImGuiKey_R)&&!ImGui::GetIO().WantTextInput)chooseUses(*item);
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)){ImGui::SetDragDropPayload("VOXEL_ITEM",&item->key,sizeof(ItemKey));ImGui::TextUnformatted(item->name.c_str());ImGui::EndDragDropSource();}
    }
    if(quick>=0) {
        if(ImGui::BeginDragDropTarget()){if(auto payload=ImGui::AcceptDragDropPayload("VOXEL_ITEM"))enqueue(Action::PinItem,quick,*static_cast<const ItemKey*>(payload->Data));ImGui::EndDragDropTarget();}
        if(clicked&&!item&&selected)enqueue(Action::PinItem,quick,selected);if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Middle))enqueue(Action::PinItem,quick);
    }
    if(hovered&&item)tooltip(*item);ImGui::PopID();
}
void stevePreview(void* raw,float x,float y,float w,float h) {
    if(!triedSkin){triedSkin=true;ID3D11Resource* resource{};
        if(SUCCEEDED(DirectX::CreateDDSTextureFromFile(static_cast<ID3D11Device*>(raw),L"Data\\textures\\VoxelControls\\steve.dds",&resource,&steve))&&resource){ID3D11Texture2D* texture{};
            if(SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture))))texture->Release();
            // prepare-steve scales the 64x64 skin to 1024x1024; UVs stay normalized to the source atlas.
            resource->Release();}}
    bevel(x,y,w,h,IM_COL32(35,37,38,255),true);box(x+20,y+h-18,w-40,5,IM_COL32(21,23,23,255));
    const float unit=(h-30)/32,cx=x+w/2;
    auto part=[&](float px,float py,float pw,float ph,float u,float v,float uw,float vh,ImU32 fallback){const auto min=point(cx+px*unit,y+12+py*unit),max=point(cx+(px+pw)*unit,y+12+(py+ph)*unit);
        if(steve)ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(steve),min,max,{u/skinWidth,v/skinHeight},{(u+uw)/skinWidth,(v+vh)/skinHeight});else ImGui::GetWindowDrawList()->AddRectFilled(min,max,fallback);};
    part(-4,0,8,8,8,8,8,8,IM_COL32(179,134,103,255));part(-4,8,8,12,20,20,8,12,IM_COL32(33,160,164,255));
    part(-8,8,4,12,44,20,4,12,IM_COL32(151,115,84,255));part(4,8,4,12,44,20,4,12,IM_COL32(151,115,84,255));
    part(-4,20,4,12,4,20,4,12,IM_COL32(64,59,158,255));part(0,20,4,12,4,20,4,12,IM_COL32(64,59,158,255));
}
void inputBox(const char* id,const char* hint,char* value,std::size_t length,float x,float y,float width) {
    ImGui::SetCursorScreenPos(point(x,y));ImGui::SetNextItemWidth(width*scale);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{8*scale,7*scale});
    ImGui::PushStyleColor(ImGuiCol_FrameBg,{.16f,.16f,.17f,1});ImGui::PushStyleColor(ImGuiCol_Text,{.94f,.94f,.9f,1});
    ImGui::InputTextWithHint(id,hint,value,length);ImGui::PopStyleColor(2);ImGui::PopStyleVar();
}
void meter(float x,float y,float width,float current,float maximum,ImU32 color,std::string_view label) {
    text(x,y,label,dim,15);bevel(x,y+21,width,19,IM_COL32(70,69,65,255),true);box(x+4,y+25,(width-8)*std::clamp(current/std::max(1.f,maximum),0.f,1.f),11,color);
    text(x+width-float(std::to_string(int(current)).size())*9-4,y,std::to_string(int(current)),ink,16);
}
}
void drawInventory(const Snapshot& state,void* device) {
    // Layout is drawn in Minecraft-style logical pixels and scaled uniformly.
    if(!state.inventoryOpen){wasOpen=false;return;}
    const auto display=ImGui::GetIO().DisplaySize;
    scale=std::clamp(std::min((display.x-48)/1268.f,(display.y-76)/820.f),.65f,1.45f);
    originX=(display.x-1268*scale)/2;originY=(display.y-820*scale)/2;
    if(!wasOpen){page=0;selected={};wasOpen=true;}
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("Voxel inventory",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(scale);
    ImGui::GetWindowDrawList()->AddRectFilled({0,0},display,IM_COL32(10,12,16,180));
    bevel(0,0,232,820);bevel(248,0,660,820);bevel(924,0,344,820);
    const auto& view=state.inventory;
    text(20,20,"Character",ink,23);text(20,54,view.playerName,ink,20,190);
    text(20,88,"Level "+std::to_string(view.level)+" / "+view.raceName,dim,16,190);
    meter(20,135,192,state.health,view.maxHealth,IM_COL32(198,61,65,255),"HEALTH");
    meter(20,185,192,state.magicka,view.maxMagicka,IM_COL32(70,117,202,255),"MAGICKA");
    meter(20,235,192,state.stamina,view.maxStamina,IM_COL32(98,161,66,255),"STAMINA");
    text(20,295,"Armor   "+std::to_string(int(view.armor)),ink,18);text(20,323,"Damage  "+std::to_string(int(view.damage)),ink,18);
    icon(ItemIcon::Coin,17,354,25);text(50,358,std::to_string(view.gold)+" gold",ink,18);
    text(20,401,"CARRY WEIGHT",dim,15);
    text(20,425,std::to_string(int(view.carryWeight))+" / "+std::to_string(int(view.carryLimit)),view.carryWeight>view.carryLimit?IM_COL32(154,45,44,255):ink,22);
    bevel(20,456,192,16,IM_COL32(78,75,70,255),true);box(24,460,184*std::clamp(view.carryWeight/std::max(1.f,view.carryLimit),0.f,1.f),8,view.carryWeight>view.carryLimit?IM_COL32(198,72,60,255):accent);
    box(20,493,192,2,IM_COL32(140,140,140,255));
    const auto item=findItem(view,selected);text(20,512,item?"Selected item":"Item details",ink,20);
    if(item) {
        icon(item->icon,20,549,40,item->tint);text(70,548,item->name,item->enchanted?IM_COL32(117,64,164,255):ink,17,137);
        text(20,609,item->source,IM_COL32(57,84,155,255),15,192);
        text(20,638,std::format("Weight {:.1f} / Value {}",item->weight,item->value),dim,15,192);
        if(item->quest)text(20,666,"QUEST ITEM",IM_COL32(125,88,20,255),16);else if(item->equipped)text(20,666,"EQUIPPED",IM_COL32(37,102,57,255),16);
        if(button("use",item->usable?item->verb:"No direct use",20,702,192,35,item->usable))enqueue(Action::UseItem,0,item->key);
        if(button("drop","Drop one",20,745,92,32,item->droppable))ImGui::OpenPopup("Drop one item?");
        if(button("uses","Uses [R]",120,745,92,32))chooseUses(*item);
    } else {text(20,553,"Select a slot to inspect an item. Hover for effects and source information.",dim,17,190);text(20,652,"Green: equipped\nPurple: enchanted\nGold: quest item",dim,16,190);}
    text(270,20,"Inventory",ink,24);text(270,52,"Equipment",dim,16);text(590,52,chosenRecipe<3?"Portable alchemy":"Spell matrix",dim,16);
    if(button("close","X",860,17,28,28))enqueue(Action::CloseInventory);
    const char* equipmentNames[]{"Head","Body","Hands","Feet","Amulet","Ring","Main hand","Off hand"};
    const ItemIcon equipmentIcons[]{ItemIcon::Helmet,ItemIcon::Chest,ItemIcon::Gloves,ItemIcon::Boots,ItemIcon::Amulet,ItemIcon::Ring,ItemIcon::Sword,ItemIcon::Shield};
    for(int i=0;i<8;++i){const float x=i<4?270.f:514.f,y=82.f+float(i%4)*57;slot(equipmentNames[i],findItem(view,view.equipment[i]),x,y,52,-1,int(equipmentIcons[i]));}
    stevePreview(device,338,82,160,223);
    const auto& recipe=recipes()[chosenRecipe];const auto* recipeView=chosenRecipe<int(state.recipes.size())?&state.recipes[chosenRecipe]:nullptr;
    for(int i=0;i<4;++i){ItemView input;const ItemView* shown=nullptr;
        if(i<2&&chosenRecipe*3+1+i<int(state.recipeItems.size())){input=state.recipeItems[chosenRecipe*3+1+i];if(recipeView)input.count=recipeView->counts[i];shown=&input;}
        const std::string id="matrix"+std::to_string(i);slot(id.c_str(),shown,590+float(i%2)*56,83+float(i/2)*56,52,-1,-1,false);
        if(i<2&&recipeView)text(597+float(i)*56,204,std::to_string(recipeView->counts[i])+" / 1",recipeView->counts[i]>0?IM_COL32(40,105,53,255):IM_COL32(153,51,50,255),15);
    }
    text(710,115,">",dim,32);const ItemView* output=chosenRecipe*3<int(state.recipeItems.size())?&state.recipeItems[chosenRecipe*3]:nullptr;
    slot("output",output,745,103,68,-1,-1,false);text(590,235,recipe.name,ink,17,283);
    const bool craftable=recipeView&&recipeView->available;
    if(button("craft",recipeView&&recipeView->learned?"Learned":recipe.spell?"Create + learn":"Craft one",590,271,288,34,craftable))enqueue(Action::Craft,chosenRecipe);
    const char* filters[]{"All","Gear","Supplies","Books","Materials","Keys"};
    for(int i=0;i<6;++i)if(button(filters[i],filters[i],270+float(i)*103,330,98,30,true,int(filter)==i)){filter=ItemFilter(i);page=0;}
    auto indices=inventoryIndices(view.items,filter,search,sorting);const int pages=int(inventoryPageCount(indices.size()));page=std::clamp(page,0,pages-1);
    text(270,376,"Backpack",dim,17);text(560,376,std::to_string(indices.size())+" stacks",dim,16);
    if(button("previous","<",748,370,30,28,page>0))--page;text(787,376,std::to_string(page+1)+" / "+std::to_string(pages),dim,15);
    if(button("next",">",858,370,30,28,page+1<pages))++page;
    for(int i=0;i<27;++i){const auto index=std::size_t(page*27+i);const ItemView* entry=index<indices.size()?&view.items[indices[index]]:nullptr;
        const auto id="bag"+std::to_string(i);slot(id.c_str(),entry,270+float(i%9)*68,408+float(i/9)*66,62);}
    if(indices.empty())text(390,487,"No matching items",white,20);
    text(270,623,"Quick bar",dim,17);text(423,626,"Drag to pin / click an empty slot to pin selection",dim,13);
    for(int i=0;i<9;++i){const auto id="quick"+std::to_string(i);slot(id.c_str(),findItem(view,view.quickSlots[i]),270+float(i)*68,650,62,i);}
    inputBox("##inventory-search","Search inventory...  @source",search,sizeof(search),270,733,392);
    const char* sortNames[]{"Name","Weight","Value"};
    if(button("sort",std::string("Sort: ")+sortNames[int(sorting)],674,733,146,36)){sorting=ItemSort((int(sorting)+1)%3);page=0;}
    if(button("clear","Clear",828,733,60,36)){search[0]=0;page=0;}
    text(270,787,"E / Esc / Tab: close     Right click: use     Middle click: unpin",dim,14);
    text(944,20,"Recipe browser",ink,23);text(944,52,"Alchemy + spell matrices",dim,16);
    inputBox("##recipe-search","Search recipes...",recipeSearch,sizeof(recipeSearch),944,84,304);
    float recipeY=143;int matches=0;
    for(int i=0;i<int(recipes().size());++i){const auto& r=recipes()[i];if(lowerText(r.name).find(lowerText(recipeSearch))==std::string::npos)continue;
        const auto id="recipe"+std::to_string(i);bool click=hit(id.c_str(),944,recipeY,304,64);bool hover=ImGui::IsItemHovered();
        bevel(944,recipeY,304,64,i==chosenRecipe?IM_COL32(162,184,167,255):hover?IM_COL32(216,216,216,255):IM_COL32(185,185,185,255));
        if(i*3<int(state.recipeItems.size()))icon(state.recipeItems[i*3].icon,955,recipeY+10,42,state.recipeItems[i*3].tint);
        text(1008,recipeY+10,r.name,ink,17,224);const auto* rv=i<int(state.recipes.size())?&state.recipes[i]:nullptr;
        text(1008,recipeY+39,rv&&rv->learned?"Learned":rv&&rv->available?"Ready to craft":"Missing ingredients",rv&&rv->available?IM_COL32(31,99,46,255):dim,14);
        if(click)chosenRecipe=i;recipeY+=70;++matches;
    }
    if(!matches)text(957,173,"No matching recipes",dim,18);
    box(944,589,304,2,IM_COL32(140,140,140,255));text(944,608,"Recipe information",ink,19);text(944,640,recipe.description,dim,16,304);
    text(944,704,recipe.spell?"Consumes reagents once. Teaches a real Skyrim spell.":"Uses carried ingredients. Output enters your Skyrim inventory.",dim,15,304);
    if(button("native","More item actions",944,772,195,30))enqueue(Action::NativeInventory);
    if(button("ids","IDs",1150,772,98,30,true,showIds))showIds=!showIds;
    if(!state.status.empty())ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(),17*scale,point(250,833),white,state.status.c_str());
    ImGui::SetNextWindowPos({display.x/2,display.y/2},ImGuiCond_Appearing,{.5f,.5f});
    if(confirmation("Drop one item?")) {
        ImGui::TextUnformatted(item?item->name.c_str():"Item no longer available");ImGui::TextUnformatted("Drop one into the world?");
        if(ImGui::Button("Drop one")&&item&&item->droppable){enqueue(Action::DropItem,0,item->key);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();endConfirmation();
    }
    ImGui::End();ImGui::PopStyleVar(2);
}
void drawNativeMenus(const Snapshot& state) {
    const auto& view=state.nativeMenu;if(view.kind==MenuKind::None)return;
    const auto display=ImGui::GetIO().DisplaySize;
    scale=std::clamp(std::min((display.x-48)/1268.f,(display.y-76)/820.f),.65f,1.45f);
    originX=(display.x-1268*scale)/2;originY=(display.y-820*scale)/2;
    static std::uint64_t previousSession{};static ItemKey selection;
    static char query[128]{};static int externalPage{},playerPage{},topicPage{},quantity=1;
    static ItemFilter category=ItemFilter::All;
    if(previousSession!=view.session){previousSession=view.session;selection={};query[0]=0;externalPage=playerPage=topicPage=0;quantity=1;category=ItemFilter::All;}
    auto send=[&](MenuAction action,int value=0,ItemKey item={},int price=0,std::string valueText={}){enqueueMenu({action,view.session,item,value,price,std::move(valueText)});};
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin("Voxel native menus",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(scale);
    if(view.kind==MenuKind::Dialogue) {
        // Anchor to the right edge, outside the normal dialogue camera's face.
        originX=display.x-1268*scale-28;
        const float panelHeight=std::max(530.f,250.f+82.f*float(std::min<std::size_t>(6,view.topics.size()))),panelTop=820-panelHeight;
        bevel(638,panelTop,630,panelHeight);text(664,panelTop+23,view.title.empty()?"Conversation":view.title,ink,26,566);
        text(664,panelTop+65,view.choicesReady?"Choose a response":"Listening...",dim,17);
        const int pages=std::max(1,int((view.topics.size()+5)/6));topicPage=std::clamp(topicPage,0,pages-1);
        for(int row=0;row<6;++row){const int index=topicPage*6+row;if(index>=int(view.topics.size()))break;const auto& topic=view.topics[index];
            const float y=panelTop+108+float(row)*82;const auto id="topic"+std::to_string(index);bool pressed=hit(id.c_str(),660,y,586,72);
            bevel(660,y,586,72,view.choicesReady?(ImGui::IsItemHovered()?IM_COL32(216,226,211,255):panel):IM_COL32(159,159,159,255));
            box(666,y+6,4,60,topic.fresh?accent:IM_COL32(121,120,118,255));
            text(680,y+15,index<9?std::to_string(index+1):">",dim,18);
            text(714,y+13,topic.text,view.choicesReady?ink:dim,18,514);
            if(pressed&&view.choicesReady&&ImGui::IsMouseReleased(ImGuiMouseButton_Left))send(MenuAction::Topic,topic.index,{},0,topic.text);
        }
        if(pages>1){if(button("topicprev","<",664,690,40,30,topicPage>0))--topicPage;text(720,696,std::to_string(topicPage+1)+" / "+std::to_string(pages),dim,16);if(button("topicnext",">",800,690,40,30,topicPage+1<pages))++topicPage;}
        if(button("skip","Continue [Space]",664,750,274,44,view.canSkip))send(MenuAction::Skip);
        if(button("goodbye","Leave [Tab]",952,750,292,44,view.canExit))send(MenuAction::Close);
        if(!view.subtitle.empty()&&view.subtitle!=" "){originX=(display.x-850*scale)/2;bevel(0,626,850,194,IM_COL32(40,40,46,250));text(22,647,view.title,IM_COL32(157,211,163,255),21);text(22,684,view.subtitle,white,21,806);}
    } else if(view.kind==MenuKind::Hub) {
        ImGui::GetWindowDrawList()->AddRectFilled({0,0},display,IM_COL32(10,12,16,155));
        bevel(150,96,968,630);text(184,127,"Adventure menu",ink,31);text(184,173,view.playerName+" / Level "+std::to_string(view.level),dim,20);
        text(770,139,std::to_string(view.gold)+" gold",ink,21);text(770,175,std::format("Carry {:.0f} / {:.0f}",view.carryWeight,view.carryLimit),dim,18);
        struct Hub {const char* title;const char* info;ItemIcon icon;int entry;};
        const Hub entries[]{{"Inventory","Equipment, items and supplies",ItemIcon::Chest,3},{"Magic","Spells and the spell matrix",ItemIcon::Scroll,2},{view.levelUp?"Level up":"Skills","Perks, skills and advancement",ItemIcon::Sword,1},{"World map","Locations, markers and travel",ItemIcon::Book,4}};
        for(int i=0;i<4;++i){const float x=184.f+float(i%2)*458,y=230.f+float(i/2)*164;auto& option=entries[i];const auto id="hub"+std::to_string(i);
            bool pressed=hit(id.c_str(),x,y,436,145);bevel(x,y,436,145,ImGui::IsItemHovered()?IM_COL32(215,221,213,255):panel);
            bevel(x+18,y+25,88,88,slotFill,true);icon(option.icon,x+31,y+36,63);text(x+125,y+33,option.title,ink,25);text(x+125,y+80,option.info,dim,17,280);
            if(pressed&&ImGui::IsMouseReleased(ImGuiMouseButton_Left))send(MenuAction::HubEntry,option.entry);
        }
        if(button("resume","Back to game [Tab]",184,588,894,52))send(MenuAction::Close);
        text(184,666,"E Inventory     F Interact     F3 Debug     F5 Camera     F8 Workbench",dim,17);
    } else {
        ImGui::GetWindowDrawList()->AddRectFilled({0,0},display,IM_COL32(10,12,16,180));
        bevel(0,0,232,820);bevel(248,0,660,820);bevel(924,0,344,820);
        const bool trade=view.kind==MenuKind::Barter,own=view.kind==MenuKind::Inventory;
        text(20,22,trade?"Trading":own?"Character":"Storage",ink,25);text(20,64,view.playerName,ink,20,190);
        text(20,104,"Level "+std::to_string(view.level),dim,18);icon(ItemIcon::Coin,18,151,27);text(55,156,std::to_string(view.gold)+" gold",ink,20);
        if(trade){text(20,211,"Merchant gold",dim,16);text(20,240,std::to_string(view.merchantGold),ink,24);}
        text(20,306,"CARRY WEIGHT",dim,15);text(20,336,std::format("{:.0f} / {:.0f}",view.carryWeight,view.carryLimit),view.carryWeight>view.carryLimit?IM_COL32(170,48,44,255):ink,25);
        bevel(20,376,192,18,slotFill,true);box(24,380,184*std::clamp(view.carryWeight/std::max(1.f,view.carryLimit),0.f,1.f),10,accent);
        text(20,448,trade?"Buy and sell":view.containerMode==2?"Pickpocketing":view.containerMode==1?"Owned container":"Item browser",ink,20,190);
        text(20,492,trade?"Select an item to see its price and choose a quantity.":view.containerMode==2?"Taking items may be noticed. The chance comes from Skyrim.":view.containerMode==1?"Taking owned items counts as stealing.":"Select an item in either grid. Use the action panel to move or equip it.",dim,18,188);
        text(20,717,"E / Tab / Esc: close\nSearch by name or @source",dim,16,190);
        text(270,21,view.title.empty()?"Inventory":view.title,ink,25,560);
        if(button("native-close","X",860,17,28,28))send(MenuAction::Close);
        inputBox("##native-search","Search items...  @source",query,sizeof(query),270,66,610);
        const char* filters[]{"All","Gear","Supplies","Books","Materials","Keys"};
        for(int i=0;i<6;++i)if(button(filters[i],filters[i],270+float(i)*103,116,98,29,true,int(category)==i)){category=ItemFilter(i);externalPage=playerPage=0;}
        const NativeItemView* chosen=nullptr;for(const auto& item:view.items)if(item.item.key==selection){chosen=&item;break;}
        auto grid=[&](bool playerSide,float y,int& currentPage,const char* label){
            std::vector<const NativeItemView*> items;for(const auto& item:view.items)if(item.player==playerSide&&itemMatches(item.item,category,query))items.push_back(&item);
            std::stable_sort(items.begin(),items.end(),[](auto a,auto b){return lowerText(a->item.name)<lowerText(b->item.name);});
            const int pages=int(inventoryPageCount(items.size()));currentPage=std::clamp(currentPage,0,pages-1);
            text(270,y,label,dim,18);text(566,y,std::to_string(items.size())+" stacks",dim,15);
            const std::string prefix=playerSide?"player":"external";
            if(button((prefix+"prev").c_str(),"<",750,y-5,30,27,currentPage>0))--currentPage;
            text(792,y,std::to_string(currentPage+1)+"/"+std::to_string(pages),dim,15);
            if(button((prefix+"next").c_str(),">",858,y-5,30,27,currentPage+1<pages))++currentPage;
            for(int i=0;i<27;++i){int index=currentPage*27+i;auto item=index<int(items.size())?items[index]:nullptr;const float x=270.f+float(i%9)*68,sy=y+34+float(i/9)*66;
                const auto id=prefix+std::to_string(i);bool pressed=hit(id.c_str(),x,sy,62,62);const bool hovered=ImGui::IsItemHovered();
                bevel(x,sy,62,62,hovered?IM_COL32(177,177,177,255):slotFill,true);
                if(item){icon(item->item.icon,x+9,sy+7,44,item->item.tint);if(item->item.count>1)text(x+55-float(std::to_string(item->item.count).size())*9,sy+40,std::to_string(item->item.count),white,18);
                    if(item->item.equipped)box(x+5,sy+5,6,6,IM_COL32(97,223,124,255));if(item->item.quest)box(x+48,sy+5,6,6,IM_COL32(255,219,113,255));
                    if(item->item.key==selection)ImGui::GetWindowDrawList()->AddRect(point(x+2,sy+2),point(x+60,sy+60),IM_COL32(255,236,154,255),0,0,2*scale);
                    if(!item->enabled)box(x+4,sy+4,54,54,IM_COL32(40,40,40,115));
                    if(pressed&&ImGui::IsMouseReleased(ImGuiMouseButton_Left)){selection=item->item.key;quantity=1;send(MenuAction::SelectItem,0,selection);}
                    if(hovered){tooltip(item->item,false);}
                }
            }
            if(items.empty())text(430,y+123,"No matching items",white,19);
        };
        if(!own){grid(false,177,externalPage,trade?"Merchant offers":"Container");grid(true,478,playerPage,trade?"Your inventory / sell":"Your inventory");}
        else {grid(true,184,playerPage,"Your inventory");text(270,490,"Additional item actions",ink,23);text(270,536,"Equip, read, drink, apply poison or drop an item. Select a slot, then use the action panel.",dim,20,594);text(270,650,"Open the main inventory with E for equipment slots, portable crafting and the recipe browser.",dim,18,594);}
        text(270,776,view.message.empty()?"Select a slot to inspect it. Items stay in their real inventories.":view.message,dim,15,610);
        text(946,22,trade?"Trade details":"Item details",ink,24);
        if(chosen){const auto& item=chosen->item;icon(item.icon,948, 70,54,item.tint);
            ImGui::GetWindowDrawList()->PushClipRect(point(1014,73),point(1244,138),true);text(1014,73,item.name,item.enchanted?IM_COL32(118,65,161,255):ink,21,226);ImGui::GetWindowDrawList()->PopClipRect();
            text(946,141,item.source,IM_COL32(56,81,159,255),16,296);
            text(946,185,std::format("Count {}   Weight {:.1f}",item.count,item.weight),dim,18);
            text(946,221,trade?std::format("{} price: {} gold",chosen->player?"Sell":"Buy",chosen->price):std::format("Value: {} gold",item.value),ink,21,296);
            if(item.damage)text(946,258,std::format("Damage {:.1f}",item.damage),dim,18);if(item.armor)text(946,258,std::format("Base armor {:.1f}",item.armor),dim,18);
            ImGui::GetWindowDrawList()->PushClipRect(point(946,302),point(1244,442),true);text(946,302,item.description,dim,17,294);ImGui::GetWindowDrawList()->PopClipRect();
            if(view.containerMode==2&&view.pickpocketChance>=0)text(946,449,std::to_string(view.pickpocketChance)+"% chance",IM_COL32(162,83,36,255),20);
            if(item.quest)text(946,473,"QUEST ITEM - protected",IM_COL32(126,87,25,255),17);
            quantity=std::clamp(quantity,1,std::max(1,item.count));text(946,514,"Quantity",dim,17);
            if(button("less","-",946,548,48,37,quantity>1))--quantity;text(1017,558,std::to_string(quantity),ink,19);
            if(button("more","+",1080,548,48,37,quantity<item.count))++quantity;
            if(button("stack","Stack",1140,548,104,37))quantity=item.count;
            const std::int64_t total=std::int64_t(chosen->price)*quantity;
            if(trade)text(946,610,std::format("Total: {} gold",total),ink,23);
            std::string verb=own?(item.usable?item.verb:"Use"):trade?(chosen->player?"Sell":"Buy"):chosen->player?"Store":view.containerMode==1||view.containerMode==2?"Steal":"Take";
            bool permitted=chosen->enabled&&!(chosen->player&&item.quest&&!own)&&(!own||item.usable||item.kind==ItemKind::Potion);
            if(trade)permitted=permitted&&chosen->price>=0&&total<=(chosen->player?view.merchantGold:view.gold);
            if(button("primary",verb+" "+(own?std::string{}:std::to_string(quantity)),946,657,298,47,permitted)){
                if(trade||(!chosen->player&&(view.containerMode==1||view.containerMode==2)))ImGui::OpenPopup("Confirm transfer");
                else send(own?MenuAction::Equip:MenuAction::Transfer,quantity,selection,chosen->price);
            }
            if(own&&button("native-drop","Drop selected quantity",946,719,298,40,item.droppable))ImGui::OpenPopup("Confirm drop");
            if(confirmation("Confirm transfer")){
                ImGui::TextWrapped("%s %d x %s?",verb.c_str(),quantity,item.name.c_str());if(trade)ImGui::Text("Total: %lld gold",static_cast<long long>(total));else ImGui::TextUnformatted("This is stealing. Skyrim applies its normal consequences.");
                if(ImGui::Button("Confirm")){send(MenuAction::Transfer,quantity,selection,chosen->price);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();endConfirmation();
            }
            if(confirmation("Confirm drop")){
                ImGui::Text("Drop %d x %s?",quantity,item.name.c_str());if(ImGui::Button("Drop")){send(MenuAction::Drop,quantity,selection);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();endConfirmation();
            }
        } else {bevel(946,84,298,150,slotFill,true);icon(ItemIcon::Bag,1055,112,86);text(946,273,"Select an item",ink,24);text(946,321,"Item information, effects, quantities and available actions appear here.",dim,19,294);}
    }
    ImGui::End();ImGui::PopStyleVar(2);
}

}
