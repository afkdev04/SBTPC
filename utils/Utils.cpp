//
// Created by archdev on 2/25/26.
//

#include "Utils.h"
#include "logger/Logger.h"
#include "textureloader/TextureLoader.h"

TextureLoader& Utils::texture_loader() {
    static TextureLoader instance;
    return instance;
};

Logger& Utils::logger() {
    static Logger instance; // The one and only instance
    return instance;
}