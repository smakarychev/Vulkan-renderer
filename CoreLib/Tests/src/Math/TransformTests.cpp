#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "Math/Transform.h"

// NOLINTBEGIN

TEST_CASE("Geometry Transform3d", "[Transform]")
{
    Transform3d defaultTransform = {};
    SECTION("Has 0 offset by default")
    {
        REQUIRE(defaultTransform.Position == glm::vec3{0.0f});
    }
    SECTION("Has z-facing orientation by default")
    {
        REQUIRE(defaultTransform.Orientation == glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
    }
    SECTION("Has uniform scale of 1 by default")
    {
        REQUIRE(defaultTransform.Scale == glm::vec3{1.0f});
    }
    SECTION("Can be transformed to identity matrix by default")
    {
        glm::mat4 mat = defaultTransform.ToMatrix();
        REQUIRE(mat == glm::mat4{1.0f});
    }
    SECTION("Transforms to matrix correctly")
    {
        defaultTransform.Position = glm::vec3{-1.0f, 2.0f, 5.0f};
        defaultTransform.Scale = glm::vec3{0.1f, 0.2f, 0.3f};
        defaultTransform.Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
        glm::mat4 mat = defaultTransform.ToMatrix();

        REQUIRE(mat[3][0] == -1.0f);
        REQUIRE(mat[3][1] == 2.0f);
        REQUIRE(mat[3][2] == 5.0f);

        /* scale + rotation */
        REQUIRE_THAT(mat[0][0], Catch::Matchers::WithinRel(0.1f, 1e-6f));
        REQUIRE_THAT(mat[1][2], Catch::Matchers::WithinRel(0.2f, 1e-6f));
        REQUIRE_THAT(mat[2][1], Catch::Matchers::WithinRel(-0.3f, 1e-6f));

        /* rest should stay 0*/
        REQUIRE_THAT(mat[0][1], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(mat[0][2], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(mat[1][0], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(mat[1][1], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(mat[2][0], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(mat[2][2], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
    SECTION("Inverse inverses")
    {
        defaultTransform.Position = glm::vec3{-1.0f, 2.0f, 5.0f};
        defaultTransform.Scale = glm::vec3{0.1f, 0.2f, 0.3f};
        defaultTransform.Orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
        const Transform3d inverse = defaultTransform.Inverse();

        const glm::mat4 mat = defaultTransform.ToMatrix() * inverse.ToMatrix();
        REQUIRE_THAT(glm::determinant(mat), Catch::Matchers::WithinRel(1.0f, 1e-6f));
    }
}

// NOLINTEND