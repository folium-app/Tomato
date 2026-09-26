//
//  bridge.cpp
//  Tomato
//
//  Created by Jarrod Norwell on 2/7/2026.
//

#include "bridge.h"
#include "mesence.h"

#include "Shared/EmuSettings.h"
#include "Shared/MessageManager.h"
#include "Utilities/FolderUtilities.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Tomato-Swift.h"
using namespace Tomato;

struct cntnr_t {
    TomatoCommon tomatoCommon{TomatoCommon::init()};
    TomatoSystem tomatoSystem{TomatoSystem::init()};
    
    std::unique_ptr<Emulator> emulator;
    std::unique_ptr<GBAInput> input;
    std::unique_ptr<iOSRenderer> renderer;
    std::unique_ptr<iOSSink> sink;
    
    GbaConfig config;
    
    std::condition_variable_any cv;
    std::mutex mutex;
    std::atomic<bool> paused, running;
    std::jthread thread;
    
    uint32_t height, width;
    
    std::filesystem::path tomato_path, debugger_path, firmware_path;
    std::filesystem::path hd_packs_path, recent_games_path, saves_path;
    std::filesystem::path save_states_path, screenshots_path, system_data_path;
} cntnr_t;

void tomato::print_about(void) {
    printf("Welcome to Tomato\n");
    printf("Game Boy Advance emulation provided by MesenCE\n");
}

void tomato::initialize_paths(void) {
    auto tomatoDirectoryURL{cntnr_t.tomatoCommon.getTomatoDirectoryURL()};
    if (tomatoDirectoryURL.isSome()) {
        auto tomato_path{std::filesystem::path{tomatoDirectoryURL.get()}};
        
        cntnr_t.tomato_path = tomato_path;
        cntnr_t.debugger_path = tomato_path / "debugger";
        cntnr_t.firmware_path = tomato_path / "firmware";
        cntnr_t.hd_packs_path = tomato_path / "hd_packs";
        cntnr_t.recent_games_path = tomato_path / "recent_games";
        cntnr_t.saves_path = tomato_path / "saves";
        cntnr_t.save_states_path = tomato_path / "save_states";
        cntnr_t.screenshots_path = tomato_path / "screenshots";
        cntnr_t.system_data_path = tomato_path / "system_data";
    }
}

void tomato::initialize_system(void) {
    auto mm{std::make_unique<iOSMessageManager>()};
    MessageManager::SetOptions(false, true);
    MessageManager::RegisterMessageManager(mm.get());
    
    cntnr_t.emulator = std::make_unique<Emulator>();
    cntnr_t.emulator->Initialize(false);
    
    cntnr_t.input = std::make_unique<GBAInput>();
    cntnr_t.renderer = std::make_unique<iOSRenderer>(cntnr_t.emulator, 160, 240);
    cntnr_t.sink = std::make_unique<iOSSink>(cntnr_t.emulator, 48000);
    
    cntnr_t.config = cntnr_t.emulator->GetSettings()->GetGbaConfig();
    cntnr_t.config.Controller.Type = ControllerType::GbaController;
    cntnr_t.emulator->GetSettings()->SetGbaConfig(cntnr_t.config);
}


void tomato::destroy_system(void) {
    tomato::initialize_system();
}


void tomato::insert_disc(std::string path) {
    FolderUtilities::SetHomeFolder(cntnr_t.tomato_path.string());
    FolderUtilities::SetFolderOverrides({}, {}, {}, cntnr_t.system_data_path);
    
    cntnr_t.emulator->LoadRom({path}, {});
    cntnr_t.emulator->RegisterInputProvider(cntnr_t.input.get());
}


bool tomato::is_paused(bool change, bool set_paused) {
    if (change)
        cntnr_t.paused.store(set_paused);
    
    if (change)
        set_paused ? cntnr_t.emulator->Pause() : cntnr_t.emulator->Resume();
    
    if (change && !set_paused)
        cntnr_t.cv.notify_one();
    
    return cntnr_t.paused.load();
}

bool tomato::is_running(bool change, bool set_running) {
    if (change)
        cntnr_t.running.store(set_running);
    return cntnr_t.running.load();
}


void tomato::start(void) {
    cntnr_t.thread = std::jthread([&](std::stop_token token) {
        using namespace std::chrono;
        
        const auto frameDuration = duration<double>(1.0 / 60.0);
        
        while (!token.stop_requested()) {
            {
                std::unique_lock lock(cntnr_t.mutex);
                cntnr_t.cv.wait(lock, token, []() {
                    return !cntnr_t.paused.load();
                });
                
                if (token.stop_requested())
                    break;
            }
            
            auto frameStart = steady_clock::now();
            
            std::vector<uint32_t> data{0};
            if (cntnr_t.renderer->GetFrameIfReady(data, cntnr_t.height, cntnr_t.width))
                tomato::video_callback(tomato::context, data.data(), 0);

            // Limit FPS
            auto frameEnd = steady_clock::now();
            auto elapsed = frameEnd - frameStart;
            if (elapsed < frameDuration)
                std::this_thread::sleep_for(frameDuration - elapsed);
        }
    });
}

void tomato::stop(void) {
    cntnr_t.emulator->Stop(false, true);
    
    cntnr_t.thread.request_stop();
    if (cntnr_t.thread.joinable())
        cntnr_t.thread.join();
    
    cntnr_t.paused.store(false);
    cntnr_t.running.store(false);
}


int tomato::framebuffer_height(void) {
    return cntnr_t.height;
}

int tomato::framebuffer_width(void) {
    return cntnr_t.width;
}


void tomato::audio_buffer_callback(tomato::AudioVideoBufferCallback callback) {
    tomato::audio_callback = callback;
}

void tomato::video_buffer_callback(tomato::AudioVideoBufferCallback callback) {
    tomato::video_callback = callback;
}


void tomato::press_button(uint32_t button) {
    cntnr_t.input->keys |= button;
}

void tomato::release_button(uint32_t button) {
    cntnr_t.input->keys &= ~button;
}


void tomato::set_context(void* context) {
    tomato::context = context;
}


void tomato::set_setting(SETTING setting, bool value) {
    cntnr_t.config = cntnr_t.emulator->GetSettings()->GetGbaConfig();
    
    switch (setting) {
        case SETTING::SKIP_BOOT_SCREEN:
            cntnr_t.config.SkipBootScreen = value;
            break;
        case SETTING::ADJUST_COLOURS:
            cntnr_t.config.GbaAdjustColors = value;
            break;
        case SETTING::BLEND_FRAMES:
            cntnr_t.config.BlendFrames = value;
            break;
        case SETTING::FRAME_SKIPPING:
            cntnr_t.config.DisableFrameSkipping = !value;
            break;
    }
    
    cntnr_t.emulator->GetSettings()->SetGbaConfig(cntnr_t.config);
}
