#include "Camera.h"

#include <array>

#include "core.h"
#include "Input.h"

#include <vector>

static constexpr glm::vec3 DEFAULT_POSITION     = glm::vec3(0.0f);
static const     glm::quat DEFAULT_ORIENTATION	= glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));
static constexpr f32  DEFAULT_FOV               = glm::radians(60.0f);
static constexpr f32  DEFAULT_ASPECT            = 16.0f / 9.0f;
static constexpr f32  DEFAULT_NEAR              = 0.1f;
static constexpr f32  DEFAULT_FAR               = std::numeric_limits<f32>::infinity();

Camera::Camera(CameraType type)
    :
    m_CameraType(type),
    m_Position(DEFAULT_POSITION), m_Orientation(DEFAULT_ORIENTATION),
    m_Aspect(DEFAULT_ASPECT),
    m_NearClipPlane(DEFAULT_NEAR), m_FarClipPlane(DEFAULT_FAR), m_FieldOfView(DEFAULT_FOV)
{
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjection();
}

Camera::Camera(CameraType type, const glm::vec3& position, f32 fov, f32 aspect)
    :
    m_CameraType(type),
    m_Position(position), m_Orientation(DEFAULT_ORIENTATION),
    m_Aspect(aspect),
    m_NearClipPlane(DEFAULT_NEAR), m_FarClipPlane(DEFAULT_FAR), m_FieldOfView(fov)
{
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjection();
}

Camera Camera::Perspective(const PerspectiveCameraCreateInfo& info)
{
    Camera camera{CameraType::Perspective};
    camera.m_Position = info.BaseInfo.Position;
    camera.m_Orientation = info.BaseInfo.Orientation;
    camera.m_NearClipPlane = info.BaseInfo.Near;
    camera.m_FarClipPlane = info.BaseInfo.Far;
    camera.m_ViewportWidth = info.BaseInfo.ViewportWidth;
    camera.m_ViewportHeight = info.BaseInfo.ViewportHeight;
    camera.m_FlipY = info.BaseInfo.FlipY;

    camera.m_FieldOfView = info.Fov;
    camera.m_Aspect = (f32)camera.m_ViewportWidth / (f32)camera.m_ViewportHeight;
    camera.UpdateViewMatrix();
    camera.UpdateProjectionMatrix();
    camera.UpdateViewProjection();

    return camera;
}

Camera Camera::Orthographic(const OrthographicCameraCreateInfo& info)
{
    Camera camera{CameraType::Orthographic};
    camera.m_Position = info.BaseInfo.Position;
    camera.m_Orientation = info.BaseInfo.Orientation;
    camera.m_NearClipPlane = info.BaseInfo.Near;
    camera.m_FarClipPlane = info.BaseInfo.Far;
    camera.m_ViewportWidth = info.BaseInfo.ViewportWidth;
    camera.m_ViewportHeight = info.BaseInfo.ViewportHeight;
    camera.m_FlipY = info.BaseInfo.FlipY;

    f32 halfHeight = (info.Top - info.Bottom) * 0.5f;
    f32 halfWidth = (info.Right - info.Left) * 0.5f;
    camera.m_FieldOfView = 2.0f * std::atan(halfHeight / std::abs(info.BaseInfo.Near));
    camera.m_Aspect = halfWidth / halfHeight;

    camera.UpdateViewMatrix();
    camera.m_ProjectionMatrix = glm::orthoRH_ZO(info.Left, info.Right, info.Bottom, info.Top,
        info.BaseInfo.Far, info.BaseInfo.Near);
    if (camera.m_FlipY)
        camera.FlipProjectionVertically();
    camera.UpdateViewProjection();

    return camera;
}

Camera Camera::EnvironmentCapture(const glm::vec3& position, u32 viewportSize, u32 faceIndex)
{
    static const std::vector DIRECTIONS = {
        glm::vec3{ 1.0, 0.0, 0.0},
        glm::vec3{-1.0, 0.0, 0.0},
        glm::vec3{ 0.0, 1.0, 0.0},
        glm::vec3{ 0.0,-1.0, 0.0},
        glm::vec3{ 0.0, 0.0, 1.0},
        glm::vec3{ 0.0, 0.0,-1.0},
    };
    static const std::vector UP_VECTORS = {
        glm::vec3{0.0,-1.0, 0.0},
        glm::vec3{0.0,-1.0, 0.0},
        glm::vec3{0.0, 0.0, 1.0},
        glm::vec3{0.0, 0.0,-1.0},
        glm::vec3{0.0,-1.0, 0.0},
        glm::vec3{0.0,-1.0, 0.0},
    };
    
    /* this should not matter at all */
    static constexpr f32 NEAR = 0.1f;
    static constexpr f32 FAR = 1.0f;
                
    return Perspective({
        .BaseInfo = CameraCreateInfo{
            .Position = position,
            .Orientation = glm::normalize(glm::quatLookAt(DIRECTIONS[faceIndex], UP_VECTORS[faceIndex])),
            .Near = NEAR,
            .Far = FAR,
            .ViewportWidth = viewportSize,
            .ViewportHeight = viewportSize,
            .FlipY = false
        },
        .Fov = glm::radians(90.0f)});
}

