/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>
#include <cmath>
#include <common/dsp/resampler/cosine.hpp>
#include <common/dsp/resampler/cubic.hpp>
#include <common/dsp/resampler/nearest.hpp>
#include <common/dsp/resampler/sinc.hpp>

#include "hw/apu/apu.hpp"

namespace nba::core {

APU::APU(
  Scheduler& scheduler,
  DMA& dma,
  Bus& bus,
  std::shared_ptr<Config> config
)   : mmio(scheduler)
    , scheduler(scheduler)
    , dma(dma)
    , mp2k(bus)
    , config(config) {
  scheduler.Register(Scheduler::EventClass::APU_mixer, this, &APU::StepMixer);
  scheduler.Register(Scheduler::EventClass::APU_sequencer, this, &APU::StepSequencer);
}

APU::~APU() {
  config->audio_dev->Close();
}

void APU::Reset() {
  mmio.fifo[0].Reset();
  mmio.fifo[1].Reset();
  mmio.psg1.Reset();
  mmio.psg2.Reset();
  mmio.psg3.Reset(WaveChannel::ResetWaveRAM::Yes);
  mmio.psg4.Reset();
  mmio.soundcnt.Reset();
  mmio.bias.Reset();
  fifo_pipe[0] = {};
  fifo_pipe[1] = {};

  resolution_old = 0;
  scheduler.Add(mmio.bias.GetSampleInterval(), Scheduler::EventClass::APU_mixer);
  scheduler.Add(BaseChannel::s_cycles_per_step, Scheduler::EventClass::APU_sequencer);

  mp2k.Reset();
  mp2k_read_index = {};

  auto audio_dev = config->audio_dev;
  audio_dev->Close();
  audio_dev->Open(this, (AudioDevice::Callback)AudioCallback);

  using Interpolation = Config::Audio::Interpolation;

  buffer = std::make_shared<StereoRingBuffer<float>>(audio_dev->GetBlockSize() * 4, true);

  switch(config->audio.interpolation) {
    case Interpolation::Cosine:
      resampler = std::make_unique<CosineStereoResampler<float>>(buffer);
      break;
    case Interpolation::Cubic:
      resampler = std::make_unique<CubicStereoResampler<float>>(buffer);
      break;
    case Interpolation::Sinc_32:
      resampler = std::make_unique<SincStereoResampler<float, 32>>(buffer);
      break;
    case Interpolation::Sinc_64:
      resampler = std::make_unique<SincStereoResampler<float, 64>>(buffer);
      break;
    case Interpolation::Sinc_128:
      resampler = std::make_unique<SincStereoResampler<float, 128>>(buffer);
      break;
    case Interpolation::Sinc_256:
      resampler = std::make_unique<SincStereoResampler<float, 256>>(buffer);
      break;
  }

  resampler->SetSampleRates(mmio.bias.GetSampleRate(), audio_dev->GetSampleRate());
}

void APU::OnTimerOverflow(int timer_id, int times) {
  auto const& soundcnt = mmio.soundcnt;

  if(!soundcnt.master_enable) {
    return;
  }

  constexpr DMA::Occasion occasion[2] = { DMA::Occasion::FIFO0, DMA::Occasion::FIFO1 };

  for(int fifo_id = 0; fifo_id < 2; fifo_id++) {
    if(soundcnt.dma[fifo_id].timer_id == timer_id) {
      auto& fifo = mmio.fifo[fifo_id];
      auto& pipe = fifo_pipe[fifo_id];

      if(fifo.Count() <= 3) {
        dma.Request(occasion[fifo_id]);
      }

      if(pipe.size == 0 && fifo.Count() > 0) {
        pipe.word = fifo.ReadWord();
        pipe.size = 4;
      }

      s8 sample = (s8)(u8)pipe.word;

      if(pipe.size > 0) {
        pipe.word >>= 8;
        pipe.size--;
      }

      latch[fifo_id] = sample;
    }
  }
}

void APU::StepMixer() {
  constexpr int psg_volume_tab[4] = { 1, 2, 4, 0 };
  constexpr int dma_volume_tab[2] = { 2, 4 };

  auto& psg = mmio.soundcnt.psg;
  auto& dma = mmio.soundcnt.dma;

  auto psg_volume = psg_volume_tab[psg.volume];

  if(mp2k.IsEngaged()) {
    StereoSample<float> sample { 0, 0 };

    if(resolution_old != 1) {
      resampler->SetSampleRates(65536, config->audio_dev->GetSampleRate());
      resolution_old = 1;
    }

    auto mp2k_sample = mp2k.ReadSample();

    for(int channel = 0; channel < 2; channel++) {
      s16 psg_sample = 0;

      if(psg.enable[channel][0]) psg_sample += mmio.psg1.GetSample();
      if(psg.enable[channel][1]) psg_sample += mmio.psg2.GetSample();
      if(psg.enable[channel][2]) psg_sample += mmio.psg3.GetSample();
      if(psg.enable[channel][3]) psg_sample += mmio.psg4.GetSample();

      sample[channel] += psg_sample * psg_volume * (psg.master[channel] + 1) / (32.0 * 0x200);

      /* TODO: we assume that MP2K sends right channel to FIFO A and left channel to FIFO B,
       * but we haven't verified that this is actually correct.
       */
      for(int fifo = 0; fifo < 2; fifo++) {
        if(dma[fifo].enable[channel]) {
          sample[channel] += mp2k_sample[fifo] * dma_volume_tab[dma[fifo].volume] * 0.25;
        }
      }
    }

    if(!mmio.soundcnt.master_enable) sample = {};

    buffer_mutex.lock();
    resampler->Write(sample);
    buffer_mutex.unlock();

    scheduler.Add(256 - (scheduler.GetTimestampNow() & 255), Scheduler::EventClass::APU_mixer);
  } else {
    StereoSample<s16> sample { 0, 0 };

    auto& bias = mmio.bias;

    if(bias.resolution != resolution_old) {
      resampler->SetSampleRates(bias.GetSampleRate(), config->audio_dev->GetSampleRate());
      resolution_old = mmio.bias.resolution;
    }

    for(int channel = 0; channel < 2; channel++) {
      s16 psg_sample = 0;

      if(psg.enable[channel][0]) psg_sample += mmio.psg1.GetSample();
      if(psg.enable[channel][1]) psg_sample += mmio.psg2.GetSample();
      if(psg.enable[channel][2]) psg_sample += mmio.psg3.GetSample();
      if(psg.enable[channel][3]) psg_sample += mmio.psg4.GetSample();

      sample[channel] += psg_sample * psg_volume * (psg.master[channel] + 1) >> 5;

      for(int fifo = 0; fifo < 2; fifo++) {
        if(dma[fifo].enable[channel]) {
          sample[channel] += latch[fifo] * dma_volume_tab[dma[fifo].volume];
        }
      }

      sample[channel] += mmio.bias.level;
      sample[channel]  = std::clamp(sample[channel], s16(0), s16(0x3FF));
      sample[channel] -= 0x200;
    }

    if(!mmio.soundcnt.master_enable) sample = {};

    buffer_mutex.lock();
    resampler->Write({ sample[0] / float(0x200), sample[1] / float(0x200) });
    buffer_mutex.unlock();

    const int sample_interval = mmio.bias.GetSampleInterval();
    const int cycles = sample_interval - (scheduler.GetTimestampNow() & (sample_interval - 1));

    scheduler.Add(cycles, Scheduler::EventClass::APU_mixer);
  }
}

void APU::StepSequencer() {
  mmio.psg1.Tick();
  mmio.psg2.Tick();
  mmio.psg3.Tick();
  mmio.psg4.Tick();

  scheduler.Add(BaseChannel::s_cycles_per_step, Scheduler::EventClass::APU_sequencer);
}

} // namespace nba::core
