#pragma once
#include <expected>
#include <utility>
#include <variant>

// Alias de type pour simplifier l'utilisation de std::expected
template<typename S, typename E>
using Result = std::expected<S, E>;

// Helper pour retourner un succès
template<typename S, typename E = const char*>  // Type d'erreur par défaut
auto Success(S&& value) {
    return Result<std::decay_t<S>, E>(std::forward<S>(value));
}

// Helper pour retourner une erreur
template<typename E>
auto Error(E&& error) {
    return std::unexpected<std::decay_t<E>>(std::forward<E>(error));
}
