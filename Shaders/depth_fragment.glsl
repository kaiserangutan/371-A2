#version 330 core
in vec3 vertexColor;
out vec4 FragColor;

uniform bool showDepth = true;
      
float near = 1.0f;
float far  = 200.0f;
float LinearizeDepth() 
{
//    // compute ndc coord, by default glDepthRange(0,1) \n
    float Zndc = gl_FragCoord.z * 2.0 - 1.0; 
//    // invert fraction \n
    float Zeye = (2.0 * near * far) / (far + near - Zndc * (far - near)); 
    return (Zeye - near)/ ( far - near);
}

void main()
{
    if(showDepth)
        FragColor = vec4(vec3(LinearizeDepth()), 1.0f);
    else
        FragColor = vec4(vertexColor, 1.0f);
};