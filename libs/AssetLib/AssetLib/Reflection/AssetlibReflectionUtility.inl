#pragma once

#include <CoreLib/Containers/Guid.h>

#include "CorelibReflectionUtility.inl"

template <>
struct glz::meta<lux::assetlib::AssetId>
{
	static constexpr auto ReadId = [](lux::assetlib::AssetId& id, const std::string& input) { 
		id.FromU64(std::stoull(input)); 
	};
   	static constexpr auto WriteId = [](auto& id) -> auto { return std::to_string(id.AsU64()); };
   	static constexpr auto value = glz::custom<ReadId, WriteId>;
};