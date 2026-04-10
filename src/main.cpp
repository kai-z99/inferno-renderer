#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


//1.) specular anti alising!!!
//2.  look at KHR texture transform
//3. Do an orbit camera mode
//4. Blur background?
//5. Fix that sync error...
//6. Look into specular pattern in prefilter



// look at babylon's subsurface
// ClearCoat done/Sheen
// KHR transmission
// paralax map
// cascade shadow map

