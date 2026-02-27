#include "imgui_impl_opengl3_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../utils/stb_image.h"
#include "TextureLoader.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
#include <iostream>

TextureLoader::TextureLoader() = default;

unsigned int TextureLoader::LoadTextureFromFile(const char* filename) {
    int width, height, nrChannels;

    // 1. Attempt to load the image data
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 4);

    if (!data) {
        // Log to our UI console in Red
        Utils::logger().Log(LogLevel::ERR, "Texture Fail: " + std::string(filename) + " (Reason: " + stbi_failure_reason() + ")");
        return 0;
    }

    // 2. Validate dimensions (prevents OpenGL from crashing on 0x0 images)
    if (width <= 0 || height <= 0) {
        Utils::logger().Log(LogLevel::ERR, "Texture Corrupt: " + std::string(filename) + " has invalid dimensions.");
        stbi_image_free(data);
        return 0;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Setup scaling filters (GL_NEAREST is perfect for QR codes)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 3. Upload pixels to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // 4. Clean up CPU memory
    stbi_image_free(data);

    // 5. Success Log (Debug level so it doesn't clutter the main log unless needed)
    std::string dimensionInfo = std::to_string(width) + "x" + std::to_string(height);
    Utils::logger().Log(LogLevel::DEBUG, "Texture Loaded: " + std::string(filename) + " [" + dimensionInfo + "]");

    return textureID;
}