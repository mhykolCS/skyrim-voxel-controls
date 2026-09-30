#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <chrono>
#ifdef VOXEL_PLAYTEST
#include <ScreenGrab.h>
#endif

namespace voxel {
namespace {
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
Present originalPresent{};
ID3D11Device* device{};
ID3D11DeviceContext* context{};
IDXGISwapChain* gameSwapChain{};
bool initialized=false;
float cursorX=640,cursorY=360;
int recipeSelection=0,tabSelection=0;
auto previousPresent=std::chrono::steady_clock::now();
const char* modeName(Mode mode){return mode==Mode::Creative?"CREATIVE FLIGHT":mode==Mode::Glide?"ELYTRA GLIDE":"SURVIVAL";}
void style() {
    auto& s=ImGui::GetStyle();s.WindowRounding=0;s.ChildRounding=0;s.FrameRounding=0;s.ScrollbarRounding=0;
    s.WindowBorderSize=2;s.FrameBorderSize=1;s.FramePadding={12,10};s.ItemSpacing={12,10};s.WindowPadding={24,20};
    s.Colors[ImGuiCol_WindowBg]={0.055f,0.071f,0.09f,0.98f};
    s.Colors[ImGuiCol_ChildBg]={0.075f,0.094f,0.11f,1};
    s.Colors[ImGuiCol_Border]={0.23f,0.30f,0.34f,1};
    s.Colors[ImGuiCol_Text]={0.9f,0.93f,0.9f,1};
    s.Colors[ImGuiCol_TextDisabled]={0.46f,0.55f,0.57f,1};
    s.Colors[ImGuiCol_Button]={0.15f,0.23f,0.23f,1};
    s.Colors[ImGuiCol_ButtonHovered]={0.24f,0.39f,0.31f,1};
    s.Colors[ImGuiCol_ButtonActive]={0.32f,0.49f,0.31f,1};
    s.Colors[ImGuiCol_Header]={0.18f,0.33f,0.28f,1};
    s.Colors[ImGuiCol_HeaderHovered]={0.25f,0.40f,0.34f,1};
    s.Colors[ImGuiCol_PlotHistogram]={0.51f,0.77f,0.34f,1};
}
void drawWorkbench(const Snapshot& state) {
    auto display=ImGui::GetIO().DisplaySize;
    float width=std::min(1120.f,display.x-40),height=std::min(760.f,display.y-40);
    ImGui::SetNextWindowPos({display.x/2,display.y/2},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
    ImGui::Begin("AETHER WORKBENCH",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.66f,.88f,.47f,1},"A E T H E R   /   W O R K B E N C H");
    ImGui::SameLine(width-150);if(ImGui::SmallButton("CLOSE [F8]"))enqueue(Action::CloseWorkbench);
    ImGui::TextDisabled("Alchemy and spell matrices   /   inventory-powered fabrication");
    ImGui::Separator();
    if(ImGui::Button("ALCHEMY",{160,42})){tabSelection=0;recipeSelection=0;}
    ImGui::SameLine();if(ImGui::Button("SPELL MATRICES",{200,42})){tabSelection=1;recipeSelection=3;}
    ImGui::SameLine();ImGui::Text("MAGICKA  %.0f",state.magicka);
    ImGui::Spacing();
    ImGui::BeginChild("recipe-book",{290,height-270},ImGuiChildFlags_Borders);
    ImGui::TextDisabled("RECIPE BOOK");ImGui::Separator();
    for(int index=tabSelection?3:0;index<(tabSelection?6:3);++index){
        const auto& recipe=recipes()[index];
        ImGui::PushID(index);
        if(ImGui::Selectable(recipe.name.c_str(),recipeSelection==index,0,{0,45}))recipeSelection=index;
        if(index<int(state.recipes.size()))ImGui::TextDisabled(state.recipes[index].learned?"MATRIX LEARNED":state.recipes[index].available?"READY TO FABRICATE":"MATERIALS REQUIRED");
        ImGui::Separator();ImGui::PopID();
    }
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("fabricator",{0,height-270},ImGuiChildFlags_Borders);
    const auto& recipe=recipes()[recipeSelection];
    ImGui::TextUnformatted(recipe.name.c_str());
    ImGui::TextWrapped("%s",recipe.description.c_str());ImGui::Spacing();
    ImGui::TextDisabled("INPUT MATRIX");
    const char* symbols[]{"FLOWER","WHEAT","FLOWER","FUNGUS","FLOWER","THISTLE","FIRE","SOUL","FROST","SOUL","VOID","SOUL"};
    for(int row=0;row<3;++row){
        for(int col=0;col<3;++col){
            if(col)ImGui::SameLine();
            int slot=row*3+col;ImGui::PushID(slot);
            std::string label=slot<2?symbols[recipeSelection*2+slot]:"";
            if(slot<2&&recipeSelection<int(state.recipes.size()))label+="\n"+std::to_string(state.recipes[recipeSelection].counts[slot])+" / 1";
            ImGui::Button(label.c_str(),{88,64});ImGui::PopID();
        }
    }
    ImGui::Spacing();
    bool available=recipeSelection<int(state.recipes.size())&&state.recipes[recipeSelection].available;
    ImGui::BeginDisabled(!available);
    if(ImGui::Button(recipe.spell?"FABRICATE + LEARN MATRIX":"FABRICATE POTION",{290,46}))enqueue(Action::Craft,recipeSelection);
    ImGui::EndDisabled();
    ImGui::TextDisabled(recipe.spell?"Consumes ingredients once. Unlocks a reusable spell.":"Consumes ingredients. Output goes to your inventory.");
    ImGui::EndChild();
    ImGui::Separator();ImGui::TextWrapped("%s",state.status.c_str());
    ImGui::TextDisabled("[1] Firebolt    [2] Ice spike    [3] Lightning    Right mouse: cast selected matrix");
    ImGui::TextDisabled("F3 diagnostics  |  F5 camera  |  F6 creative  |  F7 glide  |  F10 vanilla controls");
    ImGui::End();
}
void draw(const Snapshot& state) {
    const auto display=ImGui::GetIO().DisplaySize;
    if(state.debug){
        ImGui::SetNextWindowPos({22,24},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(.85f);
        ImGui::Begin("Voxel diagnostics",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("VoxelControls 0.1.1 / Skyrim 1.7.104 / %.1f FPS",ImGui::GetIO().Framerate);
        ImGui::Text("%s / %s",state.enabled?"ENABLED":"VANILLA",modeName(state.mode));
        ImGui::Text("XYZ: %.2f / %.2f / %.2f (Skyrim units)",state.position.x,state.position.y,state.position.z);
        ImGui::Text("Velocity: %.2f / %.2f / %.2f m/s",state.velocity.x,state.velocity.y,state.velocity.z);
        ImGui::Text("Speed %.2f m/s   Camera %d   Controller %s",state.velocity.length(),state.camera,state.active?"active":"suspended");
        if(state.controlState==ControlMode::Scripted)ImGui::Text("Skyrim owns control: scripted scene or restricted input");
        ImGui::Text("Cell: %s",state.location.c_str());
        ImGui::Text("Health %.0f   Magicka %.0f   Stamina %.0f",state.health,state.magicka,state.stamina);
        if(!state.target.empty())ImGui::Text("Target: %s",state.target.c_str());
        ImGui::TextUnformatted(state.status.c_str());
        ImGui::End();
    }
    if(state.active&&!state.workbench){
        ImGui::SetNextWindowPos({display.x/2,display.y-120},ImGuiCond_Always,{.5f,0});
        ImGui::SetNextWindowBgAlpha(.82f);
        ImGui::Begin("Voxel hotbar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.7f,.89f,.52f,1},"%s",modeName(state.mode));
        ImGui::SameLine();ImGui::TextDisabled("F8 workbench");
        const char* names[]{"1 FIRE","2 FROST","3 STORM"};
        for(int i=0;i<3;++i){if(i)ImGui::SameLine();ImGui::TextColored(i==state.selectedSpell?ImVec4(.7f,.9f,.5f,1):ImVec4(.6f,.65f,.65f,1),"[%s %s]",names[i],state.spells[i]?"+":"-");}
        ImGui::ProgressBar(state.attackCharge,{320,5},"");
        ImGui::End();
    }
    if(!state.active&&!state.debug&&state.controlState!=ControlMode::Scripted){
        ImGui::SetNextWindowPos({22,display.y-48},ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.7f);
        ImGui::Begin("Voxel ready",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.7f,.89f,.52f,1},"VoxelControls 0.1.1  /  %s  /  F3 diagnostics",state.enabled?"READY":"VANILLA");ImGui::End();
    }
    if(state.workbench)drawWorkbench(state);
}
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* swap,UINT interval,UINT flags) {
    if(swap!=gameSwapChain)return originalPresent(swap,interval,flags);
    queueFrame();
    if(!initialized) {
        if(FAILED(swap->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&device))))return originalPresent(swap,interval,flags);
        device->GetImmediateContext(&context);
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        ImFontConfig font;font.SizePixels=20;io.Fonts->AddFontDefault(&font);
        style();initialized=ImGui_ImplDX11_Init(device,context);
        spdlog::info("D3D11 overlay initialized: {}",initialized);
        if(!initialized)return originalPresent(swap,interval,flags);
    }
    ID3D11Texture2D* backBuffer{};ID3D11RenderTargetView* view{};
    if(SUCCEEDED(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&backBuffer)))) {
        D3D11_TEXTURE2D_DESC description;backBuffer->GetDesc(&description);
        device->CreateRenderTargetView(backBuffer,nullptr,&view);backBuffer->Release();
        if(view){
            auto state=readSnapshot();auto input=takePointerInput();
            auto& io=ImGui::GetIO();io.DisplaySize={float(description.Width),float(description.Height)};
            auto now=std::chrono::steady_clock::now();io.DeltaTime=std::clamp(std::chrono::duration<float>(now-previousPresent).count(),.001f,.2f);previousPresent=now;
            cursorX=std::clamp(cursorX+input.dx,0.f,io.DisplaySize.x);cursorY=std::clamp(cursorY+input.dy,0.f,io.DisplaySize.y);
            static bool wasWorkbench=false;
            if(state.workbench&&!wasWorkbench){cursorX=io.DisplaySize.x/2;cursorY=io.DisplaySize.y/2;}
            wasWorkbench=state.workbench;io.MouseDrawCursor=state.workbench;
            io.AddMousePosEvent(cursorX,cursorY);
            for(int i=0;i<3;++i)io.AddMouseButtonEvent(i,state.workbench&&input.buttons[i]);
            io.AddMouseWheelEvent(0,state.workbench?input.wheel:0);
            ImGui_ImplDX11_NewFrame();ImGui::NewFrame();draw(state);ImGui::Render();
            ID3D11RenderTargetView* oldViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};ID3D11DepthStencilView* oldDepth{};
            context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,oldViews,&oldDepth);context->OMSetRenderTargets(1,&view,nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,oldViews,oldDepth);
            for(auto oldView:oldViews)if(oldView)oldView->Release();if(oldDepth)oldDepth->Release();view->Release();
        }
    }
