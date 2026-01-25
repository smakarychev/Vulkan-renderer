#pragma once

#include "Containers/Guid.h"

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
struct glz::meta<lux::assetlib::AssetId>
{
	static constexpr auto ReadId = [](lux::assetlib::AssetId& id, const std::string& input) { 
		id.FromU64(std::stoull(input)); 
	};
   	static constexpr auto WriteId = [](auto& id) -> auto { return std::to_string(id.AsU64()); };
   	static constexpr auto value = glz::custom<ReadId, WriteId>;
};