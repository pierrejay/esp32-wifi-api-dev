// #pragma once
// #include <expected>

// // Alias de type pour simplifier l'utilisation de std::expected
// template<typename S, typename E>
// using Result = std::expected<S, E>;

// // Helper pour retourner un succès
// template<typename S>
// auto Success(S&& value) {
//     return std::expected<S, auto>(std::forward<S>(value));
// }

// // Helper pour retourner une erreur
// template<typename E>
// auto Error(E&& error) {
//     return std::unexpected(std::forward<E>(error));
// }