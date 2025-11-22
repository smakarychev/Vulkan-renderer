#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "types.h"
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

        defaultTransform.Position = glm::vec3{-12.0f, 21.0f, 11.0f};
        defaultTransform.Scale = glm::vec3{3.1f, -0.2f, 10.3f};
        defaultTransform.Orientation = glm::angleAxis(glm::radians(42.0f), glm::normalize(glm::vec3{4.0f, 1.0f, 6.0f}));
        mat = defaultTransform.ToMatrix();

        glm::mat4 mat2 =
            glm::translate(glm::mat4{1.0f}, defaultTransform.Position) *
            glm::rotate(glm::mat4{1.0f}, glm::radians(42.0f), glm::normalize(glm::vec3{4.0f, 1.0f, 6.0f})) *
            glm::scale(glm::mat4{1.0f}, defaultTransform.Scale);
        for (u32 i = 0; i < 4; i++)
            for (u32 j = 0; j < 4; j++)
                REQUIRE_THAT(mat[i][j], Catch::Matchers::WithinRel(mat2[i][j], 1e-5f));        
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
    SECTION("Can combine with other transform")
    {
        defaultTransform.Position = glm::vec3{-1.0f, 2.0f, 5.0f};
        defaultTransform.Scale = glm::vec3{2};
        defaultTransform.Orientation = glm::angleAxis(glm::radians(44.0f), glm::normalize(glm::vec3{3.0f, -1.0f, 7.0f}));

        const Transform3d other = {
            .Position = glm::vec3{1.2f, -5.0f, 41.1f},
            .Orientation = glm::angleAxis(glm::radians(41.0f), glm::normalize(glm::vec3{1.0f, -2.0f, 3.0f})),
            .Scale = glm::vec3{0.1f},
        };

        const Transform3d combined = defaultTransform.Combine(other);
        const glm::mat4 combinedAsMatrix = defaultTransform.ToMatrix() * other.ToMatrix();
        const glm::mat4 combinedToMatrix = combined.ToMatrix();

        REQUIRE_THAT(glm::length(combined.Orientation), Catch::Matchers::WithinRel(1.0f, 1e-5f));     
        for (u32 i = 0; i < 4; i++)
            for (u32 j = 0; j < 4; j++)
                REQUIRE_THAT(combinedToMatrix[i][j], Catch::Matchers::WithinRel(combinedAsMatrix[i][j], 1e-5f));        
    }
}

// NOLINTEND