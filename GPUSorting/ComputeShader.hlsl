// Odd/Even Macros
#define EVEN(x) (fmod((x),2)==0)
#define ODD(x)  (fmod((x),2)!=0)

static const int threadGroupSize = 16;
RWStructuredBuffer<float> outputData : register(u0);

cbuffer ConstantBuffer : register(b0)
{
	float4 values;
}

inline void compare_and_swap(uint l, uint r)
{
	const float x = outputData[l];
	const float y = outputData[r];

	if (x <= y) return;

	outputData[l] = y;
	outputData[r] = x;

}

void swap(uint x, uint y)
{
	float temp = outputData[x].r;
	x = outputData[y].r;
	outputData[y].r = temp;
}
float2 swap2(float x, float y)
{


	float temp = x;
	x = y;
	y = temp;

	return float2(x, y);
}
//uint3 dispatchThreadID : SV_DispatchThreadID
//uint3 GroupID : SV_GroupID
//uint3 GroupThreadID : SV_GroupThreadID
[numthreads(threadGroupSize, 1, 1)]
void CS_main(uint3 GroupThreadID : SV_GroupThreadID,
			 uint3 GroupID : SV_GroupID)
{
	int iteration = 0;
	float result = 0;
	const uint pos = GroupID.x* threadGroupSize+GroupThreadID.x;
	uint posL = max(pos - 1, 0);
	uint posR = min(pos + 1, values.x);

	while (iteration < values.x)
	{
		float C = outputData[pos].r;
		float L = outputData[posL].r; // Left
		float R = outputData[posR].r; // Right
		if ((EVEN(iteration) && EVEN(pos)) ||
			(ODD(iteration) && ODD(pos))
			)
		{
			// Max operation
			result = max(C, R);
			if(C == result)
				swap(pos, posR);
		}
		else
		{
			// Min operation
			result = min(L, C);
			if(C == result)
				swap(pos, posL);
		}
		DeviceMemoryBarrierWithGroupSync();
		iteration++;
	}

}
