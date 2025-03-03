/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <common/crc32.hpp>
#include <rom/gpio/rtc.hpp>
#include <rom/gpio/solar_sensor.hpp>

#include "emulator.hpp"

namespace nba {

namespace core {

Core::Core(std::shared_ptr<Config> config)
    : config(config)
    , cpu(scheduler, bus)
    , irq(cpu, scheduler)
    , dma(bus, irq, scheduler)
    , apu(scheduler, dma, bus, config)
    , ppu(scheduler, irq, dma, config)
    , timer(scheduler, irq, apu)
    , keypad(scheduler, irq)
    , bus(scheduler, {cpu, irq, dma, apu, ppu, timer, keypad}) {
  Reset();
}

void Core::Reset() {
  scheduler.Reset();
  cpu.Reset();
  irq.Reset();
  dma.Reset();
  timer.Reset();
  apu.Reset();
  ppu.Reset();
  bus.Reset();
  keypad.Reset();

  if(config->skip_bios) {
    SkipBootScreen();
  }

  if(config->audio.mp2k_hle_enable) {
    apu.GetMP2K().UseCubicFilter() = config->audio.mp2k_hle_cubic;
    apu.GetMP2K().ForceReverb() = config->audio.mp2k_hle_force_reverb;
    hle_audio_hook = SearchSoundMainRAM();
    if(hle_audio_hook != 0xFFFFFFFF) {
      Log<Info>("Core: detected MP2K audio mixer @ 0x{:08X}", hle_audio_hook);
    }
  } else {
    hle_audio_hook = 0xFFFFFFFF;
  }
}

void Core::Attach(std::vector<u8> const& bios) {
  bus.Attach(bios);
}

void Core::Attach(ROM&& rom) {
  bus.Attach(std::move(rom));
}

auto Core::CreateRTC() -> std::unique_ptr<RTC> {
  return std::make_unique<RTC>(irq);
}

auto Core::CreateSolarSensor() -> std::unique_ptr<SolarSensor> {
  return std::make_unique<SolarSensor>();
}

void Core::SetKeyStatus(Key key, bool pressed) {
  keypad.SetKeyStatus(key, pressed);
}

void Core::Run(int cycles) {
  using HaltControl = Bus::Hardware::HaltControl;

  const auto limit = scheduler.GetTimestampNow() + cycles;

  while(scheduler.GetTimestampNow() < limit) {
    if(bus.hw.haltcnt == HaltControl::Run) {
      if(cpu.state.r15 == hle_audio_hook) {
        const u32  sound_info_addr = *bus.GetHostAddress<u32>(0x03007FF0);
        const auto sound_info = bus.GetHostAddress<MP2K::SoundInfo>(sound_info_addr);

        if(sound_info != nullptr) {
          apu.GetMP2K().SoundMainRAM(*sound_info);
        }
      }

      cpu.Run();
    } else {
      while(scheduler.GetTimestampNow() < limit && !irq.ShouldUnhaltCPU()) {
        if(dma.IsRunning()) {
          dma.Run();
          if(irq.ShouldUnhaltCPU()) continue; // can become true during the DMA
        }

        bus.Step(scheduler.GetRemainingCycleCount());
      }

      if(irq.ShouldUnhaltCPU()) {
        bus.Step(1);
        bus.hw.haltcnt = HaltControl::Run;
      }
    }
  }
}

void Core::SkipBootScreen() {
  cpu.SwitchMode(arm::MODE_SYS);
  cpu.state.bank[arm::BANK_SVC][arm::BANK_R13] = 0x03007FE0;
  cpu.state.bank[arm::BANK_IRQ][arm::BANK_R13] = 0x03007FA0;
  cpu.state.r13 = 0x03007F00;
  cpu.state.r15 = 0x08000000;
}

auto Core::SearchSoundMainRAM() -> u32 {
  static constexpr u32 kSoundMainCRC32 = 0x27EA7FCF;
  static constexpr int kSoundMainLength = 48;

  auto& rom = bus.memory.rom.GetRawROM();

  if(rom.size() < kSoundMainLength) {
    return 0xFFFFFFFF;
  }

  u32 address_max = rom.size() - kSoundMainLength;

  for(u32 address = 0; address <= address_max; address += sizeof(u16)) {
    auto crc = crc32(&rom[address], kSoundMainLength);

    if(crc == kSoundMainCRC32) {
      /* We have found SoundMain().
       * The pointer to SoundMainRAM() is stored at offset 0x74.
       */
      address = read<u32>(rom.data(), address + 0x74);
      if(address & 1) {
        address &= ~1;
        address += sizeof(u16) * 2;
      } else {
        address &= ~3;
        address += sizeof(u32) * 2;
      }
      return address;
    }
  }

  return 0xFFFFFFFF;
}

auto Core::GetROM() -> ROM& {
  return bus.memory.rom;
}

auto Core::GetPRAM() -> u8* {
  return ppu.GetPRAM();
}

auto Core::GetVRAM() -> u8* {
  return ppu.GetVRAM();
}

auto Core::GetOAM() -> u8* {
  return ppu.GetOAM();
}

auto Core::PeekByteIO(u32 address) -> u8  {
  return bus.hw.ReadByte(address);
}

auto Core::PeekHalfIO(u32 address) -> u16 {
  return bus.hw.ReadHalf(address);
}

auto Core::PeekWordIO(u32 address) -> u32 {
  return bus.hw.ReadWord(address);
}

auto Core::GetBGHOFS(int id) -> u16 {
  return ppu.mmio.bghofs[id];
}

auto Core::GetBGVOFS(int id) -> u16 {
  return ppu.mmio.bgvofs[id];
}

Scheduler& Core::GetScheduler() {
  return scheduler;
}

} // namespace nba::core

auto CreateCore(
  std::shared_ptr<Config> config
) -> std::unique_ptr<CoreBase> {
  return std::make_unique<core::Core>(config);
}

} // namespace nba
