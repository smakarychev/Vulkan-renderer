#include "utils.h"

#include <algorithm>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/matrix_access.hpp>

#include "Core/core.h"

namespace
{

    assetLib::BoundingSphere sphereBy2Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 center = (boundaryPoints[0] + boundaryPoints[1]) * 0.5f;

        f32 radius = glm::length(boundaryPoints[0] - boundaryPoints[1]) * 0.5f;

        return {.Center = center, .Radius = radius};
    }
    
    assetLib::BoundingSphere sphereBy3Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 a = boundaryPoints[0] - boundaryPoints[2];
        glm::vec3 b = boundaryPoints[1] - boundaryPoints[2];

        glm::vec3 cross = glm::cross(a, b);

        glm::vec3 center = boundaryPoints[2] + glm::cross(glm::length2(a) * b - glm::length2(b) * a, cross)
            / (2.0f * glm::length2(cross));

        f32 radius = glm::distance(boundaryPoints[0], center);

        return {.Center = center, .Radius = radius};
    }

    assetLib::BoundingSphere sphereBy4Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 a = boundaryPoints[1] - boundaryPoints[0];
        glm::vec3 b = boundaryPoints[2] - boundaryPoints[0];
        glm::vec3 c = boundaryPoints[3] - boundaryPoints[0];

        glm::vec3 squares = {
            glm::length2(a),
            glm::length2(b),
            glm::length2(c),
        };
        
        glm::vec3 center = boundaryPoints[0] +
            (squares[0] * glm::cross(b, c) + squares[1] * glm::cross(c, a) + squares[2] * glm::cross(a, b)) /
            (2.0f * glm::dot(a, glm::cross(b, c)));

        f32 radius = glm::distance(center, boundaryPoints[0]);
        
        return {.Center = center, .Radius = radius};
    }
    
    assetLib::BoundingSphere sphereByPoints(const std::vector<glm::vec3>& boundaryPoints)
    {
        if (boundaryPoints.empty())
            return {.Center = glm::vec3(0.0f), .Radius = 0.0f};
        if (boundaryPoints.size() == 1)
            return {.Center = boundaryPoints.front(), .Radius = 0.0f};
        if (boundaryPoints.size() == 2)
            return sphereBy2Points(boundaryPoints);
        if (boundaryPoints.size() == 3)
            return sphereBy3Points(boundaryPoints);
       return sphereBy4Points(boundaryPoints);
    }

    bool isInSphere(const glm::vec3& point, const assetLib::BoundingSphere& sphere)
    {
        return glm::distance(point, sphere.Center) - sphere.Radius < 1e-7f;
    }
}

namespace utils
{
    assetLib::BoundingSphere welzlSphere(std::vector<glm::vec3> points)
    {
        static std::random_device device;
        static std::mt19937 generator{device()};
        static std::uniform_real_distribution<f32> f32Distribution(0, 1);
        
        std::ranges::shuffle(points, generator);

        std::vector<assetLib::BoundingSphere> results;
        
        enum class RecursionBranch {First, Second};
        struct Frame
        {
            u32 PointCount;
            std::optional<glm::vec3> TestPoint;
            std::vector<glm::vec3> BoundaryPoints;
            RecursionBranch Branch;

            Frame(u32 pointCount, const std::vector<glm::vec3>& boundary, RecursionBranch branch)
                : PointCount(pointCount), BoundaryPoints(boundary), Branch(branch) {}
            Frame(u32 pointCount, const std::vector<glm::vec3>& boundary, const glm::vec3& testPoint, RecursionBranch branch)
                : PointCount(pointCount), TestPoint(testPoint), BoundaryPoints(boundary), Branch(branch) {}
        };

        std::vector<Frame> frameStack;
        frameStack.emplace_back((u32)points.size(), std::vector<glm::vec3>{}, RecursionBranch::First);

        while (!frameStack.empty())
        {
            auto [count, testPoint, boundary, branch] = frameStack.back(); frameStack.pop_back();
            if (branch == RecursionBranch::First)
            {
                if (count == 0 || boundary.size() == 4)
                {
                    results.push_back(sphereByPoints(boundary));
                }
                else
                {
                    u32 random = u32(f32Distribution(generator) * (f32)count);
                    glm::vec3 point = points[random];
                    std::swap(points[random], points[count - 1]);

                    frameStack.emplace_back(count, boundary, point, RecursionBranch::Second);
                    frameStack.emplace_back(count - 1, boundary, RecursionBranch::First);
                }
            }
            else
            {
                assetLib::BoundingSphere testSphere = results.back(); results.pop_back();
                if (isInSphere(*testPoint, testSphere))
                {
                    results.push_back(testSphere);
                }
                else
                {
                    boundary.push_back(*testPoint);
                    frameStack.emplace_back(count - 1, boundary, RecursionBranch::First);
                }
            }
        }
        
        return results.back();
    }

}
