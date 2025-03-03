// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "Tomato",
    platforms: [
        .iOS(.v15),
        .tvOS(.v15)
    ],
    products: [
        .library(name: "Tomato", targets: ["Tomato"]),
        .library(name: "TomatoCXX", targets: ["TomatoCXX"]),
        .library(name: "TomatoObjC", targets: ["TomatoObjC"])
    ],
    dependencies: [
        .package(url: "https://github.com/jarrodnorwell/HQx", branch: "main"),
        .package(url: "https://github.com/jarrodnorwell/xBRZ", branch: "main"),
        .package(url: "https://github.com/ctreffs/SwiftSDL2", branch: "master")
    ],
    targets: [
        .target(name: "Tomato", dependencies: [
            "TomatoObjC"
        ]),
        .target(name: "TomatoCXX", dependencies: [
            .product(name: "SDL", package: "SwiftSDL2")
        ], publicHeadersPath: "include", cxxSettings: [
            .unsafeFlags([
                "-fbracket-depth=4096"
            ])
        ]),
        .target(name: "TomatoObjC", dependencies: [
            .product(name: "HQx", package: "HQx"),
            .product(name: "xBRZ", package: "xBRZ"),
            "TomatoCXX"
        ], publicHeadersPath: "include")
    ],
    cLanguageStandard: .c17,
    cxxLanguageStandard: .cxx17
)
