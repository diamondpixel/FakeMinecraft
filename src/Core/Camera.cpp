#include "Camera.h"
#include <cmath>

/**
 * @file Camera.cpp
 * @brief Implementation of viewport navigation and vector math.
 */

namespace {
    /// Unit vector pointing down the negative Z-axis.
    const glm::vec3 INITIAL_FRONT(0.0f, 0.0f, -1.0f);

    /**
     * @brief Normalizes an angle into the [0, max) range.
     * Uses fmod for high-performance wrapping without conditional branching where possible.
     */
    [[gnu::always_inline]]
    float wrapAngle(float angle, float max) {
        angle = std::fmod(angle, max);
        return (angle < 0.0f) ? angle + max : angle;
    }

    /**
     * @brief Fast scalar clamping.
     */
    [[gnu::always_inline]]
    float clamp(float value, float min, float max) {
        return (value < min) ? min : ((value > max) ? max : value);
    }
}

/**
 * @brief Default constructor. Updates internal vectors immediately to ensure a valid view matrix.
 */
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Position(position)
    , Front(INITIAL_FRONT)
    , Right(1.0f, 0.0f, 0.0f)
    , Up(0.0f, 1.0f, 0.0f)
    , WorldUp(up)
    , Yaw(yaw)
    , Pitch(pitch)
    , MouseSensitivity(SENSITIVITY)
{
    updateCameraVectors();
}

/**
 * @brief Scalar constructor for low-level initialization.
 */
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
    : Position(posX, posY, posZ)
    , Front(INITIAL_FRONT)
    , Right(1.0f, 0.0f, 0.0f)
    , Up(0.0f, 1.0f, 0.0f)
    , WorldUp(upX, upY, upZ)
    , Yaw(yaw)
    , Pitch(pitch)
    , MouseSensitivity(SENSITIVITY)
{
    updateCameraVectors();
}

/**
 * @brief Returns the view matrix calculated using the LookAt algorithm.
 * 
 * Target position is calculated as current position + the look-direction vector.
 * The Up vector used is the relative camera-up, not world-up.
 */
glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

/**
 * @brief Translates the camera position along its local axes.
 * 
 * Movement is relative to the camera's current rotation (e.g., FORWARD moves along the Front vector).
 * Vertical movement (UP/DOWN) is hard-coded to global Y to match typical voxel game behavior.
 */
void Camera::processKeyboard(Camera_Movement direction, float deltaTime) {
    const float velocity = deltaTime;

    // Use a switch to allow compiler optimization into a jump table.
    switch (direction) {
        case FORWARD:
            Position += Front * velocity;
            break;
        case BACKWARD:
            Position -= Front * velocity;
            break;
        case LEFT:
            Position -= Right * velocity;
            break;
        case RIGHT:
            Position += Right * velocity;
            break;
        case UP:
            Position.y += velocity;
            break;
        case DOWN:
            Position.y -= velocity;
            break;
    }
}

/**
 * @brief Updates Euler angles from mouse input and recalculates orientation vectors.
 * 
 * Applies sensitivity and enforces Pitch constraints to prevent the player from looking 
 * past the zenith/nadir (which causes gimbal lock in the view matrix).
 */
void Camera::processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (constrainPitch) [[likely]] {
        Pitch = clamp(Pitch, PITCH_MIN, PITCH_MAX);
        // Modular wrap for yaw to keep values within float precision limits
        Yaw = wrapAngle(Yaw, YAW_WRAP);
    }

    updateCameraVectors();
}

/**
 * @brief Performs the spherical-to-cartesian coordinate conversion.
 * 
 * 1. Radian conversion (cached to avoid redundant math).
 * 2. Front vector calculation:
 *    x = cos(yaw) * cos(pitch)
 *    y = sin(pitch)
 *    z = sin(yaw) * cos(pitch)
 * 3. Right/Up calculation via cross products:
 *    Right = normalize(Front x WorldUp)
 *    Up = normalize(Right x Front)
 */
void Camera::updateCameraVectors() {
    // Cache radians conversion
    const float yawRad = glm::radians(Yaw);
    const float pitchRad = glm::radians(Pitch);

    // Cache trig results
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    const float cosPitch = std::cos(pitchRad);
    const float sinPitch = std::sin(pitchRad);

    // Calculate the new Front vector
    Front.x = cosYaw * cosPitch;
    Front.y = sinPitch;
    Front.z = sinYaw * cosPitch;

    // Calculate Right and Up vectors (normalized to ensure uniform movement speed)
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}