#ifdef VOXEL_PLAYTEST
    if(captureRequested.exchange(false)) {
        ID3D11Texture2D* capture{};
        if(SUCCEEDED(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&capture)))) {
            auto result=DirectX::SaveDDSTextureToFile(context,capture,L"Data/SKSE/Plugins/VoxelControls.capture.dds");
            spdlog::info("PLAYTEST capture result {}",result);capture->Release();
        }
    }
#endif
    return originalPresent(swap,interval,flags);
}
}
bool installOverlay() {
    auto renderer=RE::BSGraphics::Renderer::GetSingleton();if(!renderer)return false;
    gameSwapChain=reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows[0].swapChain);
    if(!gameSwapChain)return false;
    // IDXGISwapChain::Present is the documented COM vtable slot 8, independent
    // of game code addresses. Keep the preceding hook when other overlays exist.
    auto table=*reinterpret_cast<void***>(gameSwapChain);
    DWORD protection{};
    if(!VirtualProtect(&table[8],sizeof(void*),PAGE_EXECUTE_READWRITE,&protection))return false;
    originalPresent=reinterpret_cast<Present>(table[8]);table[8]=reinterpret_cast<void*>(&present);
    DWORD ignored{};VirtualProtect(&table[8],sizeof(void*),protection,&ignored);
    spdlog::info("Swap-chain overlay attached");return true;
}
}
