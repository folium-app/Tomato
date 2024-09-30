//
//  Tomato.swift
//  Tomato
//
//  Created by Jarrod Norwell on 12/9/2024.
//  Copyright © 2024 Jarrod Norwell. All rights reserved.
//

import Foundation

public actor Tomato {
    public static var shared = Tomato()
    
    public let tomatoObjC = TomatoObjC.shared()
    
    public func insertCartridge(from url: URL) {
        tomatoObjC.insertCartridge(url)
    }
    
    public func loop() {
        tomatoObjC.loop()
    }
}
