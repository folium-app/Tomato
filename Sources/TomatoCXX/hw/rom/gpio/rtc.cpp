/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <rom/gpio/rtc.hpp>
#include <log.hpp>
#include <ctime>

#include "hw/irq/irq.hpp"

namespace nba {

constexpr int RTC::s_argument_count[8];

RTC::RTC(core::IRQ& irq) : irq(irq) {
  Reset();
}

void RTC::Reset() {
  current_bit = 0;
  current_byte = 0;
  data = 0;
  for(int i = 0; i < 7; i++) {
    buffer[i] = 0;
  }
  port.sck = 0;
  port.sio = 0; 
  port.cs  = 0;
  state = State::Complete;

  control = {};

  // Sennen Kazoku (J) refuses to boot unless the 24h-mode is enabled:
  control.mode_24h = true;
}

auto RTC::Read() -> int {
  return (port.sio & port.cs) << static_cast<int>(Port::SIO);
}

void RTC::Write(int value) {
  int old_sck = port.sck;
  int old_cs  = port.cs;

  if(GetPortDirection(static_cast<int>(Port::CS)) == PortDirection::Out) {
    port.cs = (value >> static_cast<int>(Port::CS)) & 1;
  } else {
    Log<Error>("RTC: CS port should be set to 'output' but configured as 'input'.");;
  }

  if(GetPortDirection(static_cast<int>(Port::SCK)) == PortDirection::Out) {
    port.sck = (value >> static_cast<int>(Port::SCK)) & 1;
  } else {
    Log<Error>("RTC: SCK port should be set to 'output' but configured as 'input'.");
  }

  if(GetPortDirection(static_cast<int>(Port::SIO)) == PortDirection::Out) {
    port.sio = (value >> static_cast<int>(Port::SIO)) & 1;
  }

  if(port.cs) {
    // on CS transition from 0 to 1:
    if(!old_cs) {
      state = State::Command;
      current_bit  = 0;
      current_byte = 0;
      return;
    }

    // on SCK transition from 0 to 1:
    if(!old_sck && port.sck) {
      switch(state) {
        case State::Command: {
          ReceiveCommandSIO();
          break;
        }
        case State::Receiving: {
          ReceiveBufferSIO();
          break;
        }
        case State::Sending: {
          TransmitBufferSIO();
          break;
        }
      }
    }
  }
}

bool RTC::ReadSIO() {
  data &= ~(1 << current_bit);
  data |= port.sio << current_bit;

  if(++current_bit == 8) {
    current_bit = 0;
    return true;
  }

  return false;
}

void RTC::ReceiveCommandSIO() {
  bool completed = ReadSIO();

  if(!completed) {
    return;
  }

  // Check whether the command should be interpreted MSB-first or LSB-first.
  if((data >> 4) == 6) {
    data = (data << 4) | (data >> 4);
    data = ((data & 0x33) << 2) | ((data & 0xCC) >> 2);
    data = ((data & 0x55) << 1) | ((data & 0xAA) >> 1);
    Log<Trace>("RTC: received command in REV format, data=0x{0:X}", data);
  } else if((data & 15) != 6) {
    Log<Error>("RTC: received command in unknown format, data=0x{0:X}", data);
    return;
  }

  reg = static_cast<Register>((data >> 4) & 7);
  current_bit  = 0;
  current_byte = 0;

  // data[7:] determines whether the RTC register will be read or written.
  if(data & 0x80) {
    ReadRegister();

    if(s_argument_count[(int)reg] > 0) {
      state = State::Sending;
    } else {
      state = State::Complete;
    }
  } else {
    if(s_argument_count[(int)reg] > 0) {
      state = State::Receiving;
    } else {
      WriteRegister();
      state = State::Complete;
    }
  }
}

void RTC::ReceiveBufferSIO() {
  if(current_byte < s_argument_count[(int)reg] && ReadSIO()) {
    buffer[current_byte] = data;

    if(++current_byte == s_argument_count[(int)reg]) {
      WriteRegister();
      state = State::Complete;
    }
  } 
}

void RTC::TransmitBufferSIO() {
  port.sio = buffer[current_byte] & 1;
  buffer[current_byte] >>= 1;

  if(++current_bit == 8) {
    current_bit = 0;
    if(++current_byte == s_argument_count[(int)reg]) {
      state = State::Complete;
    }
  }
}

void RTC::ReadRegister() {
  const auto AdjustHour = [this](int& hour) {
    if(!control.mode_24h && hour >= 12) {
      hour = (hour - 12) | 64;
    }
  };

  switch(reg) {
    case Register::Control: {
      buffer[0] = (control.unknown1 ?   2 : 0) |
                  (control.per_minute_irq ? 8 : 0) |
                  (control.unknown2 ?  32 : 0) |
                  (control.mode_24h ?  64 : 0) |
                  (control.poweroff ? 128 : 0);
      break;
    }
    case Register::DateTime: {
      auto timestamp = std::time(nullptr);
      auto time = std::localtime(&timestamp);
      AdjustHour(time->tm_hour);
      buffer[0] = ConvertDecimalToBCD(time->tm_year - 100);
      buffer[1] = ConvertDecimalToBCD(1 + time->tm_mon);
      buffer[2] = ConvertDecimalToBCD(time->tm_mday);
      buffer[3] = ConvertDecimalToBCD(time->tm_wday);
      buffer[4] = ConvertDecimalToBCD(time->tm_hour);
      buffer[5] = ConvertDecimalToBCD(time->tm_min);
      buffer[6] = ConvertDecimalToBCD(time->tm_sec);
      break;
    }
    case Register::Time: {
      auto timestamp = std::time(nullptr);
      auto time = std::localtime(&timestamp);
      AdjustHour(time->tm_hour);
      buffer[0] = ConvertDecimalToBCD(time->tm_hour);
      buffer[1] = ConvertDecimalToBCD(time->tm_min);
      buffer[2] = ConvertDecimalToBCD(time->tm_sec);
      break;
    }
  }
}

void RTC::WriteRegister() {
  // TODO: handle writes to the date and time register.
  switch(reg) {
    case Register::Control: {
      control.unknown1 = buffer[0] & 2;
      control.per_minute_irq = buffer[0] & 8;
      control.unknown2 = buffer[0] & 32;
      control.mode_24h = buffer[0] & 64;
      if(control.per_minute_irq) {
        Log<Error>("RTC: enabled the unimplemented per-minute IRQ.");
      }
      break;
    }
    case Register::ForceReset: {
      // TODO: reset date and time register.
      control = {};
      break;
    }
    case Register::ForceIRQ: {
      irq.Raise(core::IRQ::Source::ROM);
      break;
    }
    default: {
      Log<Error>("RTC: unhandled register write: {}", (int)reg);
      break;
    }
  }
}

} // namespace nba
