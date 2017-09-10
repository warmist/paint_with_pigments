//!program main fragment
#version 330

out vec4 color;
uniform sampler2DArray color_layers;
uniform sampler1DArray pigments;
varying vec3 pos;

uniform float power;

float xFit_1931(  float wave )
{
	 float t1 = (wave-442.0f)*((wave<442.0f)?0.0624f:0.0374f);
	 float t2 = (wave-599.8f)*((wave<599.8f)?0.0264f:0.0323f);
	 float t3 = (wave-501.1f)*((wave<501.1f)?0.0490f:0.0382f);
	return 0.362f*exp(-0.5f*t1*t1) + 1.056f*exp(-0.5f*t2*t2)
	- 0.065f*exp(-0.5f*t3*t3);
}
float yFit_1931(  float wave )
{
	 float t1 = (wave-568.8f)*((wave<568.8f)?0.0213f:0.0247f);
	 float t2 = (wave-530.9f)*((wave<530.9f)?0.0613f:0.0322f);
	return 0.821f*exp(-0.5f*t1*t1) + 0.286f*exp(-0.5f*t2*t2);
}
float zFit_1931(float wave )
{
	 float t1 = (wave-437.0f)*((wave<437.0f)?0.0845f:0.0278f);
	 float t2 = (wave-459.0f)*((wave<459.0f)?0.0385f:0.0725f);
	return 1.217f*exp(-0.5f*t1*t1) + 0.681f*exp(-0.5f*t2*t2);
}
vec3 fit_1931(float w)
{
	return vec3(xFit_1931(w),yFit_1931(w),zFit_1931(w));
}
float black_body_spectrum( float l, float temperature )
{
	/*float h=6.626070040e-34; //Planck constant
	float c=299792458; //Speed of light
	float k=1.38064852e-23; //Boltzmann constant
	*/
	 float const_1=5.955215e-17;//h*c*c
	 float const_2=0.0143878;//(h*c)/k
	 float top=(2*const_1);
	 float bottom=(exp((const_2)/(temperature*l))-1)*l*l*l*l*l;
	return top/bottom;
	//return (2*h*freq*freq*freq)/(c*c)*(1/(math.exp((h*freq)/(k*temperature))-1))
}
vec3 get_bb_xyz(float t)
{
	vec3 lab=vec3(0,0,0);
	for(float l=380;l<=730;l+=10)
	{
		lab+=fit_1931(l)*black_body_spectrum(l*1e-9,t);
	}
	return lab;
}
vec3 xyz2rgb(vec3 c ) {
	const mat3 mat = mat3(
        3.2406, -1.5372, -0.4986,
        -0.9689, 1.8758, 0.0415,
        0.0557, -0.2040, 1.0570
	);
    vec3 v =c*mat;// mul(c , mat);
    vec3 r;
    r.x = ( v.r > 0.0031308 ) ? (( 1.055 * pow( v.r, ( 1.0 / 2.4 ))) - 0.055 ) : 12.92 * v.r;
    r.y = ( v.g > 0.0031308 ) ? (( 1.055 * pow( v.g, ( 1.0 / 2.4 ))) - 0.055 ) : 12.92 * v.g;
    r.z = ( v.b > 0.0031308 ) ? (( 1.055 * pow( v.b, ( 1.0 / 2.4 ))) - 0.055 ) : 12.92 * v.b;
    return r;
}
float sample_pigment(float pos,int pig_id)
{
	return texelFetch(pigments,ivec2(int((pos-380)/10),pig_id),0).x;
}
vec3 get_bb_pigment(float t,int id)
{
	vec3 lab=vec3(0,0,0);
	for(float l=380;l<=730;l+=10)
	{
		lab+=fit_1931(l)*black_body_spectrum(l*1e-9,t)*sample_pigment(l,id);
	}
	return lab;
}
vec3 get_bb_pigment_mix(float t,int id,int id2,float w1,float w2)
{
	vec3 lab=vec3(0,0,0);
	for(float l=380;l<=730;l+=10)
	{
		float p1=sample_pigment(l,id);
		float p2=sample_pigment(l,id2);
		float mean=pow(pow(p1,w1)*pow(p2,w2),1/(w1+w2));
		lab+=fit_1931(l)*black_body_spectrum(l*1e-9,t)*mean;
	}
	return lab;
}
vec3 sample_pixel(vec2 pos,float t)
{
	float w[5];
	for(int i=0;i<5;i++)
	{
		w[i]=texture(color_layers, vec3(pos.xy, i)).x;
	}

	vec3 lab=vec3(0,0,0);
	for(float l=380;l<=730;l+=10)
	{
		float w_sum=0;
		float x_mean=0;
		for(int i=0;i<5;i++)
		{
			//if(w[i]>0.0000001)
			//{
				x_mean+=log(sample_pigment(l,i))*w[i];
				w_sum+=w[i];
			//}
		}
		if(w_sum>0.00000001)
		{
			x_mean=exp(x_mean/w_sum);
			lab+=fit_1931(l)*black_body_spectrum(l*1e-9,t)*x_mean;
		}
	}
	return lab;
}

void main(){
	vec2 pn=(pos.xy+vec2(1))/2;
	int pig_id=int(pn.y*5);

	float pow_v=pow(10,power*30);
	//color = vec4(texture(color_layers, vec3(pos.xy, 0)));
	//color = vec4(texture(pigments, pos.xy*3));
	vec3 bb=sample_pixel((pos.xy+vec2(1))/2,5800);

	//vec3 bb;
	/*if(pn.x<0.5)
		bb=get_bb_pigment(5800,pig_id);
	else
		bb=get_bb_pigment_mix(5800,pig_id,2,power,(1-power));*/
	//if(bb.y>0.00001)
	{
		bb/=pow_v;
	}
	color=vec4(xyz2rgb(bb),1);
	/*
	float v=texelFetch(pigments,ivec2(pn.x*40,pig_id),0).x;
	color=vec4(v,v,v,1);*/
}
