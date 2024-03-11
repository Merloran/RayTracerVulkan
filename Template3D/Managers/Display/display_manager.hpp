#pragma once

class SDisplayManager
{
public:
	SDisplayManager(SDisplayManager&) = delete;
	static SDisplayManager &get();

	void startup();

	[[nodiscard]]
	const IVector2 &get_framebuffer_size();
	[[nodiscard]]
	const IVector2 &get_window_size();
	[[nodiscard]]
	Float32			get_aspect_ratio() const;

	Void poll_events();
	[[nodiscard]]
	Bool should_window_close() const;

	Void shutdown();

private:
	SDisplayManager()  = default;
	~SDisplayManager() = default;

	String name				  = "BaseWindow";
	struct GLFWwindow* window = nullptr;
	IVector2 windowSize		  = { 1024, 768 };
	IVector2 framebufferSize{};
};

