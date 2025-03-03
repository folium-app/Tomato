/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "hw/ppu/ppu.hpp"

namespace nba::core {

void PPU::InitBackground() {
  const u64 timestamp_now = scheduler.GetTimestampNow();

  bg.timestamp_init = timestamp_now;
  bg.timestamp_last_sync = timestamp_now;
  bg.cycle = 0U;

  for(auto& text : bg.text) {
    text.fetches = 0;
  }

  const bool first_scanline = mmio.vcount == 0;

  for(int id = 0; id < 2; id++) {
    auto& bgx = mmio.bgx[id];
    auto& bgy = mmio.bgy[id];

    // @todo: should BGY be latched when BGX was written and vice versa?
    if(bgx.written || first_scanline) {
      bgx._current = bgx.initial;
      bgx.written = false;
    }
    if(bgy.written || first_scanline) {
      bgy._current = bgy.initial;
      bgy.written = false;
    }

    bg.affine[id].x = bgx._current;
    bg.affine[id].y = bgy._current;
  }
}

void PPU::DrawBackground() {
  const u64 timestamp_now = scheduler.GetTimestampNow();
  
  const int cycles = (int)(timestamp_now - bg.timestamp_last_sync);

  if(cycles == 0 || bg.cycle >= 1232U) {
    return;
  }

  const int mode = mmio.dispcnt.mode;

  switch(mode) {
    case 0: DrawBackgroundImpl<0>(cycles); break;
    case 1: DrawBackgroundImpl<1>(cycles); break;
    case 2: DrawBackgroundImpl<2>(cycles); break;
    case 3: DrawBackgroundImpl<3>(cycles); break;
    case 4: DrawBackgroundImpl<4>(cycles); break;
    case 5: DrawBackgroundImpl<5>(cycles); break;
    case 6: 
    case 7: DrawBackgroundImpl<7>(cycles); break;
  }

  bg.timestamp_last_sync = timestamp_now;
}

template<int mode> void PPU::DrawBackgroundImpl(int cycles) {
  const u16 latched_dispcnt_and_current_dispcnt = mmio.dispcnt_latch[0] & mmio.dispcnt.hword;
  
  /**
   * @todo: we are losing out on some possible optimizations,
   * by implementing the various BG modes in separate methods,
   * which we have to call on a per-cycle basis.
   */
  for(int i = 0; i < cycles; i++) {
    // We add one to the cycle counter for convenience,
    // because it makes some of the timing math simpler.
    const uint cycle = 1U + bg.cycle;

    // text-mode backgrounds
    if constexpr(mode <= 1) {
      const uint id = cycle & 3U; // BG0 - BG3

      if((id <= 1 || mode == 0) && (latched_dispcnt_and_current_dispcnt & (256U << id))) {
        RenderMode0BG(id, cycle);
      }
    }

    if(cycle < 1007U) {
      // affine backgrounds
      if constexpr(mode == 1 || mode == 2) {
        const int id = ~(cycle >> 1) & 1; // 0: BG2, 1: BG3

        if((id == 0 || mode == 2) && (latched_dispcnt_and_current_dispcnt & (1024U << id))) {
          RenderMode2BG(id, cycle);
        }
      }

      if constexpr(mode == 3) {
        if(latched_dispcnt_and_current_dispcnt & 1024U) {
          RenderMode3BG(cycle);
        }
      }

      if constexpr(mode == 4) {
        if(latched_dispcnt_and_current_dispcnt & 1024U) {
          RenderMode4BG(cycle);
        }
      }

      if constexpr(mode == 5) {
        if(latched_dispcnt_and_current_dispcnt & 1024U) {
          RenderMode5BG(cycle);
        }
      }
    }

    // @todo: research mosaic timing and narrow down the BG X/Y timing more precisely.
    if(cycle == 1232U) {
      auto& mosaic = mmio.mosaic;

      if(mmio.vcount < 159) {
        if(++mosaic.bg._counter_y == mosaic.bg.size_y) {
          mosaic.bg._counter_y = 0;
        } else {
          mosaic.bg._counter_y &= 15;
        }
      } else {
        mosaic.bg._counter_y = 0;
      }

      auto& bgx = mmio.bgx;
      auto& bgy = mmio.bgy;
      auto& bgpb = mmio.bgpb;
      auto& bgpd = mmio.bgpd;

      const auto AdvanceBGXY = [&](int id) {
        auto bg_id = 2 + id;

        /* Do not update internal X/Y unless the latched BG enable bit is set.
         * This behavior was confirmed on real hardware.
         */
        if(latched_dispcnt_and_current_dispcnt & (256U << bg_id)) {
          if(mmio.bgcnt[bg_id].mosaic_enable) {
            if(mosaic.bg._counter_y == 0) {
              bgx[id]._current += mosaic.bg.size_y * bgpb[id];
              bgy[id]._current += mosaic.bg.size_y * bgpd[id];
            }
          } else {
            bgx[id]._current += bgpb[id];
            bgy[id]._current += bgpd[id];
          }
        }
      };

      if constexpr(mode >= 1 && mode <= 5) {
        AdvanceBGXY(0);
      }

      if constexpr(mode == 2) {
        AdvanceBGXY(1);
      }
    }

    if(++bg.cycle == 1232U) {
      break;
    }
  }
}

} // namespace nba::core
