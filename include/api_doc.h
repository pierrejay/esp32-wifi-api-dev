#pragma once

// En mode Arduino, ces macros ne font rien
#ifndef API_DOC_GEN
    #define API_DOC_INIT()
    #define API_DOC_GENERATE()
#else
    // En mode génération de doc, elles activent l'extraction
    #define API_DOC_INIT() APIDocGenerator::init()
    #define API_DOC_GENERATE() APIDocGenerator::generate()
#endif