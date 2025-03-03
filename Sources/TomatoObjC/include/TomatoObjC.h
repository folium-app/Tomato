//
//  TomatoObjC.h
//  Tomato
//
//  Created by Jarrod Norwell on 27/2/2025.
//

#import <Foundation/Foundation.h>

#if __cplusplus
#include <memory>

#include "config.hpp"
#include "core.hpp"
#include "device/video_device.hpp"

#include <log.hpp>
#include <device/audio_device.hpp>
#include <SDL.h>

namespace nba {

struct SDLAudioDevice : AudioDevice {
  void SetSampleRate(int sample_rate);
  void SetBlockSize(int buffer_size);
  void SetPassthrough(SDL_AudioCallback passthrough);
  void InvokeCallback(s16* stream, int byte_len);

  auto GetSampleRate() -> int final;
  auto GetBlockSize() -> int final;
  bool Open(void* userdata, Callback callback) final;
  void SetPause(bool value) final;
  void Close() final;

private:
  Callback callback;
  void* callback_userdata;
  SDL_AudioCallback passthrough = nullptr;
  SDL_AudioDeviceID device;
  SDL_AudioSpec have;
  int want_sample_rate = 48000;
  int want_block_size = 2048;
  bool opened = false;
  bool paused = false;
};

} // namespace nba

struct SWVideoDevice : nba::VideoDevice {
    ~SWVideoDevice() override;
    void Draw(u32* _Nonnull buffer) override;
};

#include <filesystem>
#include <core.hpp>
#include <string>

namespace fs = std::filesystem;

namespace nba {

struct BIOSLoader {
  enum class Result {
    CannotFindFile,
    CannotOpenFile,
    BadImage,
    Success
  };

  static auto Load(
    std::unique_ptr<CoreBase>& core,
    fs::path const& path
  ) -> Result;
};

} // namespace nba

#include <config.hpp>
#include <map>

namespace nba {

enum class GPIODeviceType {
  None = 0,
  RTC = 1,
  SolarSensor = 2
};

struct GameInfo {
  Config::BackupType backup_type = Config::BackupType::Detect;
  GPIODeviceType gpio = GPIODeviceType::None;
  bool mirror = false;
};

extern const std::map<std::string, GameInfo> g_game_db;

constexpr GPIODeviceType operator|(GPIODeviceType lhs, GPIODeviceType rhs) {
  return (GPIODeviceType)((int)lhs | (int)rhs);
}

constexpr int operator&(GPIODeviceType lhs, GPIODeviceType rhs) {
  return (int)lhs & (int)rhs;
}

} // namespace nba

#include <core.hpp>
#include <rom/backup/backup.hpp>
#include <string>

namespace fs = std::filesystem;

namespace nba {

struct ROMLoader {
  enum class Result {
    CannotFindFile,
    CannotOpenFile,
    BadImage,
    Success
  };

  static auto Load(
    std::unique_ptr<CoreBase>& core,
    fs::path const& path,
    Config::BackupType backup_type = Config::BackupType::Detect,
    GPIODeviceType force_gpio = GPIODeviceType::None
  ) -> Result;

  static auto Load(
    std::unique_ptr<CoreBase>& core,
    fs::path const& rom_path,
    fs::path const& save_path,
    Config::BackupType backup_type = Config::BackupType::Detect,
    GPIODeviceType force_gpio = GPIODeviceType::None
  ) -> Result;

private:
  static auto ReadFile(fs::path const& path, std::vector<u8>& file_data) -> Result;

  static auto GetGameInfo(
    std::vector<u8>& file_data
  ) -> GameInfo;

  static auto GetBackupType(
    std::vector<u8>& file_data
  ) -> Config::BackupType;

  static auto CreateBackup(
    std::unique_ptr<CoreBase>& core,
    fs::path const& save_path,
    Config::BackupType backup_type
  ) -> std::unique_ptr<Backup>;

  static auto RoundSizeToPowerOfTwo(size_t size) -> size_t;
};

} // namespace nba

#include <filesystem>
#include <core.hpp>
#include <string>

namespace fs = std::filesystem;

