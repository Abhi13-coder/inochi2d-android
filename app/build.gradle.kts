plugins {
    id("com.android.application")
}

android {
    namespace = "com.abhiram.inochi2ddemo"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.abhiram.inochi2ddemo"
        minSdk = 29
        targetSdk = 34
        versionCode = 1
        versionName = "0.1"
        ndk { abiFilters += listOf("armeabi-v7a") }
    }

    buildTypes {
        debug { isMinifyEnabled = false }
    }
}
