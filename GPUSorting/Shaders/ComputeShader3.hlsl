// Odd/Even Macros
#define EVEN(x) (fmod((x), 2) == 0)
#define ODD(x) (fmod((x), 2) != 0)
static const int threadGroupSize = 128;
static const int bufferSize		 = 512;
RWStructuredBuffer<float> outputData : register(u0, space0);
StructuredBuffer<float> srv : register(t0, space0);

inline void compare_and_swap(uint l, uint r)
{
	const float x = outputData[l];
	const float y = outputData[r];

	if(x <= y)
		return;

	outputData[l] = y;
	outputData[r] = x;
}

void swap(uint x, uint y)
{
	float temp		= outputData[x].r;
	x				= outputData[y].r;
	outputData[y].r = temp;
}

[numthreads(threadGroupSize, 1, 1)] 
void CS_main(uint3 GroupThreadID : SV_GroupThreadID,
			 uint3 GroupID : SV_GroupID)
{
	int iteration  = 0;
	float result   = 0;
	const uint pos = GroupID.x * threadGroupSize + GroupThreadID.x;
	uint posL	  = max(pos - 1, 0);
	uint posR	  = min(pos + 1, bufferSize);

	while(iteration < bufferSize)
	{
		float C = outputData[pos].r;
		float L = outputData[posL].r; // Left
		float R = outputData[posR].r; // Right
		if((EVEN(iteration) && EVEN(pos)) || (ODD(iteration) && ODD(pos)))
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