/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <common/dsp/resampler.hpp>
#include <common/dsp/ring_buffer.hpp>
#include <config.hpp>
#include <save_state.hpp>
#include <scheduler.hpp>
#include <mutex>

#include "hw/apu/channel/quad_channel.hpp"
#include "hw/apu/channel/wave_channel.hpp"
#include "hw/apu/channel/noise_channel.hpp"
#include "hw/apu/channel/fifo.hpp"
#include "hw/apu/hle/mp2k.hpp"
#include "hw/apu/registers.hpp"
#include "hw/dma/dma.hpp"

namespace nba::core {

// See callback.cpp for implementation
void AudioCallback(struct APU* apu, s16* stream, int byte_len);

struct APU {
  APU(
    Scheduler& scheduler,
    DMA& dma,
    Bus& bus,
    std::shared_ptr<Config>
  );

 ~APU();

  void Reset();
  auto GetMP2K() -> MP2K& { return mp2k; }
  void OnTimerOverflow(int timer_id, int times);

  void LoadState(SaveState const& state);
  void CopyState(SaveState& state);

  struct MMIO {
    MMIO(Scheduler& scheduler)
        : psg1(scheduler, Scheduler::EventClass::APU_PSG1_generate)
        , psg2(scheduler, Scheduler::EventClass::APU_PSG2_generate)
        , psg3(scheduler)
        , psg4(scheduler, bias) {
    }

    FIFO fifo[2];

    QuadChannel psg1;
    QuadChannel psg2;
    WaveChannel psg3;
    NoiseChannel psg4;

    SoundControl soundcnt { fifo, psg1, psg2, psg3, psg4 };
    BIAS bias;
  } mmio;

  struct Pipe {
    u32 word = 0;
    int size = 0;
  } fifo_pipe[2];

  std::mutex buffer_mutex;
  std::shared_ptr<StereoRingBuffer<float>> buffer;
  std::unique_ptr<StereoResampler<float>> resampler;

private:
  friend void AudioCallback(APU* apu, s16* stream, int byte_len);

  void StepMixer();
  void StepSequencer();

  s8 latch[2];

  Scheduler& scheduler;
  DMA& dma;
  MP2K mp2k;
  int mp2k_read_index;
  std::shared_ptr<Config> config;
  int resolution_old = 0;
};

} // namespace nba::core
