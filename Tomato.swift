//
//  Tomato.swift
//  Tomato
//
//  Created by Jarrod Norwell on 2/9/2026.
//

import Foundation

public enum TomatoButton : UInt32 {
    case a = 0x1
    case b = 0x2
    case select = 0x4
    case start = 0x8
    case right = 0x10
    case left = 0x20
    case up = 0x40
    case down = 0x80
    case r = 0x100
    case l = 0x200
    
    var uint32: UInt32 { rawValue }
}

public class TomatoCommon {
    public init() {}
    
    public static var documentDirectoryURL: URL? {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
    }
    
    public static var tomatoDirectoryURL: String? {
        if let documentDirectoryURL {
            documentDirectoryURL.appending(component: "Tomato").path
        } else {
            nil
        }
    }
}

public actor TomatoSystem {
    private var fileManager: FileManager = .default
    
    public init() {}
    
    public func printAbout() {
        tomato.print_about()
    }
    
    public func initializePaths() {
        tomato.initialize_paths()
    }
    
    public func initializeSystem() {
        tomato.initialize_system()
    }
    
    public func destroySystem() {
        tomato.destroy_system()
    }
    
    public func insertDisc(at url: URL) {
        tomato.insert_disc(std.string(url.path))
    }
    
    public func set(change: Bool = false, isRunning: Bool = false) {
        if change {
            running = isRunning
        }
    }
    
    public var running: Bool {
        get {
            tomato.is_running()
        }
        set {
            tomato.is_running(true, newValue)
        }
    }
    
    public func set(change: Bool = false, isPaused: Bool = false) {
        if change {
            paused = isPaused
        }
    }
    
    public var paused: Bool {
        get {
            tomato.is_paused()
        }
        set {
            tomato.is_paused(true, newValue)
        }
    }
    
    
    public func start() {
        tomato.start()
    }
    
    public func stop() {
        tomato.stop()
    }
    
    
    public var framebufferHeight: Int32 {
        tomato.framebuffer_height()
    }
    
    public var framebufferWidth: Int32 {
        tomato.framebuffer_width()
    }
    
    
    public nonisolated func press(button: TomatoButton) {
        tomato.press_button(button.uint32)
    }
    
    public nonisolated func release(button: TomatoButton) {
        tomato.release_button(button.uint32)
    }
    
    
    public nonisolated func audioBuffer(callback: tomato.AudioVideoBufferCallback) {
        tomato.audio_buffer_callback(callback)
    }
    
    public nonisolated func videoBuffer(callback: tomato.AudioVideoBufferCallback) {
        tomato.video_buffer_callback(callback)
    }
    
    
    public func setContext(context: UnsafeMutableRawPointer) {
        tomato.set_context(context)
    }
    
    
    public func setSetting<T>(setting: tomato.SETTING, value: T) {
        switch value {
        case let boolSetting as Bool:
            tomato.set_setting(setting, boolSetting)
        default:
            break
        }
    }
    
    
    public nonisolated func boxartURLString(for url: URL) -> String? {
        let title: String = url.deletingPathExtension().lastPathComponent
        
        return "https://raw.githubusercontent.com/libretro/libretro-thumbnails/refs/heads/master/Nintendo - Game Boy Advance/Named_Boxarts/\(title).png"
    }
    
    
    public nonisolated func saveStatePath(for index: Int) -> String {
        String(tomato.save_state_path(Int32(index)))
    }
    
    public func saveStateExists(for index: Int) -> Bool {
        tomato.save_state_exists(Int32(index))
    }
    
    public func saveStateLoad(for index: Int) {
        tomato.load_state(Int32(index))
    }
    
    public func saveStateSave(for index: Int) {
        tomato.save_state(Int32(index))
    }
}
