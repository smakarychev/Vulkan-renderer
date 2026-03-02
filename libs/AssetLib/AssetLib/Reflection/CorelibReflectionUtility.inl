#pragma once

#include <CoreLib/Math/Transform.h>
#include <CoreLib/Math/Geometry.h>

#include <glaze/glaze.hpp>
#include <glm/glm.hpp>

template <typename T, glm::qualifier Q>
struct glz::meta<glm::vec<2, T, Q>>
{
    using Type = glm::vec<2, T, Q>;
    static constexpr auto value = object(&Type::x, &Type::y);
};
template <typename T, glm::qualifier Q>
struct glz::meta<glm::vec<3, T, Q>>
{
    using Type = glm::vec<3, T, Q>;
    static constexpr auto value = object(&Type::x, &Type::y, &Type::z);
};
template <typename T, glm::qualifier Q>
struct glz::meta<glm::vec<4, T, Q>>
{
    using Type = glm::vec<4, T, Q>;
    static constexpr auto value = object(&Type::x, &Type::y, &Type::z, &Type::w);
};
template <typename T, glm::qualifier Q>
struct glz::meta<glm::qua<T, Q>>
{
    using Type = glm::qua<T, Q>;
    static constexpr auto value = object(&Type::x, &Type::y, &Type::z, &Type::w);
};

template <>
struct glz::meta<lux::Guid>
{
    static constexpr auto ReadId = [](lux::Guid& guid, const std::string& input) {
        guid = lux::Guid::FromString(input); 
    };
    static constexpr auto WriteId = [](auto& guid) -> auto { return std::format("{}", guid); };
    static constexpr auto value = glz::custom<ReadId, WriteId>;
};

template <>
struct glz::meta<Transform3d> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<Sphere> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<AABB> : lux::assetlib::reflection::CamelCase {};
