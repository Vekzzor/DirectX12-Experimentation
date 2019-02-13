#pragma once

// DirectX
#include "pch.h"
#include "d3dx12.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "D3d12.lib")
#pragma comment(lib, "DXGI.lib")

// Comptr
#include <wrl.h>

// other
#include <string>

#pragma region HR Thrower

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr)
		: std::runtime_error(HrToString(hr))
		, m_hr(hr)
	{
		puts(HrToString(hr).c_str());
	}
	HRESULT Error() const
	{
		return m_hr;
	}

private:
	const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
#if _DEBUG
	if(FAILED(hr))
	{
		throw HrException(hr);
	}
#endif
}

template <class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}


#pragma endregion