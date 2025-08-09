#pragma once

#include <memory>

#include "types.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Math/Geometry.h"

struct FrustumPlanes
{
    f32 TopY;
    f32 TopZ;
    f32 RightX;
    f32 RightZ;
    f32 Near;
    f32 Far;
};

using FrustumCorners = std::array<glm::vec3, 8>;

struct ProjectionData
{
    f32 Width;
    f32 Height;

    f32 BiasX;
    f32 BiasY;
};

enum class CameraType {Perspective, Orthographic};

struct CameraCreateInfo
{
    glm::vec3 Position{0.0f};
    glm::quat Orientation{glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f))};
    f32 Near{0.1f};
    f32 Far{5000.0f};

    u32 ViewportWidth{1600};
    u32 ViewportHeight{900};

    bool FlipY{true};
};

struct PerspectiveCameraCreateInfo
{
    CameraCreateInfo BaseInfo{};
    f32 Fov{glm::radians(45.0f)};
};
struct OrthographicCameraCreateInfo
{
    CameraCreateInfo BaseInfo{};
    f32 Left{-0.5f};
    f32 Right{0.5f};
    f32 Bottom{-0.5f};
    f32 Top{0.5f};
};

class Camera
{
    friend class CameraController;
public:
    Camera(CameraType type);
    Camera(CameraType type, const glm::vec3& position, f32 fov, f32 aspect);

    static Camera Perspective(const PerspectiveCameraCreateInfo& info);
    static Camera Orthographic(const OrthographicCameraCreateInfo& info);
    static Camera EnvironmentCapture(const glm::vec3& position, u32 viewportSize, u32 faceIndex);

    void SetType(CameraType type);
    CameraType GetType() const { return m_CameraType; }
    
    void SetPosition(const glm::vec3& position);
    void SetOrientation(const glm::quat& orientation);

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::quat& GetOrientation() const { return m_Orientation; }
    
    const glm::mat4& GetViewProjection() const { return m_ViewProjection; }
    const glm::mat4& GetViewProjectionInverse() const { return m_ViewProjectionInverse; }
    const glm::mat4& GetView() const { return m_ViewMatrix; }
    const glm::mat4& GetProjection() const { return m_ProjectionMatrix; }

    void SetView(const glm::mat4& view);
    void SetProjection(const glm::mat4& projection);

    f32 GetNear() const { return m_NearClipPlane; }
    f32 GetFar() const { return m_FarClipPlane; }

    void SetViewport(u32 width, u32 height);
    u32 GetViewportWidth() const { return m_ViewportWidth; }
    u32 GetViewportHeight() const { return m_ViewportHeight; }

    glm::vec3 GetForward() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;

    FrustumPlanes GetFrustumPlanes(f32 maxDistance) const;
    FrustumCorners GetFrustumCorners() const;
    FrustumCorners GetFrustumCorners(f32 maxDistance) const;
    FrustumCorners GetFrustumCorners(f32 minDistance, f32 maxDistance) const;
    ProjectionData GetProjectionData() const;

    /* return near camera plane with normal equal to the `Forward` vector */
    Plane GetNearViewPlane() const;
private:
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateViewProjection();
    void FlipProjectionVertically();
private:
    CameraType m_CameraType{};
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

    bool m_FlipY{true};
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