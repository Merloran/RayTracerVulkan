#include <GLFW/glfw3.h>
#include <thread>

#include "Managers/Resource/resource_manager.hpp"
#include "Managers/Display/display_manager.hpp"
#include "Managers/Raytrace/raytrace_manager.hpp"
#include "Managers/Render/render_manager.hpp"
#include "Managers/Render/Camera/camera.hpp"
#include "Managers/Resource/Common/texture.hpp"


Int32 main()
{
	SResourceManager& resourceManager = SResourceManager::get();
	SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();
	SRaytraceManager& raytraceManager = SRaytraceManager::get();

	resourceManager.startup();
	resourceManager.load_gltf_asset(resourceManager.ASSETS_PATH + "SponzaLighted/SponzaLighted.gltf");
	resourceManager.load_texture(resourceManager.TEXTURES_PATH + "EnvironmentMap.hdr", "EnvironmentMap", ETextureType::HDR);


	displayManager.startup();
	renderManager.startup();
	renderManager.setup_imgui();

	renderManager.generate_mesh_buffers(resourceManager.get_meshes());
	renderManager.generate_texture_images(resourceManager.get_textures());
	renderManager.setup_graphics_descriptors(resourceManager.get_textures());
	raytraceManager.startup();

	
	Camera camera{};
	camera.initialize(FVector3(5.0f, 2.0f, 0.0f));
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
		renderManager.update_imgui();
		if (raytraceManager.isEnabled)
		{
			raytraceManager.update(camera, deltaTimeMs);
		} else {
			renderManager.render(camera, resourceManager.get_models(), time);
		}
		renderManager.render_imgui();
	}
	

	raytraceManager.shutdown();
	renderManager.shutdown();
	displayManager.shutdown();
	resourceManager.shutdown();
	return 0;
}
