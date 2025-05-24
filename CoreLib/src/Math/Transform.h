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
            .Scale = 1.0f / Scale
        };
    }

    /* Creates a transform, that represents `this` applied after `other`.
     * For uniform scaling, this produces a transform that is equivalent to `this->ToMatrix() * other.ToMatrix()`
     * NOTE: gives incorrect result for nonuniform scaling
     */
    Transform3d Combine(const Transform3d& other) const
    {
        return {
            .Position = Position + glm::rotate(Orientation, other.Position * Scale),
            .Orientation = Orientation * other.Orientation,
            .Scale = Scale * other.Scale
        };
    }
};
