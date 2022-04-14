#pragma once
#include <VEngineConfig.h>
#include <random>
typedef uint32_t uint;
class VENGINE_DLL_COMMON Random
{
private:
	std::mt19937 eng;
	std::uniform_int_distribution<uint> dist;// (eng.min(), eng.max());
public:
	Random();
	double GetNormFloat();
	uint GetInt();
	double GetRangedFloat(double min, double max);
};