#include "camera.hpp"

#include <GLFW/glfw3.h>
#include "../../Display/display_manager.hpp"

Void Camera::initialize(const FVector3& position)
{
	this->position = position;
	forward = { 1.0f,  0.0f, 0.0f };
	up = { 0.0f,  1.0f,  0.0f };
	right = { 0.0f,  0.0f,  1.0f };
	viewBounds = { 0.001f,  5000.0f };
	yaw = -180.0f;
	pitch = 0.0f;
	speed = 10.0f;
	sensitivity = 10.0f;
	fov = 70.0f;
	isInactive = true;
	hasChanged = true;

	update_camera_vectors();
}

Void Camera::move_forward(Float32 dt)
{
	position += forward * dt * speed;
	hasChanged = true;
}

Void Camera::move_right(Float32 dt)
{
	position += right * dt * speed;
	hasChanged = true;
}

Void Camera::move_up(Float32 dt)
{
	position += FVector3(0.0f, 1.0f, 0.0f) * dt * speed;
	hasChanged = true;
}

Void Camera::rotate(Float32 xOffset, Float32 yOffset)
{
	xOffset *= sensitivity;
	yOffset *= sensitivity;

	yaw += xOffset;
	pitch += yOffset;

	if (pitch > 89.0f)
	{
		pitch = 89.0f;
	}

	if (pitch < -89.0f)
	{
		pitch = -89.0f;
	}

	update_camera_vectors();
}

const FMatrix4& Camera::get_view() const
{
	return glm::lookAt(position, position + forward, up);
}

const FMatrix4& Camera::get_projection(Float32 aspectRatio) const
{
	return glm::perspective(glm::radians(fov), aspectRatio, viewBounds.x, viewBounds.y);
}

const FVector3& Camera::get_forward() const
{
	return forward;
}

const FVector3& Camera::get_right() const
{
	return right;
}

const FVector3& Camera::get_up() const
{
	return up;
}

const FVector3& Camera::get_position() const
{
	return position;
}

const FVector2& Camera::get_view_bounds() const
{
	return viewBounds;
}

Float32 Camera::get_sensitivity() const
{
	return sensitivity;
}

Float32 Camera::get_speed() const
{
	return speed;
}

Float32 Camera::get_fov() const
{
	return fov;
}

Bool Camera::has_changed() const
{
	return hasChanged;
}

Void Camera::set_position(const FVector3& position)
{
	this->position = position;
	hasChanged = true;
}

Void Camera::set_view_bounds(const FVector2& viewBounds)
{
	this->viewBounds = viewBounds;
	hasChanged = true;
}

Void Camera::set_sensitivity(Float32 sensitivity)
{
	this->sensitivity = sensitivity;
	hasChanged = true;
}

Void Camera::set_speed(Float32 speed)
{
	this->speed = speed;
	hasChanged = true;
}

Void Camera::set_fov(Float32 fov)
{
	this->fov = fov;
	hasChanged = true;
}

Void Camera::set_camera_changed(Bool hasChanged)
{
	this->hasChanged = hasChanged;
}

Void Camera::catch_input(Float32 deltaTime)
{
	SDisplayManager& displayManager = SDisplayManager::get();

	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_E) == GLFW_PRESS)
	{
		if (glfwGetInputMode(&displayManager.get_window(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		{
			glfwSetInputMode(&displayManager.get_window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			isInactive = true;
		} else {
			glfwSetInputMode(&displayManager.get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	}

	if (glfwGetInputMode(&displayManager.get_window(), GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
	{
		return;
	}

	DVector2 mousePosition;
	glfwGetCursorPos(&displayManager.get_window(), &mousePosition.x, &mousePosition.y);
	if (isInactive)
	{
		lastPosition.x = mousePosition.x;
		lastPosition.y = mousePosition.y;
		isInactive = false;
	}

	FVector2 offset = FVector2(mousePosition) - lastPosition;
	offset.y = -offset.y;

	lastPosition = mousePosition;
	if (glm::length2(offset) > 0.0f)
	{
		rotate(offset.x * deltaTime, offset.y * deltaTime);
	}

	// Camera Movement
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_W) == GLFW_PRESS)
	{
		move_forward(deltaTime);
	}
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_S) == GLFW_PRESS)
	{
		move_forward(-deltaTime);
	}
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_A) == GLFW_PRESS)
	{
		move_right(-deltaTime);
	}
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_D) == GLFW_PRESS)
	{
		move_right(deltaTime);
	}
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		move_up(deltaTime);
	}
	if (glfwGetKey(&displayManager.get_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		move_up(-deltaTime);
	}

}

Void Camera::update_camera_vectors()
{
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);

	right = glm::normalize(glm::cross(forward, FVector3(0.0f, 1.0f, 0.0f)));
	up = glm::normalize(glm::cross(right, forward));

	hasChanged = true;
}
