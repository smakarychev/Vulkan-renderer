#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform3d
{
    glm::vec3 Position{0.0f};
    glm::quat Orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f};

    glm::mat4 ToMatrix() const
    {
        glm::mat4 transform = glm::mat4(glm::mat3_cast(Orientation));
        transform[0] *= Scale.x;
        transform[1] *= Scale.y;
        transform[2] *= Scale.z;

        transform[3] = glm::vec4(Position, 1.0f);

        return transform;
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
            .Position = Position + Orientation * other.Position * Scale,
            .Orientation = Orientation * other.Orientation,
            .Scale = Scale * other.Scale
        };
    }
};
