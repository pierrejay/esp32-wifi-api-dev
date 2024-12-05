#ifndef API_DOC_BACKEND_H
#define API_DOC_BACKEND_H

#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Tooling/Core/Replacement.h>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <sstream>

using json = nlohmann::json;

// Obtenir le chemin de base de PlatformIO
static std::string getPlatformIOHome() {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        return "";
    }
    return std::string(home) + "/.platformio";
}

// Obtenir les chemins d'inclusion pour Arduino ESP32
static std::vector<std::string> getArduinoIncludes() {
    std::string pioHome = getPlatformIOHome();
    if (pioHome.empty()) {
        return {};
    }

    return {
        "-I" + pioHome + "/packages/framework-arduinoespressif32/cores/esp32",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/freertos/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/freertos/include/esp_additions/freertos",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/esp_common/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/esp_hw_support/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/esp_rom/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/hal/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/tools/sdk/esp32/include/soc/esp32/include",
        "-I" + pioHome + "/packages/framework-arduinoespressif32/variants/esp32"
    };
}

struct APIRoute {
    std::string module;
    std::string path;
    std::string method;
    std::string description;
    json params;
    json responses;
};

class APIDocBackend {
private:
    std::vector<APIRoute> routes;

    class APIConsumer : public clang::ASTConsumer {
    private:
        APIDocBackend& backend;
        clang::ASTContext* context;

    public:
        explicit APIConsumer(APIDocBackend& b) : backend(b) {}

        void HandleTranslationUnit(clang::ASTContext& Context) override {
            context = &Context;
            clang::TranslationUnitDecl* TU = Context.getTranslationUnitDecl();
            
            APIVisitor visitor(backend);
            visitor.TraverseDecl(TU);
        }
    };

    class APIFrontendAction : public clang::ASTFrontendAction {
    public:
        explicit APIFrontendAction(APIDocBackend& b) : backend(&b) {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance& CI, llvm::StringRef InFile) override {
            return std::make_unique<APIConsumer>(*backend);
        }

    private:
        APIDocBackend* backend;
    };

    class APIFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        APIDocBackend& backend;

    public:
        explicit APIFrontendActionFactory(APIDocBackend& b) : backend(b) {}

