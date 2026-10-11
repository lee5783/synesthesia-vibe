//
//  settings.gradle.kts
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "synesthesiavibe-sample"
include(":app")
includeBuild("../../platform/android")
