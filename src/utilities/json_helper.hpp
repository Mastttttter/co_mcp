#pragma once
#include <meta>
#include <nlohmann/json.hpp>
#include "enum_helper.hpp"

namespace json_helper {
namespace json_meta {
struct ignore_t {};

inline constexpr ignore_t ignore{};

struct serializable_t {};

inline constexpr serializable_t serializable{};

template <typename T>
struct default_value {
  T value;
};

struct default_string_t {
  char const *value;
};

inline consteval default_string_t default_string(std::string_view s) {
  return default_string_t(std::define_static_string(s));
}

struct rename_t {
  char const *value;
};

inline consteval rename_t rename(std::string_view name) {
  return rename_t(std::define_static_string(name));
}

}  // namespace json_meta

namespace details {
// template <std::meta::info M, typename T>
// void demo(T &obj) {
//   using namespace std::string_literals;
//   using Member = std::remove_cvref_t<decltype(obj.[:M:])>;
//   static_assert(std::same_as<decltype(obj.[:M:]), Member>, "not same
//   type!!!");
//   static_assert(details::is_json_serializable<^^decltype(obj.[:M:])>() ==
//                     details::is_json_serializable<^^Member>(),
//                 "not same annotation!!!");
// }

template <typename T>
inline constexpr bool always_false_v = false;

template <typename T>
concept josn_get_to_able =
    requires(nlohmann::json const &o, T &value) { o.get_to(value); };

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename Allocator>
struct is_vector<std::vector<T, Allocator>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<std::remove_cvref_t<T>>::value;

template <std::meta::info M, typename Ann>
consteval std::optional<Ann> get_unique_annotation() {
  std::optional<Ann> result;
  for (std::meta::info a: std::meta::annotations_of_with_type(M, ^^Ann)) {
    if (result.has_value()) {
      throw std::runtime_error("duplicate annotation");
    }
    result = std::meta::extract<Ann>(a);
  }
  return result;
}

template <std::meta::info M>
consteval bool is_ignored_member() {
  return get_unique_annotation<M, json_meta::ignore_t>().has_value();
}

template <std::meta::info M>
constexpr bool is_json_serializable() {
  return get_unique_annotation<std::meta::dealias(M),
                               json_meta::serializable_t>()
      .has_value();
}

template <typename T>
nlohmann::json json_value(T const &value);

template <typename T>
void apply_annotations_defaults_into(T &obj);

template <std::meta::info M, typename T>
void apply_default_for_member(T &obj);

template <typename T>
void json_assign(nlohmann::json const &json, T &out);

}  // namespace details

template <typename T>
  requires std::is_class_v<T>
nlohmann::json reflect_to_json(T const &o);

template <typename T>
void from_json_reflect_into(nlohmann::json const &json, T &obj);

template <typename T>
void details::json_assign(nlohmann::json const &json, T &out) {
  using U = std::remove_cvref_t<T>;
  if constexpr (details::is_vector_v<U>) {
    if (!json.is_array()) {
      throw std::runtime_error("except json array");
    }
    out.clear();
    using Elem = typename U::value_type;
    for (auto const &item: json) {
      Elem elem{};
      details::json_assign(item, elem);
      out.emplace_back(std::move(elem));
    }
  } else if constexpr (std::is_enum_v<T>) {
    auto enum_value = enum_helper::string_to_enum<T>(json.get<std::string>());
    if (enum_value.has_value()) {
      out = enum_value.value();
    } else {
      throw std::runtime_error("invalid enum type from json");
    }
  } else if constexpr (details::josn_get_to_able<U>) {
    json.get_to(out);
  } else if constexpr (std::is_class_v<U>) {
    from_json_reflect_into(json, out);
  } else {
    static_assert(details::always_false_v<U>, "invalid type to json");
  }
}

template <std::meta::info M, typename T>
void details::apply_default_for_member(T &obj) {
  using namespace std::string_literals;
  using Member = std::remove_cvref_t<decltype(obj.[:M:])>;
  if constexpr (std::same_as<Member, std::string>) {
    constexpr auto ann =
        details::get_unique_annotation<M, json_meta::default_string_t>();
    if constexpr (ann.has_value()) {
      obj.[:M:] = ann->value;
    }
  } else if constexpr (std::is_arithmetic_v<Member> || std::is_enum_v<Member>) {
    constexpr auto ann =
        details::get_unique_annotation<M, json_meta::default_value<Member>>();
    if constexpr (ann.has_value()) {
      obj.[:M:] = ann->value;
    }
  } else if constexpr (std::is_class_v<Member>) {
    if constexpr (!details::is_json_serializable<^^Member>()) {
      throw std::runtime_error("object should be esrializable:"s +
                               std::meta::identifier_of(^^decltype(obj.[:M:])));
    }
    constexpr auto ann =
        details::get_unique_annotation<M, json_meta::default_value<Member>>();
    if constexpr (ann.has_value()) {
      obj.[:M:] = ann->value;
    } else {
      apply_annotations_defaults_into(obj.[:M:]);
    }
  }
}

template <typename T>
void details::apply_annotations_defaults_into(T &obj) {
  using namespace std::meta;
  constexpr auto ctx = access_context::current();
  template for (constexpr auto M:
                std::define_static_array(nonstatic_data_members_of(^^T, ctx))) {
    if constexpr (!details::is_ignored_member<M>()) {
      details::apply_default_for_member<M>(obj);
    }
  }
}

template <typename T>
void from_json_reflect_into(nlohmann::json const &json, T &obj) {
  using namespace std::meta;
  if (!json.is_object()) {
    throw std::runtime_error("expected json object");
  }
  if (!details::is_json_serializable<^^T>()) {
    throw std::runtime_error("object should be esrializable");
  }
  constexpr auto ctx = access_context::current();
  template for (constexpr auto M:
                std::define_static_array(nonstatic_data_members_of(^^T, ctx))) {
    if constexpr (details::is_ignored_member<M>()) {
      continue;
    } else if constexpr (has_identifier(M)) {
      std::string_view name = identifier_of(M);
      if constexpr (constexpr auto rename =
                        details::get_unique_annotation<M,
                                                       json_meta::rename_t>();
                    rename.has_value()) {
        name = rename->value;
      }
      auto it = json.find(std::string{name});
      if (it != json.end()) {
        details::json_assign(it.value(), obj.[:M:]);
      } else {
        details::apply_default_for_member<M>(obj);
      }
    }
  }
}

template <typename T>
  requires std::default_initializable<T>
T from_json_reflect(nlohmann::json const &json) {
  T obj{};
  from_json_reflect_into(json, obj);
  return obj;
}

template <typename T>
nlohmann::json details::json_value(T const &value) {
  if constexpr (std::is_enum_v<T>) {
    return nlohmann::json(enum_helper::enum_to_string(value));
  } else if constexpr (requires { nlohmann::json(value); }) {
    return nlohmann::json(value);
  } else if constexpr (std::is_class_v<std::remove_cvref_t<T>>) {
    return reflect_to_json(value);
  } else {
    static_assert(details::always_false_v<T>, "invalid type to json");
  }
}

template <typename T>
  requires std::is_class_v<T>
nlohmann::json reflect_to_json(T const &o) {
  using namespace std::meta;
  if constexpr (!details::is_json_serializable<^^T>()) {
    throw std::runtime_error("object need to be serializable");
  }
  nlohmann::json json_obj = nlohmann::json::object();
  constexpr auto ctx = access_context::current();
  template for (constexpr auto M:
                std::define_static_array(nonstatic_data_members_of(^^T, ctx))) {
    if constexpr (!details::is_ignored_member<M>() && has_identifier(M)) {
      std::string_view name = identifier_of(M);
      if constexpr (constexpr auto rename =
                        details::get_unique_annotation<M,
                                                       json_meta::rename_t>();
                    rename.has_value()) {
        name = rename->value;
      }
      json_obj[std::string{name}] = details::json_value(o.[:M:]);
    }
  }
  return json_obj;
}

}  // namespace json_helper
