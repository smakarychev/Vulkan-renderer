#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct Transform3d
{
    glm::vec3 Position{0.0f};
    glm::quat Orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f};

    glm::mat4 ToMatrix() const
    {
        return
            glm::translate(glm::mat4{1.0f}, Position) * 
            glm::toMat4(Orientation) * 
            glm::scale(glm::mat4{1.0f}, Scale);
    }

    Transform3d Inverse() const
    {
        return {
            .Position = -Position,
            .Orientation = glm::inverse(Orientation),
            .Scale = 1.0f / Scale};
    }
};
