import org.gradle.api.tasks.Copy

plugins {
    id("com.android.application") version "8.7.3"
    id("org.jetbrains.kotlin.android") version "2.0.21"
}

android {
    namespace = "com.ykw2.port"
    compileSdk = 35

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        applicationId = "com.ykw2.port"
        minSdk = 26
        targetSdk = 35
        versionCode = 10
        versionName = "0.4.15"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                // Keep the debuggable APK usable on-device: the emulator core is
                // CPU-heavy, so leaving its C++ translation units unoptimized
                // makes the game run dramatically slower.
                cppFlags += listOf("-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets.getByName("main").jniLibs.srcDir(
        layout.buildDirectory.dir("generated/zakuroJniLibs"),
    )
    sourceSets.getByName("main").assets.srcDir("../cheats")
}

val cargoTargetRoot = providers.environmentVariable("CARGO_TARGET_DIR")
    .map { file(it) }
    .orElse(layout.buildDirectory.dir("zakuro-cargo").map { it.asFile })

androidComponents.onVariants { variant ->
    val capitalizedName = variant.name.replaceFirstChar { it.uppercase() }
    val stageRuntime = tasks.register<Copy>("stageZakuro${capitalizedName}Runtime") {
        dependsOn("externalNativeBuild${capitalizedName}")
        from(cargoTargetRoot.map {
            it.resolve("aarch64-linux-android/release/libykw2_mobile_host.so")
        })
        into(layout.buildDirectory.dir("generated/zakuroJniLibs/arm64-v8a"))
    }
    tasks.matching {
        it.name == "merge${capitalizedName}NativeLibs" ||
            it.name == "merge${capitalizedName}JniLibFolders"
    }.configureEach {
        dependsOn(stageRuntime)
    }
}

val versionedDebugApk = tasks.register<Copy>("copyVersionedDebugApk") {
    dependsOn("packageDebug")
    from(layout.buildDirectory.file("outputs/apk/debug/YKW2Port-debug.apk"))
    into(layout.buildDirectory.dir("outputs/apk/versioned"))
    rename { "YKW2Port-v${android.defaultConfig.versionName}-debug.apk" }
}

tasks.matching { it.name == "assembleDebug" }.configureEach {
    finalizedBy(versionedDebugApk)
}

tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
    compilerOptions.jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
}
