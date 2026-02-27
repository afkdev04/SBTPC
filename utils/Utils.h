//
// Created by archdev on 2/25/26.
//

#ifndef SBTPC_UTILS_H
#define SBTPC_UTILS_H

class TextureLoader;
class Logger;

class Utils {
public:
    static TextureLoader& texture_loader();
    static Logger& logger();
};

#endif //SBTPC_UTILS_H