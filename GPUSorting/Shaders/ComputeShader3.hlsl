RWStructuredBuffer<uint> outputData : register(u0, space0);
StructuredBuffer<uint> srv : register(t0, space0);
[numthreads(1, 1, 1)]
void CS_main( uint3 DTid : SV_DispatchThreadID )
{
		outputData[DTid.x] = srv.Load(3);
}