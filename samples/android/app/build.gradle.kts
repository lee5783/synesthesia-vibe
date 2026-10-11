//
//  build.gradle.kts
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

import groovy.json.JsonSlurper

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

val repoRoot: File = rootDir.resolve("../..").canonicalFile
val referenceResources: File = repoRoot.resolve("resources/beatdrop")
val captureSetFile: File = repoRoot.resolve("resources/beatdrop/capture-set.json")
val generatedAssets: Provider<Directory> = layout.buildDirectory.dir("generated/synesthesia-assets")

@Suppress("UNCHECKED_CAST")
val captureSetPresetFiles: List<String> = (JsonSlurper().parse(captureSetFile) as Map<String, Any>)
    .let { it["presets"] as List<Map<String, String>> }
    .map { it.getValue("file") }

val releaseSigningEnv: Map<String, String>? = listOf(
    "ANDROID_KEYSTORE_FILE",
    "ANDROID_KEYSTORE_PASSWORD",
    "ANDROID_KEY_ALIAS",
    "ANDROID_KEY_PASSWORD",
).associateWith { providers.environmentVariable(it).orNull.orEmpty() }
    .takeIf { env -> env.values.all { it.isNotEmpty() } }

val syncSynesthesiaAssets by tasks.registering(Sync::class) {
    into(generatedAssets)
    into("presets") {
        from(repoRoot.resolve("resources/mvp-presets")) { include("*.milk") }
    }
    into("capture-presets") {
        from(referenceResources.resolve("capture-presets")) { include(captureSetPresetFiles) }
    }
    into("textures") {
        from(referenceResources.resolve("textures"))
    }
    into("audio") {
        from(repoRoot.resolve("tests/tools/wav-generator/deterministic-test.wav"))
    }
    from(captureSetFile)
}

android {
    namespace = "com.synesthesiavibe.sample"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.synesthesiavibe.sample"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    releaseSigningEnv?.let { env ->
        val releaseSigning = signingConfigs.create("release") {
            storeFile = file(env.getValue("ANDROID_KEYSTORE_FILE"))
            storePassword = env.getValue("ANDROID_KEYSTORE_PASSWORD")
            keyAlias = env.getValue("ANDROID_KEY_ALIAS")
            keyPassword = env.getValue("ANDROID_KEY_PASSWORD")
        }
        buildTypes.getByName("release").signingConfig = releaseSigning
    }

    buildFeatures {
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    sourceSets {
        getByName("main") {
            assets.srcDir(generatedAssets)
        }
    }
}

kotlin {
    jvmToolchain(17)
}

tasks.named("preBuild") {
    dependsOn(syncSynesthesiaAssets)
}

dependencies {
    implementation("com.synesthesiavibe:synesthesiavibe")
    implementation(platform("androidx.compose:compose-bom:2026.06.01"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.12.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.10.0")
}
