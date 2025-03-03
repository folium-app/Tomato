/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <integer.hpp>
#include <scheduler.hpp>

#include "hw/apu/channel/base_channel.hpp"

namespace nba::core {

class WaveChannel : public BaseChannel {
public:
  enum class ResetWaveRAM {
    No,
    Yes
  };

  WaveChannel(Scheduler& scheduler);

  void Reset(ResetWaveRAM reset_wave_ram);
  bool IsEnabled() override { return playing && BaseChannel::IsEnabled(); }
  auto GetSample() -> s8 override { return sample; }
  void Generate();
  auto Read (int offset) -> u8;
  void Write(int offset, u8 value);

  void LoadState(SaveState::APU::IO::WaveChannel const& state);
  void CopyState(SaveState::APU::IO::WaveChannel& state);

  auto ReadSample(int offset) -> u8 {
    return wave_ram[wave_bank ^ 1][offset];
  }

  void WriteSample(int offset, u8 value) {
    wave_ram[wave_bank ^ 1][offset] = value;
  }

private:
  constexpr int GetSynthesisIntervalFromFrequency(int frequency) {
    // 8 cycles equals 2097152 Hz, the highest possible sample rate.
    return 8 * (2048 - frequency);
  }

  Scheduler& scheduler;
  Scheduler::Event* event = nullptr;

  s8 sample = 0;
  bool playing;
  bool force_volume;
  int volume;
  int frequency;
  int dimension;
  int wave_bank;
  u8 wave_ram[2][16];
  int phase;
};

} // namespace nba::core
