#include "Camera.h"

#include "Input.h"

static constexpr glm::vec3 DEFAULT_POSITION		= glm::vec3(0.0f);
static const     glm::quat DEFAULT_ORIENTATION	= glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));
static constexpr f32  DEFAULT_FOV				= 45.0f;
static constexpr f32  DEFAULT_ASPECT			= 16.0f / 9.0f;
static constexpr f32  DEFAULT_NEAR				= 0.003f;
static constexpr f32  DEFAULT_FAR				= 1000.0f;

Camera::Camera()
    : m_Position(DEFAULT_POSITION), m_Orientation(DEFAULT_ORIENTATION),
    m_Aspect(DEFAULT_ASPECT),
    m_NearClipPlane(DEFAULT_NEAR), m_FarClipPlane(DEFAULT_FAR), m_FieldOfView(DEFAULT_FOV)
{
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjection();
}

Camera::Camera(const glm::vec3& position, f32 fov, f32 aspect)
    : m_Position(position), m_Orientation(DEFAULT_ORIENTATION),
    m_Aspect(aspect),
    m_NearClipPlane(DEFAULT_NEAR), m_FarClipPlane(DEFAULT_FAR), m_FieldOfView(fov)
{
    UpdateViewMatrix();
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

FrustumPlanes Camera::GetFrustumPlanes()
{
    static glm::mat4 mat = GetViewProjection();
    
    FrustumPlanes frustumPlanes = {};
    for (i32 i = 0; i < 3; i++)
    {
        for (i32 j = 0; j < 4; j++)
        {
            frustumPlanes.Planes[2 * i + 0][j] = mat[j][3] - mat[j][i];
            frustumPlanes.Planes[2 * i + 1][j] = mat[j][3] + mat[j][i];
        }
    }

    for (auto& plane : frustumPlanes.Planes)
        plane /= glm::length(glm::vec3(plane.x, plane.y, plane.z));
    
    return frustumPlanes;
}

void Camera::UpdateViewMatrix()
{
    m_ViewMatrix = glm::toMat4(glm::inverse(m_Orientation)) * glm::translate(glm::mat4(1.0f), -m_Position);
}

void Camera::UpdateProjectionMatrix()
{
    m_Aspect = f32(m_ViewportWidth) / f32(m_ViewportHeight);
    m_ProjectionMatrix = glm::perspectiveRH_ZO(m_FieldOfView, m_Aspect, m_FarClipPlane, m_NearClipPlane);
    m_ProjectionMatrix[1][1] *= -1.0f;
}

void Camera::UpdateViewProjection()
{
    m_ViewProjection = m_ProjectionMatrix * m_ViewMatrix;
    m_ViewProjectionInverse = glm::inverse(m_ViewProjection);
}

static constexpr f32 DEFAULT_TRANSLATION_SPEED	         = 0.1f;
static constexpr f32 DEFAULT_TRANSLATION_SPEED_FPS	     = 1.0f;
static constexpr f32 DEFAULT_TRANSLATION_SPEED_BOOST_FPS = 2.0f;
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
