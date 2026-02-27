//
// Created by archdev on 2/20/26.
//

#ifndef SBTPC_TEXTURELOADER_H
#define SBTPC_TEXTURELOADER_H

#pragma once

class TextureLoader {
public:
    TextureLoader();
    static unsigned int LoadTextureFromFile(const char* filename);
};

#endif //SBTPC_TEXTURELOADER_H