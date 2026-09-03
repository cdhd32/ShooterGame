#pragma once
#include<d3d12.h>
#include<Windows.h>
#include<cassert>
#include<string>
#include<algorithm>

using namespace std;

class Utils
{
public:
	static void PrintErrorBlob(ID3DBlob* blob);

	static bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error);


};

