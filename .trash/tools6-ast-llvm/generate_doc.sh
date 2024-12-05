#!/bin/bash

# Vérifier si LLVM est installé
if ! command -v brew &> /dev/null; then
    echo "Homebrew n'est pas installé. Veuillez l'installer d'abord."
    exit 1
fi

if [ ! -d "/usr/local/opt/llvm" ]; then
    echo "LLVM n'est pas installé. Installation en cours..."
    brew install llvm
fi

if [ ! -d "/usr/local/opt/nlohmann-json" ]; then
    echo "nlohmann-json n'est pas installé. Installation en cours..."
    brew install nlohmann-json
fi

if [ ! -d "/usr/local/opt/yaml-cpp" ]; then
    echo "yaml-cpp n'est pas installé. Installation en cours..."
    brew install yaml-cpp
fi

# Définir les variables d'environnement pour LLVM
export LLVM_DIR="/usr/local/opt/llvm/lib/cmake/llvm"
export Clang_DIR="/usr/local/opt/llvm/lib/cmake/clang"
export PATH="/usr/local/opt/llvm/bin:$PATH"
export LDFLAGS="-L/usr/local/opt/llvm/lib"
export CPPFLAGS="-I/usr/local/opt/llvm/include"

# Créer le build directory
mkdir -p build
cd build

# Nettoyer l'ancien build si nécessaire
rm -rf *

# Compiler le générateur de doc
echo "Configuration avec CMake..."
cmake .. || exit 1

echo "Compilation..."
make || exit 1

# Exécuter sur les fichiers sources
echo "Génération de la documentation..."
./api_doc_gen ../../src/main.cpp

# Copier la doc générée
if [ -f openapi.json ] && [ -f openapi.yaml ]; then
    cp openapi.* ../..
    echo "Documentation générée avec succès!"
else
    echo "Erreur: Les fichiers de documentation n'ont pas été générés."
    exit 1
fi