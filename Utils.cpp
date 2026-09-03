#include "Utils.h"


void Utils::PrintErrorBlob(ID3DBlob* blob)
{
	assert(blob);
	string errstr;
	errstr.resize(blob->GetBufferSize());

	std::copy_n((char*)blob->GetBufferPointer(), blob->GetBufferSize(), errstr.begin());
	errstr += "\n";

	::OutputDebugStringA(errstr.c_str());
}

bool Utils::CheckShaderCompileResult(HRESULT result, ID3DBlob* error) {
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("File not exist");
		}
		else
		{
			PrintErrorBlob(error);
		}
		return false;
	}
	else
	{
		return true;
	}
}