namespace nba {

struct SaveStateLoader {
  enum class Result {
    CannotFindFile,
    CannotOpenFile,
    BadImage,
    UnsupportedVersion,
    Success
  };

  static auto Load(
    std::unique_ptr<CoreBase>& core,
    fs::path const& path
  ) -> Result;

private:
  static auto Validate(SaveState const& save_state) -> Result;
};

struct SaveStateWriter {
  enum class Result {
    CannotOpenFile,
    CannotWrite,
    Success
  };

  static auto Write(
    std::unique_ptr<CoreBase>& core,
    fs::path const& path
  ) -> Result;
};

} // namespace nba

#include <chrono>
#include <functional>
#include <thread>

namespace nba {

struct FrameLimiter {
  FrameLimiter(float fps = 60.0) {
    Reset(fps);
  }

  void Reset();
  void Reset(float fps);
  auto GetFastForward() const -> bool;
  void SetFastForward(bool value);

  void Run(
    std::function<void(void)> frame_advance,
    std::function<void(float)> update_fps
  );

private:
  static constexpr int kMillisecondsPerSecond = 1000;
  static constexpr int kMicrosecondsPerSecond = 1000000;

  int frame_count = 0;
  int frame_duration;
  float frames_per_second;
  bool fast_forward = false;

  std::chrono::time_point<std::chrono::steady_clock> timestamp_target;
  std::chrono::time_point<std::chrono::steady_clock> timestamp_fps_update;
};

} // namespace nba

#include <atomic>
#include <functional>
#include <core.hpp>
#include <integer.hpp>
#include <thread>
#include <queue>
#include <mutex>

namespace nba {

struct EmulatorThread {
  EmulatorThread();
 ~EmulatorThread();

  bool IsRunning() const;
  bool IsPaused() const;
  void SetPause(bool value);
  bool GetFastForward() const;
  void SetFastForward(bool enabled);
  void SetFrameRateCallback(std::function<void(float)> callback);
  void SetPerFrameCallback(std::function<void()> callback);

  void Start(std::unique_ptr<CoreBase> core);
  std::unique_ptr<CoreBase> Stop();

  void Reset();
  void SetKeyStatus(Key key, bool pressed);

private:
  enum class MessageType : u8 {
    Reset,
    SetKeyStatus
  };

  struct Message {
    MessageType type;
    union {
      struct {
        Key key;
        u8bool pressed;
      } set_key_status;
    };
  };

  void PushMessage(const Message& message);
  void ProcessMessages();
  void ProcessMessage(const Message& message);

  static constexpr int k_number_of_input_subframes = 4;
  static constexpr int k_cycles_per_second = 16777216;
  static constexpr int k_cycles_per_frame = 280896;
  static constexpr int k_cycles_per_subframe = k_cycles_per_frame / k_number_of_input_subframes;

  static_assert(k_cycles_per_frame % k_number_of_input_subframes == 0);

  std::queue<Message> msg_queue;
  std::mutex msg_queue_mutex;

  std::unique_ptr<CoreBase> core;
  FrameLimiter frame_limiter;
  std::thread thread;
  std::atomic_bool running = false;
  bool paused = false;
  std::function<void(float)> frame_rate_cb = [](float) {};
  std::function<void()> per_frame_cb = []() {};
};

} // namespace nba




std::shared_ptr<nba::Config> config;
std::unique_ptr<nba::CoreBase> core;
std::unique_ptr<nba::EmulatorThread> thread;

#endif

NS_ASSUME_NONNULL_BEGIN

@interface TomatoObjC : NSObject {
    NSString *name;
    NSURL *directory;
}

@property (nonatomic, strong) void (^buffer) (uint32_t*);
@property (nonatomic, strong) void (^framerate) (float);

+(TomatoObjC *) sharedInstance NS_SWIFT_NAME(shared());

-(void) insertCartridge:(NSURL *)url;

-(void) start;
-(void) pause:(BOOL)paused;
-(void) stop;

-(void) load;
-(void) save;

-(void) button:(uint8_t)button player:(int)player pressed:(BOOL)pressed;
@end

NS_ASSUME_NONNULL_END
