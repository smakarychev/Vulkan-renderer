#pragma once

#include <AssetLib/Assetlib.h>
#include <CoreLib/Containers/Guid.h>

#include "CorelibReflectionUtility.inl"
#include "ImageFormatReflection.inl"

template <>
struct glz::meta<lux::assetlib::AssetId>
{
	static constexpr auto ReadId = [](lux::assetlib::AssetId& id, u64 input) { 
		id.FromU64(input); 
	};
   	static constexpr auto WriteId = [](auto& id) -> auto { return id.AsU64(); };
   	static constexpr auto value = glz::custom<ReadId, WriteId>;
};

template <> struct ::glz::meta<lux::assetlib::AssetTypeMetadata> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::AssetIoMetadata> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::AssetMetadata> : lux::assetlib::reflection::CamelCase {};

#define DEFINE_BASIC_METADATA_READ(metaType, metaOutput, pathParameter) \
	using namespace io; \
	metaType metaOutput = {}; \
	auto read = readFileToString(pathParameter); \
	ASSETLIB_CHECK_RETURN_IO_ERROR(read.has_value(), IoError::ErrorCode::FailedToOpen, \
		#metaType ": Failed to open: {}", pathParameter.string()) \
	const glz::error_ctx error = glz::read_json(meta, *read); \
	ASSETLIB_CHECK_RETURN_IO_ERROR(!error, IoError::ErrorCode::GeneralError, \
		#metaType ": Failed to parse: {} ({})", glz::format_error(error, *read), pathParameter.string())

#define DEFINE_BASIC_METADATA_PACK(metaType, metaOutput, metaParameter) \
	auto metaOutput = glz::write_json(metaParameter); \
	ASSETLIB_CHECK_RETURN_IO_ERROR(meta.has_value(), io::IoError::ErrorCode::GeneralError, \
		#metaType ": Failed to pack: {}", glz::format_error(meta.error()))


#define DEFINE_BASIC_HEADER_READ(headerType, resultOutput, metadataParameter) \
	auto headerRead = readFileToString((metadataParameter).Io.HeaderFile); \
	ASSETLIB_CHECK_RETURN_IO_ERROR(headerRead.has_value(), io::IoError::ErrorCode::GeneralError, \
		"Assetlib: Failed to read header file: {}", (metadataParameter).Io.HeaderFile.string()) \
	const auto resultOutput = glz::read_json<headerType>(*headerRead); \
	ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError, \
		"Assetlib: Failed to read: {}", glz::format_error(result.error(), *headerRead))