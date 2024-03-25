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

	resourceManager.load_gltf_asset(resourceManager.ASSETS_PATH + "SponzaLighted/SponzaLighted.gltf");
	renderManager.generate_mesh_buffers(resourceManager.get_meshes());
	Camera camera;
	camera.initialize(glm::vec3(5.0f, 2.0f, 0.0f));

	Float32 lastFrame = 0.0f;
	while (!displayManager.should_window_close())
	{
		Float32 currentFrame = glfwGetTime();
		Float32 deltaTimeMs = currentFrame - lastFrame;
		lastFrame = currentFrame;
		displayManager.poll_events();
		camera.catch_input(deltaTimeMs);

		renderManager.draw_frame(camera, resourceManager.get_meshes());
	}

	renderManager.shutdown();
	displayManager.shutdown();
	resourceManager.shutdown();
	return 0;
}
