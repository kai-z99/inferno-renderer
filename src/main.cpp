#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


// apply specular correction factors to direct light
// ClearCoat/Sheen
// ScreenSpaceSubSurfaceScattering
// emissive simple
// paralax map
// cascade shadow map

