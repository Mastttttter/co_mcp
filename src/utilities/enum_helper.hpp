#pragma once
#include <meta>
#include <optional>
#include <string>

namespace enum_helper {
namespace details {
template <typename T>
inline constexpr bool always_false_v = false;
}

template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
  if constexpr (Enumerable) {
    template for (constexpr auto e:
                  std::define_static_array(std::meta::enumerators_of(^^E))) {
      if (value == [:e:]) {
        return std::meta::identifier_of(e);
      }
    }
  }
  return "<unnamed>";
}

template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr std::optional<E> string_to_enum(std::string_view name) {
  if constexpr (Enumerable) {
    template for (constexpr auto &e:
                  std::define_static_array(std::meta::enumerators_of(^^E))) {
      if (name == std::meta::identifier_of(e)) {
        return [:e:];
      }
    }
  }
  return std::nullopt;
}
}  // namespace enum_helper
