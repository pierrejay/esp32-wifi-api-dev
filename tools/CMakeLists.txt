cmake_minimum_required(VERSION 3.10)
project(gen)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Désactiver les warnings
if (MSVC)
    add_compile_options(/w)
else()
    add_compile_options(-w)
endif()

# Ajouter le chemin vers ArduinoJson
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/deps/ArduinoJson/src)

# Ajouter l'exécutable
add_executable(gen gen.cpp)

# Si besoin de flags de compilation supplémentaires
target_compile_options(gen PRIVATE -Wall -Wextra) 