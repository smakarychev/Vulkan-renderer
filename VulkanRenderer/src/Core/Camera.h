﻿#pragma once

#include <memory>

#include "types.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera
{
    friend class CameraController;
public:
    Camera();
    Camera(const glm::vec3& position, f32 fov, f32 aspect);

    void SetPosition(const glm::vec3& position);
    void SetOrientation(const glm::quat& orientation);

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::quat& GetOrientation() const { return m_Orientation; }
    
    glm::mat4& GetViewProjection() { return m_ViewProjection; }
    glm::mat4& GetView() { return m_ViewMatrix; }
    glm::mat4& GetProjection() { return m_ProjectionMatrix; }

    void SetViewport(u32 width, u32 height);
    u32 GetViewportWidth() const { return m_ViewportWidth; }
    u32 GetViewportHeight() const { return m_ViewportHeight; }

    glm::vec3 GetForward() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;
    
private:
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjection();
private:
    glm::vec3 m_Position;
    glm::quat m_Orientation;

    glm::mat4 m_ViewProjection;
    glm::mat4 m_ViewProjectionInverse;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;

    f32 m_Aspect;
    f32 m_NearClipPlane, m_FarClipPlane;
    f32 m_FieldOfView;
    u32 m_ViewportWidth = 1600, m_ViewportHeight = 900;
};

class CameraController
{
public:
    CameraController(const std::shared_ptr<Camera>& camera);

    void OnUpdate(f32 dt);
private:
    void FPSOnUpdate(f32 dt);
    void OrbitOnUpdate(f32 dt);
    f32 ZoomSpeed();
private:
    std::shared_ptr<Camera> m_Camera;

    f32 m_TranslationSpeed;
    f32 m_RotationSpeed;

    f32 m_TranslationSpeedFPS;
    f32 m_TranslationSpeedBoostFPS;

    f32 m_Yaw, m_Pitch;

    glm::vec2 m_MouseCoordinates;

    glm::vec3 m_FocalPoint;
    f32 m_Distance;
};