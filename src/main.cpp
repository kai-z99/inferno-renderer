#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


// look at babylon's subsurface
// Look at specular ao
// ClearCoat/Sheen
// ScreenSpaceSubSurfaceScattering
// emissive simple
// paralax map
// cascade shadow map

