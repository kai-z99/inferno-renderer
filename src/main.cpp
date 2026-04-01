#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


// IBL
// ClearCoat
// ScreenSpaceSubSurfaceScattering
// optimze shadowpass culling