void Camera::SetType(CameraType type)
{
    m_CameraType = type;
    UpdateProjectionMatrix();
    UpdateViewProjection();
}

void Camera::SetPosition(const glm::vec3& position)
{
    if (m_Position != position)
    {
        m_Position = position;
        UpdateViewMatrix();
        UpdateViewProjection();
    }
}

void Camera::SetOrientation(const glm::quat& orientation)
{
    if (m_Orientation != orientation)
    {
        m_Orientation = orientation;
        UpdateViewMatrix();
        UpdateViewProjection();
    }
}

void Camera::SetView(const glm::mat4& view)
{
    m_ViewMatrix = view;
    UpdateViewProjection();
}

void Camera::SetProjection(const glm::mat4& projection)
{
    m_ProjectionMatrix = projection;
    UpdateViewProjection();
}

void Camera::SetViewport(u32 width, u32 height)
{
    m_ViewportWidth = width;
    m_ViewportHeight = height;
    UpdateProjectionMatrix();
    UpdateViewProjection();
}

glm::vec3 Camera::GetForward() const
{
    return glm::rotate(m_Orientation, glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::GetUp() const
{
    return glm::rotate(m_Orientation, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::GetRight() const
{
    return glm::rotate(m_Orientation, glm::vec3(1.0f, 0.0f, 0.0f));
}

FrustumPlanes Camera::GetFrustumPlanes(f32 maxDistance) const
{
    const glm::mat4& mat = GetProjection();

    FrustumPlanes frustumPlanes = {};
    frustumPlanes.Near = m_NearClipPlane;
    frustumPlanes.Far  = std::min(m_FarClipPlane, maxDistance);
    
    switch (m_CameraType)
    {
    case CameraType::Perspective:
    {
        f32 rightLengthInverse = 1.0f / std::sqrt(1.0f + mat[0][0] * mat[0][0]);
        f32 topLengthInverse = 1.0f / std::sqrt(1.0f + mat[1][1] * mat[1][1]);

        frustumPlanes.RightX = mat[0][0] * rightLengthInverse;
        frustumPlanes.RightZ = rightLengthInverse;
        frustumPlanes.TopY = mat[1][1] * topLengthInverse;
        frustumPlanes.TopZ = topLengthInverse;
        break;
    }
    case CameraType::Orthographic:
    {
        frustumPlanes.RightX = mat[0][0];
        frustumPlanes.RightZ = 0.0f;
        frustumPlanes.TopY = mat[1][1];
        frustumPlanes.TopZ = 0.0f;
        break;
    }
    }
    
    return frustumPlanes;
}

FrustumCorners Camera::GetFrustumCorners() const
{
    return GetFrustumCorners(m_NearClipPlane, m_FarClipPlane);
}

FrustumCorners Camera::GetFrustumCorners(f32 maxDistance) const
{
    return GetFrustumCorners(m_NearClipPlane, maxDistance);
}

FrustumCorners Camera::GetFrustumCorners(f32 minDistance, f32 maxDistance) const
{
    f32 near = std::max(m_NearClipPlane, minDistance);
    f32 far = std::min(m_FarClipPlane, maxDistance);
    
    glm::vec3 nearCenter = GetForward() * near;
    glm::vec3 farCenter = GetForward() * far;

    f32 tanFov = std::tan(m_FieldOfView * 0.5f);
    f32 nearHeight = tanFov * near;
    f32 nearWidth = nearHeight * m_Aspect;
    f32 farHeight = tanFov * far;
    f32 farWidth = farHeight * m_Aspect;

    FrustumCorners p = {};

    p[0] = m_Position + nearCenter - GetRight() * nearWidth - GetUp() * nearHeight;
    p[1] = m_Position + nearCenter - GetRight() * nearWidth + GetUp() * nearHeight;
    p[2] = m_Position + nearCenter + GetRight() * nearWidth + GetUp() * nearHeight;
    p[3] = m_Position + nearCenter + GetRight() * nearWidth - GetUp() * nearHeight;
    p[4] = m_Position + farCenter  - GetRight() * farWidth  - GetUp() * farHeight;
    p[5] = m_Position + farCenter  - GetRight() * farWidth  + GetUp() * farHeight;
    p[6] = m_Position + farCenter  + GetRight() * farWidth  + GetUp() * farHeight;
    p[7] = m_Position + farCenter  + GetRight() * farWidth  - GetUp() * farHeight;

    return p;   
}

ProjectionData Camera::GetProjectionData() const
{
    const glm::mat4& mat = GetProjection();
    
    return {mat[0][0], mat[1][1], mat[3][0], mat[3][1]};
}

Plane Camera::GetNearViewPlane() const
{
    return Plane{
        .Normal = GetForward(),
        .Offset = -glm::dot(GetForward(), GetPosition())};
}

void Camera::UpdateViewMatrix()
{
    m_ViewMatrix = glm::toMat4(glm::inverse(m_Orientation)) * glm::translate(glm::mat4(1.0f), -m_Position);
}

namespace
{
    glm::mat4 infiniteReverseDepthProjection(f32 fov, f32 aspect, f32 near)
    {
        const f32 tanHalfFov = tan(fov * 0.5f);

        glm::mat4 projection(0.0f);
        projection[0][0] = 1.0f / (aspect * tanHalfFov);
        projection[1][1] = 1.0f / (tanHalfFov);
        projection[2][3] = -1.0f;
        projection[3][2] = near;
        
        return projection;
    }
}

void Camera::UpdateProjectionMatrix()
{
    m_Aspect = (f32)m_ViewportWidth / (f32)m_ViewportHeight;

    switch (m_CameraType)
    {
    case CameraType::Perspective:
    {
        m_ProjectionMatrix = infiniteReverseDepthProjection(m_FieldOfView, m_Aspect, m_NearClipPlane);
        break;
    }
    case CameraType::Orthographic:
    {
        f32 orthoHeight = 2.0f * std::tan(m_FieldOfView * 0.5f) * (m_NearClipPlane + 1.0f);
        f32 orthoWidth = orthoHeight * m_Aspect;
        f32 x = orthoWidth * 0.5f;
        f32 y = orthoHeight * 0.5f;
        m_ProjectionMatrix = glm::orthoRH_ZO(-x, x, -y, y, m_FarClipPlane, m_NearClipPlane);
        break;
    }
    }

    if (m_FlipY)
        FlipProjectionVertically();
}

void Camera::UpdateViewProjection()
{
    m_ViewProjection = m_ProjectionMatrix * m_ViewMatrix;
    m_ViewProjectionInverse = glm::inverse(m_ViewProjection);
}

void Camera::FlipProjectionVertically()
{
    m_ProjectionMatrix[1][1] *= -1.0f;
    m_ProjectionMatrix[3][1] *= -1.0f;
}

static constexpr f32 DEFAULT_TRANSLATION_SPEED	         = 0.1f;
static constexpr f32 DEFAULT_TRANSLATION_SPEED_FPS	     = 1.0f;
static constexpr f32 DEFAULT_TRANSLATION_SPEED_BOOST_FPS = 5.0f;
static constexpr f32 DEFAULT_ROTATION_SPEED		         = 0.5f;
static constexpr f32 DEFAULT_E_YAW				         = 0.0f;
static constexpr f32 DEFAULT_E_PITCH			         = glm::radians(-15.0f);
static constexpr f32 DEFAULT_DISTANCE		             = 3.0f;
static constexpr glm::vec3 DEFAULT_FOCAL_POINT           = glm::vec3(0.0);

CameraController::CameraController(const std::shared_ptr<Camera>& camera)
    : m_Camera(camera),
    m_TranslationSpeed(DEFAULT_TRANSLATION_SPEED), m_RotationSpeed(DEFAULT_ROTATION_SPEED),
    m_TranslationSpeedFPS(DEFAULT_TRANSLATION_SPEED_FPS), m_TranslationSpeedBoostFPS(DEFAULT_TRANSLATION_SPEED_BOOST_FPS),
    m_Yaw(DEFAULT_E_YAW), m_Pitch(DEFAULT_E_PITCH), m_FocalPoint(DEFAULT_FOCAL_POINT),
    m_Distance(DEFAULT_DISTANCE)
{
    m_MouseCoordinates = Input::MousePosition();
    m_Camera->SetOrientation(glm::quat(glm::vec3(m_Pitch, m_Yaw, 0.0f)));
    m_Camera->SetPosition(m_FocalPoint - m_Distance * m_Camera->GetForward());
    m_Camera->UpdateViewMatrix();
}

void CameraController::OnUpdate(f32 dt)
{
    // combined controls: while holding right mouse button, the controller behaves as fps controller,
    // if not holding rmb, it behaves as orbit controller
    if (!Input::GetKey(Key::LeftAlt) && Input::GetMouseButton(Mouse::ButtonRight))
        FPSOnUpdate(dt);
    else
        OrbitOnUpdate(dt);

    m_MouseCoordinates = Input::MousePosition();
}

void CameraController::FPSOnUpdate(f32 dt)
{
    f32 prevMouseX = m_MouseCoordinates.x;
    f32 prevMouseY = m_MouseCoordinates.y;
    m_MouseCoordinates.x = Input::MousePosition().x;
    m_MouseCoordinates.y = Input::MousePosition().y;

    f32 xOffset =   prevMouseX - m_MouseCoordinates.x;
    f32 yOffset = -(prevMouseY - m_MouseCoordinates.y);

    xOffset *= m_RotationSpeed * dt;
    yOffset *= m_RotationSpeed * dt;

    m_Yaw += xOffset;
    m_Pitch += yOffset;

    if (m_Pitch > glm::radians(89.99f)) m_Pitch = glm::radians(89.99f);
    else if (m_Pitch < glm::radians(-89.99f)) m_Pitch = glm::radians(-89.99f);

    glm::quat newOrientation = glm::quat(glm::vec3(m_Pitch, m_Yaw, 0.0f));
    m_Camera->SetOrientation(newOrientation);

    glm::vec3 velocityVector = {};
    if (Input::GetKey(Key::W))
        velocityVector += m_Camera->GetForward();
    if (Input::GetKey(Key::S))
        velocityVector -= m_Camera->GetForward();
    if (Input::GetKey(Key::A))
        velocityVector -= m_Camera->GetRight();
    if (Input::GetKey(Key::D))
        velocityVector += m_Camera->GetRight();

    f32 speed = m_TranslationSpeedFPS * dt;
    if (Input::GetKey(Key::LeftShift))
        speed *= m_TranslationSpeedBoostFPS;

    if (glm::length2(velocityVector) > 0.0f)
        velocityVector = speed * glm::normalize(velocityVector);

    m_Camera->SetPosition(m_Camera->GetPosition() + velocityVector);
    m_FocalPoint = m_Camera->GetPosition() + m_Distance * m_Camera->GetForward(); 
    
    m_Camera->UpdateViewMatrix();
    m_Camera->UpdateViewProjection();
}

void CameraController::OrbitOnUpdate(f32 dt)
{
    if (Input::GetKey(Key::LeftAlt))
    {
        f32 prevMouseX = m_MouseCoordinates.x;
        f32 prevMouseY = m_MouseCoordinates.y;
        m_MouseCoordinates = Input::MousePosition();
        f32 xOffset =   m_MouseCoordinates.x - prevMouseX;
        f32 yOffset = -(m_MouseCoordinates.y - prevMouseY);

        if (Input::GetMouseButton(Mouse::ButtonRight))
        {
            f32 deltaDistance = yOffset * ZoomSpeed() * dt;
            m_Distance += deltaDistance;
            if (m_Distance < 0.0f)
                m_Distance = 1;

            glm::vec3 newCameraPosition = m_FocalPoint - m_Distance * m_Camera->GetForward();
            m_Camera->SetPosition(newCameraPosition);
            m_Camera->UpdateViewMatrix();
            m_Camera->UpdateViewProjection();
        }
        
        if (Input::GetKey(Key::LeftShift))
        {
            if (Input::GetMouseButton(Mouse::ButtonLeft))
            {
                xOffset *= -m_TranslationSpeed * dt * m_Distance;
                yOffset *= m_TranslationSpeed * dt * m_Distance;
                m_FocalPoint += xOffset * m_Camera->GetRight() + yOffset * m_Camera->GetUp();
                glm::vec3 newCameraPosition = m_FocalPoint - m_Distance * m_Camera->GetForward();
                m_Camera->SetPosition(newCameraPosition);
                m_Camera->UpdateViewMatrix();
                m_Camera->UpdateViewProjection();
            }
        }
        else
        {
            if (Input::GetMouseButton(Mouse::ButtonLeft))
            {
                xOffset *=  m_RotationSpeed * dt;
                yOffset *= -m_RotationSpeed * dt;
                f32 yawSign = m_Camera->GetUp().y < 0 ? 1.0f : -1.0f;
                m_Yaw += xOffset * yawSign * m_RotationSpeed;
                m_Pitch += yOffset * m_RotationSpeed;

                glm::quat newOrientation = glm::quat(glm::vec3(m_Pitch, m_Yaw, 0.0f));
                m_Camera->SetOrientation(newOrientation);
                glm::vec3 newPosition = m_FocalPoint - m_Distance * m_Camera->GetForward();
                m_Camera->SetPosition(newPosition);

                m_Camera->UpdateViewMatrix();
                m_Camera->UpdateViewProjection();
            }
        }
            
    }	
}

f32 CameraController::ZoomSpeed()
{
    f32 dist = std::max(m_Distance * 0.2f, 0.0f);
    f32 speed = dist * dist;
    speed = std::min(speed, 100.0f);
    return speed;
}
