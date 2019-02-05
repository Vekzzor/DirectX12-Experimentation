// GPUSorting.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <stdio.h>
#include <fstream>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <random>
#include <time.h>
#include <cmath>
#include <chrono>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

ID3D11Buffer* constantBuffer			= nullptr;
ID3D11Buffer* computeBuffer				= nullptr;
ID3D11Buffer* computeBufferStager		= nullptr;
ID3D11ComputeShader* computeShader		= nullptr;
ID3D11UnorderedAccessView* computeUAV	= nullptr;

ID3D11Device* device			   = nullptr;
ID3D11DeviceContext* deviceContext = nullptr;

//ID3D11ShaderResourceView* shaderResourceView = nullptr;
//ID3D11Texture1D *pTexture = NULL;

float* arr = nullptr;
float* arr2 = nullptr;
int threadCount = 10;
int values = threadCount*16;
using namespace std;
using namespace DirectX;

struct float4 //added
{
	float x;
	XMFLOAT3 filler;
};

int initContext()
{
	// Create Device
	const D3D_FEATURE_LEVEL lvl[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
									  D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, lvl, _countof(lvl),
		D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
	if (hr == E_INVALIDARG)
	{
		// DirectX 11.0 Runtime doesn't recognize D3D_FEATURE_LEVEL_11_1 as a valid value
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &lvl[1], _countof(lvl) - 1,
			D3D11_SDK_VERSION, &device, nullptr, &deviceContext);
	}
	if (FAILED(hr))
	{
		printf("Failed creating Direct3D 11 device %08X\n", hr);
		return -1;
	}

	// Verify compute shader is supported
	if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
	{
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts = { 0 };
		(void)device->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
		if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
		{
			device->Release();
			printf("DirectCompute is not supported by this device\n");
			return -1;
		}
	}
}

void createConstantBuffer()
{
	D3D11_BUFFER_DESC constantBufferDesc;

	ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));

	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.ByteWidth = sizeof(XMFLOAT4);
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.StructureByteStride = 0;

	// check if the creation failed for any reason
	HRESULT hr = 0;
	hr = device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer);
	if (FAILED(hr))
	{
		// handle the error, could be fatal or a warning...
		exit(-1);
	}
	XMFLOAT4 val;
	val.x = values;
	val.y = threadCount;
	D3D11_MAPPED_SUBRESOURCE resource;
	deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	memcpy(resource.pData, &val, sizeof(XMFLOAT4));
	deviceContext->Unmap(constantBuffer, 0);
}
void createBuffer()
{

	D3D11_BUFFER_DESC computeBufferDesc;
	computeBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	computeBufferDesc.ByteWidth = sizeof(float)*values;
	computeBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	computeBufferDesc.CPUAccessFlags = 0;
	computeBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	computeBufferDesc.StructureByteStride = sizeof(float);

	D3D11_SUBRESOURCE_DATA aData;
	ZeroMemory(&aData, sizeof(D3D11_SUBRESOURCE_DATA));
	aData.pSysMem = arr2;

	HRESULT hr = (device->CreateBuffer(&computeBufferDesc, &aData, &computeBuffer));
	if (FAILED(hr))
	{
		// handle the error, could be fatal or a warning...
		exit(-1);
	}

	D3D11_BUFFER_DESC computeBufferStagerDesc;
	computeBufferStagerDesc.Usage = D3D11_USAGE_STAGING;
	computeBufferStagerDesc.ByteWidth = sizeof(float)*values;
	computeBufferStagerDesc.BindFlags = 0;
	computeBufferStagerDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	computeBufferStagerDesc.MiscFlags = 0;
	computeBufferStagerDesc.StructureByteStride = sizeof(float);

	hr = (device->CreateBuffer(&computeBufferStagerDesc, nullptr, &computeBufferStager));
	if (FAILED(hr))
	{
		// handle the error, could be fatal or a warning...
		exit(-1);
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = values;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	hr = device->CreateUnorderedAccessView(computeBuffer, &uavDesc, &computeUAV);
	if (FAILED(hr))
	{
		// handle the error, could be fatal or a warning...
		exit(-1);
	}
	
	createConstantBuffer();
}
void createShader()
{
	ID3DBlob* CS = nullptr;
	D3DCompileFromFile(
		L"ComputeShader.hlsl", // filename
		nullptr,		// optional macros
		nullptr,		// optional include files
		"CS_main",		// entry point
		"cs_5_0",		// shader model (target)
		0,				// shader compile options
		0,				// effect compile options
		&CS,			// double pointer to ID3DBlob		
		nullptr		// pointer for Error Blob messages.
					// how to use the Error blob, see here
					// https://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
	);
	HRESULT hr = device->CreateComputeShader(CS->GetBufferPointer(), CS->GetBufferSize(), nullptr, &computeShader);
	if (FAILED(hr))
	{
		// handle the error, could be fatal or a warning...
		exit(-1);
	}
	// we do not need anymore this COM object, so we release it.
	/*D3D11_INPUT_ELEMENT_DESC computeInputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};*/
	CS->Release();
}
/*void createTexture()
{
	HRESULT hr;

	D3D11_TEXTURE1D_DESC desc;
	desc.Width = values;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA aData;
	ZeroMemory(&aData, sizeof(D3D11_SUBRESOURCE_DATA));
	aData.pSysMem = arr;

	hr = device->CreateTexture1D(&desc, &aData, &pTexture);

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};

	shaderResourceViewDesc.Format = desc.Format;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(pTexture, &shaderResourceViewDesc, &shaderResourceView);

	
}*/
void createData()
{
	arr = new float[values];//{ 5, 2, 7, 3, 9, 1 };
	arr2 = new float[values];
	for (int i = 0; i < values; i++)
	{
		//arr[i] = values - i;
		//arr2[i] = values - i;
		arr[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
		arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
	}
}

int main()
{
	double time1 = 0;

	initContext();
	createData();
	createShader();
	createBuffer();
	//createTexture();

	deviceContext->CSSetShader(computeShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &constantBuffer);
	//deviceContext->CSSetShaderResources(0, 1, &shaderResourceView);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &computeUAV, 0);

	auto start = std::chrono::high_resolution_clock::now();

	deviceContext->Dispatch(threadCount, 1, 1);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CopyResource(computeBufferStager, computeBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = deviceContext->Map(computeBufferStager, 0, D3D11_MAP_READ, 0, &mappedResource);

	if (SUCCEEDED(hr))
	{
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		time1 = elapsed_seconds.count();
		std::cout << std::endl << "Sorting time: " << time1 << std::endl;

		ofstream myfile;
		myfile.open("data.txt");
		myfile << "Data: \n";
		float* dataView = reinterpret_cast<float*>(mappedResource.pData);
		for (int i = 0; i < values; i++)
		{
			myfile << dataView[i] << endl;

			if (i+1 != values && dataView[i] > dataView[i+1])
			{
				std::cout << std::endl << "SORTED INCORRECTLY" << std::endl;

			}
		}
		myfile.close();
		deviceContext->Unmap(computeBufferStager, 0);
	}
	computeBuffer->Release();
	computeBufferStager->Release();
	computeShader->Release();
	computeUAV->Release();

	deviceContext->Release();
	device->Release();
	//shaderResourceView->Release();
	//pTexture->Release();
	constantBuffer->Release();
	delete[] arr;
	delete[] arr2;
	system("pause");
}
