#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "Math/Geometry.h"

// NOLINTBEGIN

TEST_CASE("Geometry AABB", "[Geometry]")
{
    AABB aabb{};
    REQUIRE(aabb.Min == glm::vec3{0.0f});
    REQUIRE(aabb.Max == glm::vec3{0.0f});
    SECTION("Merge leaves original unchanged")
    {
        AABB other = {
            .Min = {-1.0f, 2.0f, -3.0f},
            .Max = {3.0f, 3.0f, 7.0f}};
        other = aabb.Merge(other);
        REQUIRE(aabb.Min == glm::vec3{0.0f});
        REQUIRE(aabb.Max == glm::vec3{0.0f});
    }
    SECTION("Merge merges")
    {
        AABB other = {
            .Min = {-1.0f, 2.0f, -3.0f},
            .Max = {3.0f, 3.0f, 7.0f}};
        aabb = aabb.Merge(other);
        REQUIRE(aabb.Min == glm::vec3{-1.0f, 0.0f, -3.0f});
        REQUIRE(aabb.Max == glm::vec3{3.0f, 3.0f, 7.0f});
    }
    SECTION("Merge is commutative")
    {
        AABB other = {
            .Min = {-1.0f, 2.0f, -3.0f},
            .Max = {3.0f, 3.0f, 7.0f}};
        AABB merged = aabb.Merge(other);
        REQUIRE(merged.Min == glm::vec3{-1.0f, 0.0f, -3.0f});
        REQUIRE(merged.Max == glm::vec3{3.0f, 3.0f, 7.0f});
        merged = other.Merge(aabb);
        REQUIRE(merged.Min == glm::vec3{-1.0f, 0.0f, -3.0f});
        REQUIRE(merged.Max == glm::vec3{3.0f, 3.0f, 7.0f});
    }
    SECTION("Transform with default transform leaves AABB unchanged")
    {
        AABB aabb = {
            .Min = {-1.0f, 2.0f, -3.0f},
            .Max = {3.0f, 3.0f, 7.0f}};
        aabb = aabb.Transform({});
        REQUIRE(aabb.Min == glm::vec3{-1.0f, 2.0f, -3.0f});
        REQUIRE(aabb.Max == glm::vec3{3.0f, 3.0f, 7.0f});
    }
    SECTION("Transform transforms")
    {
        AABB aabb = {
            .Min = {-1.0f, 2.0f, -3.0f},
            .Max = {3.0f, 3.0f, 7.0f}};
        Transform3d transform3d = {
            .Position = {1.0f, 2.0f, 3.0f},
            .Orientation = glm::angleAxis(glm::degrees(45.0f), glm::normalize(glm::vec3{1.0f, 1.0f, 1.0f})),
            .Scale = glm::vec3{0.5f, 1.5f, 3.3f}};
        aabb = aabb.Transform(transform3d);

        /* I had to do it with blender :( */
        REQUIRE_THAT(aabb.Min.x, Catch::Matchers::WithinRel(-8.7565202f, 1e-6f));
        REQUIRE_THAT(aabb.Min.y, Catch::Matchers::WithinRel(0.5810874f, 1e-6f));
        REQUIRE_THAT(aabb.Min.z, Catch::Matchers::WithinRel(4.5417022f, 1e-6f));
        REQUIRE_THAT(aabb.Max.x, Catch::Matchers::WithinRel(24.331470f, 1e-6f));
        REQUIRE_THAT(aabb.Max.y, Catch::Matchers::WithinRel(4.8166194f, 1e-6f));
        REQUIRE_THAT(aabb.Max.z, Catch::Matchers::WithinRel(8.1856393f, 1e-6f));
    }
}
TEST_CASE("Geometry Sphere", "[Geometry]")
{
    Sphere sphere = {};
    REQUIRE(sphere.Center == glm::vec3{0.0f});
    REQUIRE(sphere.Radius == 0.0f);
    SECTION("Merge leaves original unchanged")
    {
        Sphere other = {
            .Center = {-1.0f, 2.0f, -3.0f},
            .Radius = 2.0f};
        other = sphere.Merge(other);
        REQUIRE(sphere.Center == glm::vec3{0.0f, 0.0f, 0.0f});
        REQUIRE(sphere.Radius == 0.0f);
    }
    SECTION("Merge merges when one is fully enclosed in other")
    {
        sphere.Radius = 1.0f;
        Sphere other = {
            .Center = glm::vec3{0.0f},
            .Radius = 2.0f};
        other = sphere.Merge(other);
        REQUIRE(other.Center == glm::vec3{0.0f, 0.0f, 0.0f});
        REQUIRE(other.Radius == 2.0f);

        other.Radius = 0.5f;
        other = sphere.Merge(other);
        REQUIRE(other.Center == glm::vec3{0.0f, 0.0f, 0.0f});
        REQUIRE(sphere.Radius == 1.0f);
    }
    SECTION("Merge merges when one is touching the other")
    {
        sphere.Radius = 1.0f;
        sphere.Center = glm::vec3{-1.0f, 2.0f, 0.0f};
        Sphere other = {
            .Center = glm::vec3{1.0f, 2.0f, 0.0f},
            .Radius = 1.0f};
        other = sphere.Merge(other);
        REQUIRE_THAT(other.Center.x, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(other.Center.y, Catch::Matchers::WithinRel(2.0f, 1e-6f));
        REQUIRE_THAT(other.Center.z, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(other.Radius, Catch::Matchers::WithinAbs(2.0f, 1e-6f));
    }
    SECTION("Merge merges when one is overlapping the other")
    {
        sphere.Radius = 1.0f;
        sphere.Center = glm::vec3{-0.5f, 2.0f, 0.0f};
        Sphere other = {
            .Center = glm::vec3{0.5f, 2.0f, 0.0f},
            .Radius = 1.0f};
        other = sphere.Merge(other);
        REQUIRE_THAT(other.Center.x, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(other.Center.y, Catch::Matchers::WithinRel(2.0f, 1e-6f));
        REQUIRE_THAT(other.Center.z, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(other.Radius, Catch::Matchers::WithinAbs(1.5f, 1e-6f));
    }
}
TEST_CASE("Geometry Plane", "[Geometry]")
{
    Plane plane = {};
    REQUIRE(plane.Normal == glm::vec3{0.0f, 1.0f, 0.0f});
    REQUIRE(plane.Offset == 0.0f);

    glm::vec3 normal = glm::vec3{2.0f, 3.0f, 1.0f};
    glm::vec3 point = glm::vec3{1.0f};
    plane = Plane::ByPointAndNormal(point, normal);
    
    SECTION("Is constructable from point and normal and has normalized normal")
    {
        glm::vec3 normalized = glm::normalize(normal);
        f32 planeNormalLength = glm::length(plane.Normal);
        REQUIRE_THAT(planeNormalLength, Catch::Matchers::WithinRel(1.0f, 1e-6f));
        REQUIRE_THAT(plane.Normal.x, Catch::Matchers::WithinRel(normalized.x, 1e-6f));
        REQUIRE_THAT(plane.Normal.y, Catch::Matchers::WithinRel(normalized.y, 1e-6f));
        REQUIRE_THAT(plane.Normal.z, Catch::Matchers::WithinRel(normalized.z, 1e-6f));
        REQUIRE_THAT(plane.Offset, Catch::Matchers::WithinRel(-6.0f / glm::length(normal), 1e-6f));
    }

    SECTION("SignedDistance returns correct distance on positive side of the plane")
    {
        glm::vec3 testPoint = point + plane.Normal;
        f32 distance = plane.SignedDistance(testPoint);
        REQUIRE_THAT(distance, Catch::Matchers::WithinRel(1.0f, 1e-6f));
    }
    SECTION("SignedDistance returns correct distance on negative side of the plane")
    {
        glm::vec3 testPoint = point - plane.Normal;
        f32 distance = plane.SignedDistance(testPoint);
        REQUIRE_THAT(distance, Catch::Matchers::WithinRel(-1.0f, 1e-6f));
    }
    SECTION("SignedDistance returns zero distance on the plane")
    {
        glm::vec3 testPoint = point;
        f32 distance = plane.SignedDistance(testPoint);
        REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
}

// NOLINTEND