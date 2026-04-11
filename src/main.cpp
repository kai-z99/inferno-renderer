#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}


//1.) specular anti alising DONE
//X. Look into the transparent pipeline (alpha bledning etc) and how it relateswith KHR_Transmission
//2.  look at KHR texture transform
//3. Do an orbit camera mode DONE
//X. Live scene change (model and env) DONE
//4. Fix that sync error...
//5. Blur background?
//6. Look into specular pattern in prefilter
//7 set up clearcoat normal


// look at babylon's subsurface
// ClearCoat done/Sheen
// KHR transmission
// paralax map
// cascade shadow map

