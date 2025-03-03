// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
import TomatoObjC

/*
 enum class Key : u8 {
   A = 0,
   B = 1,
   Select = 2,
   Start = 3,
   Right = 4,
   Left = 5,
   Up = 6,
   Down = 7,
   R = 8,
   L = 9,
   Count = 10
 };
 */

public enum GBAKey : UInt8 {
    case a = 0
    case b = 1
    case select = 2
    case start = 3
    case right = 4
    case left = 5
    case up = 6
    case down = 7
    case r = 8
    case l = 9
}

public struct Tomato : @unchecked Sendable {
    public static let shared = Tomato()
    
    var emulator: TomatoObjC = .shared()
    
    public func insert(cartridge: URL) { emulator.insertCartridge(cartridge) }
    
    public func start() { emulator.start() }
    public func pause(_ value: Bool) { emulator.pause(value) }
    public func stop() { emulator.stop() }
    
    public func load() { emulator.load() }
    public func save() { emulator.save() }
    
    public func framebuffer(_ framebuffer: @escaping (UnsafeMutablePointer<UInt32>) -> Void) { emulator.buffer = framebuffer }
    public func framerate(_ framerate: @escaping (Float) -> Void) { emulator.framerate = framerate }
    
    public func button(button: GBAKey, player: Int, pressed: Bool) { emulator.button(button.rawValue, player: .init(player), pressed: pressed) }
}
