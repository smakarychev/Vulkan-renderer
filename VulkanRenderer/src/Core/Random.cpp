#include "Random.h"

std::random_device Random::m_Device;
std::mt19937 Random::m_Mt(13);
std::uniform_real_distribution<> Random::m_UniformNormalizedReal(0.0f, 1.0f);

f32 Random::Float()
{
    return static_cast<f32>(m_UniformNormalizedReal(m_Mt));
}

f32 Random::Float(f32 left, f32 right)
{
    return Float() * (right - left) + left;
}

glm::vec2 Random::Float2()
{
    return glm::vec2(Float(), Float());
}

glm::vec2 Random::Float2(f32 left, f32 right)
{
    return glm::vec2(Float(left, right), Float(left, right));
}

glm::vec3 Random::Float3()
{
    return glm::vec3(Float(), Float(), Float());
}

glm::vec3 Random::Float3(f32 left, f32 right)
{
    return glm::vec3(Float(left, right), Float(left, right), Float(left, right));
}

glm::vec4 Random::Float4()
{
    return glm::vec4(Float(), Float(), Float(), Float());
}

glm::vec4 Random::Float4(f32 left, f32 right)
{
    return glm::vec4(Float(left, right), Float(left, right), Float(left, right), Float(left, right));
}

i32 Random::Int32()
{
    static std::uniform_int_distribution<i32> distribution(std::numeric_limits<i32>::min(), std::numeric_limits<i32>::max());
    return distribution(m_Mt);
}

u32 Random::UInt32()
{
    static std::uniform_int_distribution<u32> distribution(std::numeric_limits<u32>::min(), std::numeric_limits<u32>::max());
    return distribution(m_Mt);
}

i32 Random::Int32(i32 left, i32 right)
{
    std::uniform_int_distribution<i32> distribution(left, right);
    return distribution(m_Mt);
}

u32 Random::UInt32(u32 left, u32 right)
{
    std::uniform_int_distribution<u32> distribution(left, right);
    return distribution(m_Mt);
}

i64 Random::Int64()
{
    static std::uniform_int_distribution<i64> distribution(std::numeric_limits<i64>::min(), std::numeric_limits<i64>::max());
    return distribution(m_Mt);
}

u64 Random::UInt64()
{
    static std::uniform_int_distribution<u64> distribution(std::numeric_limits<u64>::min(), std::numeric_limits<u64>::max());
    return distribution(m_Mt);
}

i64 Random::Int64(i64 left, i64 right)
{
    std::uniform_int_distribution<i64> distribution(left, right);
    return distribution(m_Mt);
}

u64 Random::UInt64(u64 left, u64 right)
{
    static std::uniform_int_distribution<u64> distribution(left, right);
    return distribution(m_Mt);
}
