#include "Managers/Resource/resource_manager.hpp"
#include "Managers/Display/display_manager.hpp"
#include "Managers/Render/render_manager.hpp"


Int32 main()
{
	SResourceManager& resourceManager = SResourceManager::get();
	SDisplayManager& displayManager = SDisplayManager::get();
	SRenderManager& renderManager = SRenderManager::get();

	displayManager.startup();
	renderManager.startup();
	resourceManager.startup();
	resourceManager.load_gltf_asset("Resources/Assets/Gun/weapon.gltf");

	while (!displayManager.should_window_close())
	{
		displayManager.poll_events();
	}

	resourceManager.shutdown();
	renderManager.shutdown();
	displayManager.shutdown();
	return 0;
}