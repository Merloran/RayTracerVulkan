#pragma once
struct GLFWwindow;
class SDisplayManager
{
public:
	SDisplayManager(SDisplayManager&) = delete;
	static SDisplayManager &get();

	Void startup();

	[[nodiscard]]
	const IVector2 &get_framebuffer_size();
	[[nodiscard]]
	const IVector2 &get_window_size();
	[[nodiscard]]
	Float32			get_aspect_ratio() const;
	[[nodiscard]]
	GLFWwindow*		get_window() const;


	Void poll_events();
	[[nodiscard]]
	Bool should_window_close() const;

	Void shutdown();

private:
	SDisplayManager()  = default;
	~SDisplayManager() = default;

	String name			= "Ray Tracer";
	GLFWwindow* window	= nullptr;
	IVector2 windowSize	= { 1024, 768 };
	IVector2 framebufferSize{};
};

