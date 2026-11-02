plugins { id("com.android.application") }
val softwareNotices = file("D:/VR4ALL-Gunblade-Build/quest/android/app/generated/software-notices")
val prepareSoftwareNotices = tasks.register<Copy>("prepareSoftwareNotices") {
    into(softwareNotices.resolve("notices"))
    from(rootProject.file("../LICENSE"), rootProject.file("../CREDITS.md"),
         rootProject.file("../engine-lock.json"), rootProject.file("../game-set.json"),
         rootProject.file("../docs/platform-notes.md"))
    from(rootProject.file("../licenses")) { into("licenses") }
}
android {
    namespace = "com.vrlightgunhub.gunbladeny"
    ndkVersion = "29.0.14206865"
    compileSdk { version = release(36) { minorApiLevel = 1 } }
    defaultConfig {
        applicationId = "com.vrlightgunhub.gunbladeny"
        minSdk = 29
        targetSdk = 36
        versionCode = 9
        versionName = "0.1.8-beta"
        ndk { abiFilters += "arm64-v8a" }
    }
    signingConfigs { create("local") {
        storeFile = file("D:/VR4ALL-Gunblade-Build/private/gunblade.keystore")
        storePassword = "android"
        keyAlias = "gunblade"
        keyPassword = "android"
    } }
    buildTypes { getByName("release") { signingConfig = signingConfigs.getByName("local"); isMinifyEnabled = false } }
    sourceSets.getByName("main").assets.srcDirs("../../assets", softwareNotices)
    sourceSets.getByName("main").jniLibs.srcDirs(providers.gradleProperty("gunbladeNativeDir").orNull ?: file("D:/VR4ALL-Gunblade-Build/quest/jniLibs"))
    packaging { jniLibs { useLegacyPackaging = true } }
}

layout.buildDirectory.set(file("D:/VR4ALL-Gunblade-Build/quest/android/app"))
tasks.named("preBuild").configure { dependsOn(prepareSoftwareNotices) }
