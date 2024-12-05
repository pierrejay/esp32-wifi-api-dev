#include "api_doc_backend.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source-file>" << std::endl;
        return 1;
    }

    // Lire le contenu du fichier
    std::ifstream sourceFile(argv[1]);
    if (!sourceFile) {
        std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << sourceFile.rdbuf();
    std::string sourceCode = buffer.str();

    APIDocBackend backend;
    
    // Traiter le fichier source
    if (!backend.processFile(sourceCode)) {
        std::cerr << "Error processing file: " << argv[1] << std::endl;
        return 1;
    }

    std::cout << "API documentation generated successfully!" << std::endl;
    return 0;
}