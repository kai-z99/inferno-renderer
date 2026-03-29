//input: shadowmap, fragpos, lightmatrix
//output: shadowfactor

//THIS SET IS PROGRESSIVE FOR 8 16 32 64 SAMPLES. THEREFORE WE CAN JUST CLAMP THE LOOP TO THE DESIRED SAMPLE COUNT
const int POISSON_MAX = 64;
const vec2 poisson[POISSON_MAX] = vec2[](
        vec2(-0.0936905, 0.3196758),
        vec2(0.1592735, -0.9686295),
        vec2(0.9430245, 0.3139912),
        vec2(-0.7416366, -0.4377831),
        vec2(-0.9517487, 0.2963554),
        vec2(0.8581337, -0.4240460),
        vec2(0.3276062, 0.9244621),
        vec2(-0.5325066, 0.8410385),
        vec2(0.0902534, -0.3503742),
        vec2(0.4452095, 0.2580113),
        vec2(-0.4462856, -0.0426502),
        vec2(-0.2192158, -0.6911137),
        vec2(-0.1154335, 0.8248222),
        vec2(0.5149567, -0.7502338),
        vec2(-0.5523247, 0.4272514),
        vec2(0.6470215, 0.7474022),
        vec2(-0.5987766, -0.7512833),
        vec2(0.1604507, 0.5460774),
        vec2(0.5947998, -0.2146744),
        vec2(-0.1203411, -0.1301079),
        vec2(-0.7304786, -0.0100693),
        vec2(-0.3897587, -0.4665619),
        vec2(0.3929337, -0.5010948),
        vec2(-0.3096867, 0.5588146),
        vec2(0.0617981, 0.0729416),
        vec2(0.6455986, 0.0441933),
        vec2(0.8934509, 0.0736939),
        vec2(-0.3580975, 0.2806469),
        vec2(-0.8682281, -0.1990303),
        vec2(0.1853630, 0.3213367),
        vec2(0.8400612, -0.2001190),
        vec2(-0.1598610, 0.1038342),
        vec2(0.6632416, 0.3067062),
        vec2(0.1562584, -0.5610626),
        vec2(-0.6930340, 0.6913887),
        vec2(-0.9402866, 0.0447434),
        vec2(0.3029106, 0.0949703),
        vec2(0.6464897, -0.4666451),
        vec2(0.4356628, -0.0710125),
        vec2(0.1253822, 0.9892166),
        vec2(0.0349884, -0.7968109),
        vec2(0.3935608, 0.4609676),
        vec2(0.3085465, -0.7842533),
        vec2(-0.3090832, 0.9020988),
        vec2(-0.6518704, -0.2503952),
        vec2(-0.4037193, -0.2611179),
        vec2(0.3401214, -0.3047142),
        vec2(-0.0197372, 0.6478714),
        vec2(0.1741608, -0.1682285),
        vec2(-0.5128918, 0.1448544),
        vec2(-0.1596546, -0.8791054),
        vec2(0.6987045, -0.6843052),
        vec2(-0.7445076, 0.5035095),
        vec2(-0.5862702, -0.5531025),
        vec2(0.4112572, 0.7500054),
        vec2(-0.1080467, -0.5329178),
        vec2(0.8587891, 0.4838005),
        vec2(-0.7647934, 0.2709858),
        vec2(-0.1493771, -0.3147511),
        vec2(-0.4676369, 0.6570358),
        vec2(0.6295372, 0.5629555),
        vec2(0.0689201, 0.8124840),
        vec2(-0.0566467, 0.9952820),
        vec2(-0.4230408, -0.7129914)
);


float ShadowEval_PCF(sampler2D shadowMap, vec4 fragPosWorld, mat4 lightViewProj)
{
    const int sampleCount = 32;
    const float sampleRadius = 2.5;
    const float shadowBias = 0.001;

    vec4 fragPosLightClip = lightViewProj * fragPosWorld;
    vec3 fragPosLightNDC = vec3(fragPosLightClip / fragPosLightClip.w);
    vec2 shadowUV = fragPosLightNDC.xy * 0.5 + 0.5; //-1,1 -> 0,1

    float currentDepth = fragPosLightNDC.z;

    float sum = 0.0;
    int iterations = clamp(sampleCount, 1, POISSON_MAX);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    for (int i = 0; i < iterations; i++)
    {
            vec2 o = poisson[i] * sampleRadius * texelSize;

            float closestDepth = texture(shadowMap, shadowUV + o).r;

            sum += (currentDepth - shadowBias > closestDepth) ? 0.0 : 1.0;
    }
    return sum / max(float(iterations), 1.0);

    // for (int i = -1; i <= 1; i++)
    // {
    //     for (int j = -1; j <= 1; j++)
    //     {
    //         vec2 o = vec2(i, j) * sampleRadius * texelSize;

    //         float closestDepth = texture(shadowMap, shadowUV + o).r;

    //         sum += (currentDepth - shadowBias > closestDepth) ? 0.0 : 1.0;
    //     }
    // }

    // return sum / 9.0;

    //float closestDepth = texture(shadowMap, shadowUV).r;
    //return (currentDepth - shadowBias > closestDepth) ? 0.0 : 1.0;
    
}