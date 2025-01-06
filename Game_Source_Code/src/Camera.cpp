#include "headers/Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
                                                                           MouseSensitivity(SENSITIVITY){
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// constructor with scalar values
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw,
               float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)),MouseSensitivity(SENSITIVITY){
    Position = glm::vec3(posX, posY, posZ);
    WorldUp = glm::vec3(upX, upY, upZ);
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 Camera::getViewMatrix() {
    return lookAt(Position, Position + Front, Up);
}

// processes input received from any keyboard-like input system.
void Camera::processKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = deltaTime;
    if (direction == FORWARD) { Position += Front * velocity; }
    if (direction == BACKWARD) { Position -= Front * velocity; }
    if (direction == LEFT) { Position -= Right * velocity; }
    if (direction == RIGHT) { Position += Right * velocity; }
    if (direction == UP) { Position += glm::vec3(0, 1, 0) * velocity; }
    if (direction == DOWN) { Position -= glm::vec3(0, 1, 0) * velocity; }
}

// processes input received from a mouse
void Camera::processMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f)Pitch = -89.0f;
        if (Yaw > 360.0f) Yaw -= 360.0f;
        if (Yaw < 0.0f) Yaw += 360.0f;
    }

    // update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

// calculates the front vector from the Camera's (updated) Euler Angles
void Camera::updateCameraVectors() {
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}
