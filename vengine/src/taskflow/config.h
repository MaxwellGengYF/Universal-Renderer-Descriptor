#pragma once
#ifdef COMMON_DLL_PROJECT
#define TF_API _declspec(dllexport)
#else
#define TF_API _declspec(dllimport)
#endif