        std::unique_ptr<clang::FrontendAction> create() override {
            return std::make_unique<APIFrontendAction>(backend);
        }
    };

    void visitMethodRegistration(const clang::CallExpr* call) {
        if (call->getNumArgs() < 3) return;

        const auto* moduleArg = call->getArg(0);
        const auto* pathArg = call->getArg(1);
        const auto* builderArg = call->getArg(2);

        APIRoute route;

        if (const auto* moduleLiteral = llvm::dyn_cast<clang::StringLiteral>(moduleArg)) {
            route.module = moduleLiteral->getString().str();
        }

        if (const auto* pathLiteral = llvm::dyn_cast<clang::StringLiteral>(pathArg)) {
            route.path = pathLiteral->getString().str();
        }

        analyzeMethodBuilder(builderArg, route);

        routes.push_back(route);
    }

    void analyzeMethodBuilder(const clang::Expr* builder, APIRoute& route) {
        const auto* memberCall = llvm::dyn_cast<clang::CXXMemberCallExpr>(builder);
        while (memberCall) {
            const auto* method = memberCall->getMethodDecl();
            std::string methodName = method->getNameAsString();

            if (methodName == "desc") {
                if (const auto* arg = memberCall->getArg(0)) {
                    if (const auto* literal = llvm::dyn_cast<clang::StringLiteral>(arg)) {
                        route.description = literal->getString().str();
                    }
                }
            }
            else if (methodName == "param") {
                extractParameter(memberCall, route.params);
            }
            else if (methodName == "response") {
                extractResponse(memberCall, route.responses);
            }

            memberCall = llvm::dyn_cast<clang::CXXMemberCallExpr>(
                memberCall->getImplicitObjectArgument());
        }
    }

    void extractParameter(const clang::CXXMemberCallExpr* call, json& params) {
        if (call->getNumArgs() < 2) return;

        const auto* nameArg = call->getArg(0);
        const auto* typeArg = call->getArg(1);

        std::string name;
        std::string type;

        if (const auto* nameLiteral = llvm::dyn_cast<clang::StringLiteral>(nameArg)) {
            name = nameLiteral->getString().str();
        }

        if (const auto* typeExpr = llvm::dyn_cast<clang::DeclRefExpr>(typeArg)) {
            type = typeExpr->getNameInfo().getName().getAsString();
        }

        if (!name.empty() && !type.empty()) {
            bool required = true;
            if (call->getNumArgs() >= 3) {
                if (const auto* boolLiteral = llvm::dyn_cast<clang::ConstantExpr>(call->getArg(2))) {
                    required = !boolLiteral->getResultAsAPSInt().getBoolValue();
                }
            }
            
            params[name] = {
                {"type", type},
                {"required", required}
            };
        }
    }

    void extractResponse(const clang::CXXMemberCallExpr* call, json& responses) {
        if (call->getNumArgs() < 2) return;

        const auto* nameArg = call->getArg(0);
        const auto* schemaArg = call->getArg(1);

        std::string name;
        if (const auto* nameLiteral = llvm::dyn_cast<clang::StringLiteral>(nameArg)) {
            name = nameLiteral->getString().str();
        }

        if (!name.empty()) {
            if (const auto* initListExpr = llvm::dyn_cast<clang::InitListExpr>(schemaArg)) {
                json schema;
                extractResponseSchema(initListExpr, schema);
                responses[name] = schema;
            }
        }
    }

    void extractResponseSchema(const clang::InitListExpr* initList, json& schema) {
        for (const auto* init : initList->inits()) {
            if (const auto* mapItem = llvm::dyn_cast<clang::CXXConstructExpr>(init)) {
                if (mapItem->getNumArgs() >= 2) {
                    const auto* keyArg = mapItem->getArg(0);
                    const auto* valueArg = mapItem->getArg(1);

                    std::string key;
                    if (const auto* keyLiteral = llvm::dyn_cast<clang::StringLiteral>(keyArg)) {
                        key = keyLiteral->getString().str();
                    }

                    if (!key.empty()) {
                        if (const auto* valueInitList = llvm::dyn_cast<clang::InitListExpr>(valueArg)) {
                            json subSchema;
                            extractResponseSchema(valueInitList, subSchema);
                            schema[key] = subSchema;
                        }
                        else if (const auto* typeExpr = llvm::dyn_cast<clang::DeclRefExpr>(valueArg)) {
                            schema[key] = {
                                {"type", typeExpr->getNameInfo().getName().getAsString()}
                            };
                        }
                    }
                }
            }
        }
    }

    // Liste des fichiers à conserver (les autres seront remplacés par des stubs vides)
    const std::vector<std::string> API_FILES = {
        "WiFiManagerAPI.h",
        "APIServer.h",
        "main.cpp"
    };

    // Cache des fichiers déjà prétraités
    std::map<std::string, std::string> processedFiles;

    // Fonction pour lire un fichier
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        return std::string(std::istreambuf_iterator<char>(file),
                          std::istreambuf_iterator<char>());
    }

    // Fonction pour extraire le chemin d'un include
    std::pair<std::string, std::string> parseInclude(const std::string& line) {
        size_t start = line.find("<");
        bool isSystemInclude = true;
        if (start == std::string::npos) {
            start = line.find("\"");
            isSystemInclude = false;
        }
        size_t end = line.find(">");
        if (end == std::string::npos) {
            end = line.find("\"", start + 1);
        }
        
        if (start != std::string::npos && end != std::string::npos) {
            std::string header = line.substr(start + 1, end - start - 1);
            return {header, isSystemInclude ? "<" + header + ">" : "\"" + header + "\""};
        }
        return {"", ""};
    }

    // Fonction récursive pour prétraiter les includes
    std::string preprocessSourceCodeRecursive(const std::string& sourceCode, const std::string& currentFile, const std::vector<std::string>& includePaths) {
        std::istringstream stream(sourceCode);
        std::string line;
        std::string result;
        bool inApiDeclaration = false;
        bool inDocComment = false;
        std::string currentComment;
        
        while (std::getline(stream, line)) {
            // Gérer les commentaires de documentation
            if (line.find("/**") != std::string::npos) {
                inDocComment = true;
                result += line + "\n";
                continue;
            }
            
            if (inDocComment) {
                result += line + "\n";
                if (line.find("*/") != std::string::npos) {
                    inDocComment = false;
                }
                continue;
            }

            // Détecter les routes API
            if (line.find("API_ROUTE") != std::string::npos || 
                line.find("API_GET") != std::string::npos || 
                line.find("API_POST") != std::string::npos) {
                inApiDeclaration = true;
                result += line + "\n";
                continue;
            }

            // Si on est dans une déclaration API, garder les lignes jusqu'à la fin de la déclaration
            if (inApiDeclaration) {
                result += line + "\n";
                if (line.find("{") != std::string::npos) {
                    inApiDeclaration = false;
                    result += "} // end of API route\n";
                }
                continue;
            }

            // Pour les includes
            if (line.find("#include") != std::string::npos) {
                auto [header, originalInclude] = parseInclude(line);
                if (header.empty()) {
                    continue;
                }

                bool isApiFile = false;
                for (const auto& apiFile : API_FILES) {
                    if (header.find(apiFile) != std::string::npos) {
                        isApiFile = true;
                        break;
                    }
                }

                if (isApiFile) {
                    // Inclure et prétraiter les fichiers API
                    std::string headerPath;
                    for (const auto& includePath : includePaths) {
                        std::string testPath = includePath + "/" + header;
                        if (std::ifstream(testPath).good()) {
                            headerPath = testPath;
                            break;
                        }
                    }

                    if (!headerPath.empty()) {
                        auto it = processedFiles.find(headerPath);
                        if (it != processedFiles.end()) {
                            result += it->second;
                        } else {
                            std::string headerContent = readFile(headerPath);
                            if (!headerContent.empty()) {
                                std::string processedHeader = preprocessSourceCodeRecursive(headerContent, headerPath, includePaths);
                                processedFiles[headerPath] = processedHeader;
                                result += processedHeader;
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

public:
    class APIVisitor : public clang::RecursiveASTVisitor<APIVisitor> {
        APIDocBackend& backend;

    public:
        explicit APIVisitor(APIDocBackend& b) : backend(b) {}

        bool VisitCallExpr(clang::CallExpr* call) {
            if (const auto* callee = call->getDirectCallee()) {
                std::string name = callee->getNameAsString();
                if (name == "registerMethod") {
                    backend.visitMethodRegistration(call);
                }
            }
            return true;
        }
    };

    bool processFile(const std::string& sourceCode) {
        // Chemins d'inclusion pour la recherche des fichiers
        std::vector<std::string> includePaths = {
            "../../src",
            "../../lib",
            "../../lib/WiFiManager/src"
        };

        // Prétraiter le code source pour ne garder que les routes API
        std::string processedCode = preprocessSourceCodeRecursive(sourceCode, "input.cc", includePaths);

        // Configurer les options du compilateur (minimal)
        std::vector<std::string> args = {
            "-xc++",
            "-std=c++17",
            "-DGENERATE_API_DOCS"
        };

        // Créer les arguments pour Clang Tooling
        std::vector<std::string> commandLine;
        commandLine.push_back("/usr/local/opt/llvm/bin/clang++");
        commandLine.insert(commandLine.end(), args.begin(), args.end());

        // Créer un compilateur avec les options
        clang::tooling::FixedCompilationDatabase Compilations(".", commandLine);
        std::vector<std::string> Sources;
        Sources.push_back("input.cc");

        // Créer l'outil et l'exécuter
        clang::tooling::ClangTool Tool(Compilations, Sources);
        Tool.mapVirtualFile("input.cc", processedCode);
        
        APIFrontendActionFactory Factory(*this);
        int result = Tool.run(&Factory);
        
        if (result != 0) {
            return false;
        }

        return generateDocs();
    }

    bool generateDocs() {
        json apiDoc;
        
        // Info de base
        apiDoc["openapi"] = "3.1.1";
        apiDoc["info"] = {
            {"title", "WiFi Manager API"},
            {"version", "1.0.0"},
            {"description", "WiFi configuration and monitoring API"},
            {"contact", {
                {"name", "Pierre"},
                {"email", ""}
            }},
            {"license", {
                {"name", "MIT"},
                {"identifier", "MIT"}
            }}
        };

        // Serveurs
        apiDoc["servers"] = json::array({
            {{"url", "http://device.local/api"}}
        });

        // Tags
        apiDoc["tags"] = json::array();
        std::set<std::string> modules;
        for (const auto& route : routes) {
            if (modules.insert(route.module).second) {
                apiDoc["tags"].push_back({
                    {"name", route.module},
                    {"description", route.description}
                });
            }
        }

        // Chemins
        for (const auto& route : routes) {
            std::string path = "/" + route.path;
            apiDoc["paths"][path][route.method] = {
                {"description", route.description},
                {"tags", json::array({route.module})},
                {"parameters", route.params},
                {"responses", {
                    {"200", {
                        {"description", "Successful operation"},
                        {"content", {
                            {"application/json", {
                                {"schema", route.responses}
                            }}
                        }}
                    }}
                }}
            };
        }

        // Sauvegarder en JSON
        std::ofstream jsonFile("openapi.json");
        jsonFile << apiDoc.dump(2);

        // Sauvegarder en YAML
        std::ofstream yamlFile("openapi.yaml");
        YAML::Node yamlDoc = YAML::Load(apiDoc.dump());
        yamlFile << yamlDoc;

        return true;
    }
};

#endif // API_DOC_BACKEND_H