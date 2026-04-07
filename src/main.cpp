#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


//fix mip levels in prefilter shader (by genreating them)
// IBL
// ClearCoat
// ScreenSpaceSubSurfaceScattering
// optimze shadowpass culling
// paralax map
// cascade shadow map

