#include <vk_types.h>
#include <SDL_events.h>


class Camera 
{
public:
    enum class CameraControls : int
    {
        Orbit = 0,
        Fly = 1,
        
    };

    CameraControls controls;

    glm::vec3 velocity {0.0f};
    glm::vec3 position {0.0f, 0.0f, 4.0f};

    // vertical rotation
    float pitch { 0.f };
    
    // horizontal rotation
    float yaw { 0.f };

    // target for orbit
    glm::vec3 target {0.0f};
    float orbitDistance;
    float orbitMinDistance { 0.5f };
    float orbitMaxDistance { 100.0f };

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void processSDLEvent(SDL_Event& e);

    void update();



};
