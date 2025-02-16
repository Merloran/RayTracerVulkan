#include "display_manager.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


SDisplayManager& SDisplayManager::get()
{
	static SDisplayManager instance;
	return instance;
}

Void SDisplayManager::startup()
{
	SPDLOG_INFO("Display Manager startup.");
	if (!glfwInit())
	{
		SPDLOG_ERROR("Failed to initialize GLFW!");
		return;
	}

	doesFramebufferResized = false;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(windowSize.x, windowSize.y, name.c_str(), nullptr, nullptr);
	glfwSetFramebufferSizeCallback(window, s_framebuffer_resize_callback);
	glfwSetWindowUserPointer(window, this);
}

const IVector2& SDisplayManager::get_framebuffer_size()
{
	if (window == nullptr)
	{
		SPDLOG_ERROR("Window is null!");
		return IVector2{ -1, -1 };
	}
	glfwGetFramebufferSize(window, &framebufferSize.x, &framebufferSize.y);
	return framebufferSize;
}

const IVector2& SDisplayManager::get_window_size()
{
	if (window == nullptr)
	{
		SPDLOG_ERROR("Window is null!");
		return IVector2{ -1, -1 };
	}
	glfwGetWindowSize(window, &windowSize.x, &windowSize.y);
	return windowSize;
}

Float32 SDisplayManager::get_aspect_ratio() const
{
	if (windowSize.y == 0)
	{
		return 0.0f;
	}

	return Float32(windowSize.x) / Float32(windowSize.y);
}

GLFWwindow& SDisplayManager::get_window() const
{
	return *window;
}

Void SDisplayManager::poll_events()
{
	glfwPollEvents();
	glfwGetWindowSize(window, &windowSize.x, &windowSize.y);
	glfwGetFramebufferSize(window, &framebufferSize.x, &framebufferSize.y);
}

Bool SDisplayManager::should_window_close() const
{
	if (window == nullptr)
	{
		SPDLOG_ERROR("Window is null!");
		return true;
	}

	return glfwWindowShouldClose(window);
}

Bool SDisplayManager::was_resize_handled()
{
	if (doesFramebufferResized)
	{
		doesFramebufferResized = false;
		return true;
	}
	return false;
}

Void SDisplayManager::shutdown()
{
	SPDLOG_INFO("Display Manager shutdown.");
	glfwDestroyWindow(window);
	glfwTerminate();
	window = nullptr;
}

Void SDisplayManager::s_framebuffer_resize_callback(GLFWwindow* window, Int32 width, Int32 height)
{
	SDisplayManager& displayManager = *static_cast<SDisplayManager*>(glfwGetWindowUserPointer(window));
	displayManager.doesFramebufferResized = true;
}
