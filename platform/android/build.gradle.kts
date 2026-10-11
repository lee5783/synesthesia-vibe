//
//  build.gradle.kts
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import java.util.Properties

plugins {
    id("com.android.library") version "8.13.1"
    `maven-publish`
}

group = "com.synesthesiavibe"
version = rootDir.resolve("../../VERSION").takeIf { it.isFile }?.readText()?.trim() ?: "0.0.1"

val vcpkgTriplets = mapOf(
    "arm64-v8a" to "arm64-android",
    "x86_64" to "x64-android",
)

val localProperties = Properties().apply {
    val file = rootDir.resolve("local.properties")
    if (file.isFile) file.inputStream().use { load(it) }
}

val vcpkgRoot: String = providers.environmentVariable("VCPKG_ROOT").orNull
    ?: localProperties.getProperty("vcpkg.dir")
    ?: error("Set VCPKG_ROOT or vcpkg.dir in local.properties")

android {
    namespace = "com.synesthesiavibe"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 26

        ndk {
            abiFilters += vcpkgTriplets.keys
        }

        externalNativeBuild {
            cmake {
                val ndkToolchain = android.sdkDirectory.resolve("ndk/$ndkVersion/build/cmake/android.toolchain.cmake")
                arguments += listOf(
                    "-DSYNESTHESIA_VIBE_GL_DEV=ON",
                    "-DSYNESTHESIA_VIBE_BACKEND=gles",
                    "-DSYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS=OFF",
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_PLATFORM=android-26",
                    "-DCMAKE_TOOLCHAIN_FILE=${file("src/main/cpp/vcpkg-android-toolchain.cmake")}",
                    "-DSYNESTHESIA_VIBE_VCPKG_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake",
                    "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ndkToolchain",
                    "-DSYNESTHESIA_VIBE_VCPKG_TRIPLETS=${vcpkgTriplets.map { (abi, triplet) -> "$abi=$triplet" }.joinToString(",")}",
                )
                targets += "synesthesiavibe-jni"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false

            externalNativeBuild {
                cmake {
                    arguments += "-DCMAKE_BUILD_TYPE=Release"
                }
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "4.4.3"
        }
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

dependencies {
    compileOnly("androidx.annotation:annotation:1.9.1")
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = project.group.toString()
            artifactId = project.name
            version = project.version.toString()

            afterEvaluate {
                from(components["release"])
            }
        }
    }
}
