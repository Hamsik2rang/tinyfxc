/*
 * MIT License
 *
 * Copyright (c) 2024 Yongsik Im
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstdio>
#include <d3dcompiler.h>
#include <Windows.h>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

enum e_error_code
{
	NONE = 0,
	INVALID_SHADER_PROFILE = 1,
	INVALID_SHADER_DEFINE = 2,
	INVALID_ARGUMENT = 3,
	INVALID_ENTRY_NAME = 4,
	INVALID_INCLUDE_PATH = 5,
	INVALID_INPUT_PATH = 6,
	INVALID_OUTPUT_PATH = 7,

	SHADER_COMPILE_FAILED = 100,
};

class IncludeHandler : public ID3DInclude
{
public:
	IncludeHandler(const std::vector<const char*>& includePaths)
	{
		for (const auto& path : includePaths)
		{
			size_t len = strlen(path);
			char* buf = new char[2048];
			if (path[0] == '.') // relative path
			{
				GetFullPathNameA(path, 2048, buf, NULL);
			}
			else
			{
				memcpy(buf, path, len);
			}

			_includePaths.push_back(buf);
		}
	}

	~IncludeHandler()
	{
		for (auto& path : _includePaths)
		{
			delete[] path;
			path = nullptr;
		}
		_includePaths.clear();
	}

	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
	{
		bool hasFound = false;

		for (const auto& path : _includePaths)
		{
			char fullPath[2048];
			sprintf_s(fullPath, "%s%s", path, pFileName);
			fprintf(stdout, "shader compile include path: %s\n", fullPath);
			
			FILE* fp;
			fopen_s(&fp, fullPath, "rb");
			if (nullptr == fp)
			{
				continue;
			}

			hasFound = true;

			fseek(fp, 0, SEEK_END);
			*pBytes = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			char* buf = new char[*pBytes];
			fread(buf, 1, *pBytes, fp);
			*ppData = buf;

			fclose(fp);

			break;
		}

		return hasFound ? S_OK : E_FAIL;
	}

	HRESULT __stdcall Close(LPCVOID pData)
	{
		char* buf = (char*)pData;
		delete[] buf;
		return S_OK;
	}

private:
	std::vector<const char*> _includePaths;
};

bool CompareArgument(const char* argument, const char* syntax, bool* hasLineSpacing)
{
	assert(argument);
	assert(syntax);
	assert(hasLineSpacing);

	size_t argumentLen = strlen(argument);
	size_t syntaxLen = strlen(syntax);

	if (argumentLen < syntaxLen)
	{
		return 0;
	}

	for (size_t  i = 0; i < syntaxLen; i++)
	{
		if (argument[i] != syntax[i])
		{
			return 0;
		}
	}
	*hasLineSpacing = argumentLen == syntaxLen ? 1 : 0;

	return 1;
}

//wine64 ./testcompiler.exe /T ps_5_0 /E fragment /Fo ./output.dxbc /Od /I ./TEMP ./temp.hlsl
int main(int argc, char** argv)
{
	uint8_t errorCode = 0;

	char* hlslCode = nullptr;
	size_t hlslCodeSize = 0;
	ID3DBlob* compiledShader = NULL;
	ID3DBlob* compilationErrors = NULL;

	FILE* fp = nullptr;
	HRESULT hr;

	char** argBuffer = new char* [argc] {};

	printf("Hello! this is tiny fxc.\n");
	printf("yonr arguments is :\n");
	for (int i = 0; i < argc; i++)
	{
		char buf[2048] = { '\0' };
		strcpy_s(buf, 2047, argv[i]);
		printf("\t%s\n", buf);
		if (nullptr != argBuffer)
		{
			argBuffer[i] = new char[2048] {};
			if (nullptr != argBuffer[i])
			{
				memcpy(argBuffer[i], buf, 2048);
			}
		}
	}

	const char* inputPath = nullptr;
	const char* outputPath = nullptr;
	const char* shaderProfile = nullptr;
	const char* entryName = nullptr;
	IncludeHandler* includeHandler = nullptr;
	std::vector<const char*> includePaths;
	//D3D_SHADER_MACRO macros[64] = { 0 };
	std::vector<D3D_SHADER_MACRO> macros;
	UINT flags = 0;
	char absoluteInputPath[2048]{};

	for (int i = 1; i < argc; i++)
	{
		if (nullptr == argBuffer || nullptr == argBuffer[i])
		{
			continue;
		}

		if (i == argc - 1)
		{
			inputPath = argBuffer[i];
			if (argBuffer[i][0] == '.')
			{
				GetFullPathNameA(inputPath, 2048, absoluteInputPath, NULL);
				inputPath = absoluteInputPath;
			}

 			break;
		}

		bool hasLineSpacing = false;

		if (CompareArgument(argBuffer[i], "/ZI", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_DEBUG;
		}
		else if (CompareArgument(argBuffer[i], "/Vd", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_SKIP_VALIDATION;
		}
		else if (CompareArgument(argBuffer[i], "/Od", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}
		else if (CompareArgument(argBuffer[i], "/Zpr", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
		}
		else if (CompareArgument(argBuffer[i], "/Zpc", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
		}
		else if (CompareArgument(argBuffer[i], "/Gpp", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_PARTIAL_PRECISION;
		}
		else if (CompareArgument(argBuffer[i], "/Op", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_NO_PRESHADER;
		}
		else if (CompareArgument(argBuffer[i], "/Gfa", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_AVOID_FLOW_CONTROL;
		}
		else if (CompareArgument(argBuffer[i], "/Ges", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_ENABLE_STRICTNESS;
		}
		else if (CompareArgument(argBuffer[i], "/Gis", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_IEEE_STRICTNESS;
		}
		else if (CompareArgument(argBuffer[i], "/Gec", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
		}
		else if (CompareArgument(argBuffer[i], "/O0", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
		}
		else if (CompareArgument(argBuffer[i], "/O1", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
		}
		else if (CompareArgument(argBuffer[i], "/O2", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
		}
		else if (CompareArgument(argBuffer[i], "/O3", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		}
		else if (CompareArgument(argBuffer[i], "/WX", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
		}
		else if (CompareArgument(argBuffer[i], "/Zss", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
		}
		else if (CompareArgument(argBuffer[i], "/Zsb", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_DEBUG_NAME_FOR_BINARY;
		}
		else if (CompareArgument(argBuffer[i], "/res_may_alias", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_RESOURCES_MAY_ALIAS;
		}
		else if (CompareArgument(argBuffer[i], "/enable_unbounded_descriptor_tables", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
		}
		else if (CompareArgument(argBuffer[i], "/all_resources_bound", &hasLineSpacing))
		{
			flags |= D3DCOMPILE_ALL_RESOURCES_BOUND;
		}
		else if (CompareArgument(argBuffer[i], "/D", &hasLineSpacing))
		{
			char* defineArgument = NULL;
			if (hasLineSpacing)
			{
				if (i + 1 >= argc || NULL == argBuffer[i + 1])
				{
					fprintf(stderr, "you should write Define key and value after /D argument.\n");
					errorCode = INVALID_SHADER_DEFINE;
					goto lb_release;
				}
				else
				{
					defineArgument = argBuffer[i + 1];
				}

				i++;
			}
			else
			{
				defineArgument = &(argBuffer[i][2]);
			}

			int equalIndex = -1;
			size_t defineArgumentLen = strlen(defineArgument);
			for (size_t j = 0; j < defineArgumentLen; j++)
			{
				if (defineArgument[j] == '=')
				{
					equalIndex = j;
					break;
				}
			}

			const char* keyArgument = defineArgument;
			const char* valueArgument = NULL;

			if (equalIndex == -1)
			{
				valueArgument = "1";
			}
			else
			{
				defineArgument[equalIndex] = '\0';
				valueArgument = &(defineArgument[equalIndex + 1]);
			}

			D3D_SHADER_MACRO macro{};

			macro.Name = keyArgument;
			macro.Definition = valueArgument;
			macros.push_back(macro);
		}
		else if (CompareArgument(argBuffer[i], "/T", &hasLineSpacing))
		{
			if (hasLineSpacing)
			{
				if (i + 1 >= argc || nullptr == argBuffer[i + 1])
				{
					fprintf(stderr, "you should write shader profile after /T argument.\n");
					errorCode = INVALID_SHADER_PROFILE;
					goto lb_release;
				}
				else
				{
					shaderProfile = argBuffer[i + 1];
				}

				i++;
			}
			else
			{
				shaderProfile = &(argBuffer[i][2]);
			}
		}
		else if (CompareArgument(argBuffer[i], "/E", &hasLineSpacing))
		{
			if (hasLineSpacing)
			{
				if (i + 1 >= argc || nullptr == argBuffer[i + 1])
				{
					fprintf(stderr, "you should write entry name after /E argument.\n");
					errorCode = INVALID_ENTRY_NAME;
					goto lb_release;
				}
				else
				{
					entryName = argBuffer[i + 1];
				}

				i++;
			}
			else
			{
				entryName = &(argBuffer[i][2]);
			}
		}
		else if (CompareArgument(argBuffer[i], "/I", &hasLineSpacing))
		{
			char* includePath = nullptr;
			if (hasLineSpacing)
			{
				if (i + 1 >= argc || nullptr == argBuffer[i + 1])
				{
					fprintf(stderr, "you should write include path after /I argument.\n");
					errorCode = INVALID_INCLUDE_PATH;
					goto lb_release;
				}
				else
				{
					includePath = argBuffer[i + 1];
				}

				i++;
			}
			else
			{
				includePath = &(argBuffer[i][2]);
			}
			size_t pathLen = strlen(includePath);
			if (includePath[pathLen - 1] != '/' && includePath[pathLen - 1] != '\\')
			{
				includePath[pathLen] = '/';
				includePath[pathLen + 1] = '\0';
			}

			includePaths.push_back(includePath);
		}
		else if (CompareArgument(argBuffer[i], "/Fo", &hasLineSpacing))
		{
			if (hasLineSpacing)
			{
				if (i + 1 >= argc || nullptr == argBuffer[i + 1])
				{
					fprintf(stderr, "you should write output path after /Fo argument.\n");
					errorCode = INVALID_OUTPUT_PATH;
					goto lb_release;
				}
				else
				{
					outputPath = argBuffer[i + 1];
				}

				i++;
			}
			else
			{
				outputPath = &(argBuffer[i][3]);
			}
		}
		else
		{
			fprintf(stdout, "%s argument is not supported.\n", argBuffer[i]);
			errorCode = INVALID_ARGUMENT;
			goto lb_release;
		}
	}

	fopen_s(&fp, inputPath, "rb");
	if (nullptr == fp)
	{
		fprintf(stderr, "invalid input hlsl file path. you should write input hlsl file path into last argument.\n");
		fprintf(stderr, "NOTE. your input file path is : %s\n", inputPath);
		errorCode = INVALID_INPUT_PATH;
		goto lb_release;
	}

	includeHandler = new IncludeHandler(includePaths);

	fseek(fp, 0, SEEK_END);
	hlslCodeSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	hlslCode = new char[hlslCodeSize];
	fread(hlslCode, 1, hlslCodeSize, fp);

	fclose(fp);

	fp = nullptr;

	hr = D3DCompile(
		hlslCode, 
		hlslCodeSize, 
		NULL, 
		macros.data(),
		includeHandler,
		entryName, 
		shaderProfile, 
		flags, 
		0, 
		&compiledShader,
		&compilationErrors
	);

	if (FAILED(hr))
	{
		if (compilationErrors)
		{
			fprintf(stderr, "Compilation failed with errors: \n\t%s\n", (char*)compilationErrors->GetBufferPointer());
			compilationErrors->Release();
		}
		else
		{
			fprintf(stderr, "Compilation failed with HRESULT: 0x%08X\n", hr);
		}

		errorCode = SHADER_COMPILE_FAILED;
	}

	if (nullptr != outputPath)
	{
		fopen_s(&fp, outputPath, "wb");
	}
	if (nullptr == fp)
	{
		fprintf(stderr, "failed to create output file.");
		errorCode = INVALID_OUTPUT_PATH;
		goto lb_release;
	}

	fwrite(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), 1, fp);
	fclose(fp);

	fprintf(stdout, "compilation success.\n");

lb_release:
	delete includeHandler;
	for (int i = 0; i < argc; i++)
	{
		delete[] argBuffer[i];
	}
	delete[] argBuffer;

	printf("Thank you!\n");

	return errorCode;
}
