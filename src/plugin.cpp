#include "voxel/pch.hpp"
#include "voxel/runtime.hpp"
SKSEPluginInfo(
    .Version=REL::Version{0,1,1,0},
    .Name="VoxelControls"sv,
    .Author="VoxelControls contributors"sv,
    .StructCompatibility=SKSE::StructCompatibility::Dependent,
    .RuntimeCompatibility=SKSE::PluginDeclaration::RuntimeCompatibility{SKSE::VersionIndependence::AddressLibrary,true},
    .MinimumSKSEVersion=REL::Version{2,3,1,0}
)
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    auto folder=SKSE::log::log_directory();
    if(!folder) return false;
    auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*folder/"VoxelControls.log").string(),true);
    auto logger=std::make_shared<spdlog::logger>("VoxelControls",sink);
    spdlog::set_default_logger(logger);spdlog::flush_on(spdlog::level::info);
    spdlog::info("VoxelControls 0.1.1 loading; runtime {}",skse->RuntimeVersion().string());
    // This adapter is intentionally restricted to the binary tested locally.
    if(skse->RuntimeVersion()!=REL::Version{1,7,104,0}) {
        spdlog::error("Unsupported runtime. This build requires Steam Skyrim 1.7.104.0");return false;
    }
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message){
        switch(message->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                voxel::startRuntime();
                if(!voxel::installOverlay()) spdlog::error("Unable to install renderer overlay");
                break;
            case SKSE::MessagingInterface::kPreLoadGame:
            case SKSE::MessagingInterface::kNewGame:
                voxel::resetRuntime();break;
            default:break;
        }
    });
    spdlog::info("Plugin loaded");return true;
}
