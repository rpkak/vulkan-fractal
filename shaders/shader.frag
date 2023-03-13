#version 450

layout(location = 0) in vec2 coordinate;

layout(location = 0) out vec4 outColor;

void set_out_color(uint i)
{
    float group = mod(i, 360);
    float f = mod(i / 60.0, 1.0);
    if (group < 60)
    {
        outColor = vec4(1, f, 0, 1);
    }
    else if (group >= 60 && group < 120)
    {
        outColor = vec4(1 - f, 1, 0, 1);
    }
    else if (group >= 120 && group < 180)
    {
        outColor = vec4(0, 1, f, 1);
    }
    else if (group >= 180 && group < 240)
    {
        outColor = vec4(0, 1 - f, 1, 1);
    }
    else if (group >= 240 && group < 300)
    {
        outColor = vec4(f, 0, 1, 1);
    }
    else if (group >= 300)
    {
        outColor = vec4(1, 0, 1 - f, 1);
    }
}

void main() {
    float c_r=coordinate.x;
    float c_i=coordinate.y;

    float z_r_tmp;

    float z_r=c_r;
    float z_i=c_i;

    for(uint i = 0; i<2000;i++){
        if((z_r*z_r)+(z_i*z_i)>4){
            set_out_color(i);
            return;
        }
        z_r_tmp=z_r;
        z_r=z_r*z_r-z_i*z_i+c_r;
        z_i=2*z_r_tmp*z_i+c_i;
    }
    outColor=vec4(0,0,0,1);
    
    // outColor = vec4(coordinate, 0.0, 1.0);
}
