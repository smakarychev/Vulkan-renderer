#pragma once

#include "types.h"

#include <random>
#include <glm/glm.hpp>

class Random
{
public:
    static f32 Float();
    static f32 Float(f32 left, f32 right);
    static glm::vec2 Float2();
    static glm::vec2 Float2(f32 left, f32 right);
    static glm::vec3 Float3();
    static glm::vec3 Float3(f32 left, f32 right);
    static glm::vec4 Float4();
    static glm::vec4 Float4(f32 left, f32 right);

    static i32 Int32();
    static i32 Int32(i32 left, i32 right);
    static u32 UInt32();
    static u32 UInt32(u32 left, u32 right);
    static i64 Int64();
    static i64 Int64(i64 left, i64 right);
    static u64 UInt64();
    static u64 UInt64(u64 left, u64 right);
		
private:
    static std::random_device m_Device;
    static std::mt19937 m_Mt;
    static std::uniform_real_distribution<> m_UniformNormalizedReal;
};
