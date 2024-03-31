#include <GLFW/glfw3.h>

#include "Managers/Resource/resource_manager.hpp"
#include "Managers/Display/display_manager.hpp"
#include "Managers/Render/render_manager.hpp"
#include "Managers/Render/Camera/camera.hpp"


Int32 main()
{
	SResourceManager& resourceManager = SResourceManager::get();
	SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();

	resourceManager.startup();
	displayManager.startup();
	renderManager.startup();
	renderManager.setup_imgui();

	resourceManager.load_gltf_asset(resourceManager.ASSETS_PATH + "SponzaLighted/SponzaLighted.gltf");
	renderManager.generate_mesh_buffers(resourceManager.get_meshes());
	renderManager.generate_texture_images(resourceManager.get_textures());
	renderManager.setup_graphics_descriptors(resourceManager.get_textures());
	Camera camera{};
	camera.initialize(FVector3(5.0f, 2.0f, 0.0f));

	//renderManager.pre_render();
	Float32 lastFrame = 0.0f;
	Float32 time = 0.0f;
	while (!displayManager.should_window_close())
	{
		Float32 currentFrame = Float32(glfwGetTime());
		Float32 deltaTimeMs = currentFrame - lastFrame;
		time += deltaTimeMs;
		lastFrame = currentFrame;
		displayManager.poll_events();
		camera.catch_input(deltaTimeMs);
		renderManager.render_imgui();
		renderManager.render(camera, resourceManager.get_models(), time);
	}

	renderManager.shutdown();
	displayManager.shutdown();
	resourceManager.shutdown();
	return 0;
}
