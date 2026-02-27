cmake_minimum_required(VERSION 3.20)
project(SBTPC) # Ensure this matches your project folder name

set(CMAKE_CXX_STANDARD 20)

# 1. Tell CMake where to find your class headers
include_directories(include)
include_directories(utils/imgui) # If your imgui files are in a subfolder

# 2. Find the necessary system libraries
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Zint REQUIRED)

# 4. Collect all source files (main.cpp, your src folder, and imgui folder)
file(GLOB_RECURSE SOURCES
        "main.cpp"
        "src/*.cpp"
        "utils/imgui/*.cpp"
)

# 5. Create the executable
add_executable(SBTPC ${SOURCES}
        menu/Menu.cpp
        core/servermanager/ServerManager.cpp
        core/servermanager/ServerManager.h
        core/catalogmanager/CatalogManager.cpp
        core/catalogmanager/CatalogManager.h
        core/routemanager/RouteManager.h
        core/invoicemanager/InvoiceManager.h
        core/invoicemanager/InvoiceManager.cpp
        utils/textureloader/TextureLoader.cpp
        utils/textureloader/TextureLoader.h
        utils/stb_image.h
        core/ordermanager/OrderManager.cpp
        core/ordermanager/OrderManager.h
        menu/dashboard/Dashboard.cpp
        menu/catalog/Catalog.cpp
        core/Core.cpp
        core/Core.h
        menu/dashboard/Invoicing.cpp
        core/routemanager/RouteManager.cpp
        utils/Utils.h
        utils/Utils.cpp
        utils/logger/Logger.cpp
        utils/logger/Logger.h
        utils/Structs.h
)

# 6. Link everything (Using the variables found above)
target_link_libraries(SBTPC PRIVATE
        glfw
        GLEW::GLEW
        OpenGL::GL
        Threads::Threads
        nlohmann_json::nlohmann_json
        zint
)