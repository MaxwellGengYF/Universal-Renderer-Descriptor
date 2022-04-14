
#include <stdint.h>
#include <math.h>
using uint = uint32_t;
struct uint2 {
	uint x, y;
	uint2(uint x, uint y)
		: x(x), y(y) {}
};
struct uint4 {
	uint x, y, z, w;
	uint4(uint x, uint y, uint z, uint w)
		: x(x), y(y), z(z), w(w) {}
};
#if defined(_MSC_VER)
#define VENGINE_UNITY_EXTERN extern "C" __declspec(dllexport)
#elif defined(__CYGWIN__)
#define VENGINE_UNITY_EXTERN __attribute__((dllexport))
#elif (defined(__GNUC__) && (__GNUC__ >= 4))
#define VENGINE_UNITY_EXTERN __attribute__((visibility("default")))
#else
#define VENGINE_UNITY_EXTERN
#endif
#define A_CPU
#include <Graphics/AMD_FSR/ffx_a.h>
#include <Graphics/AMD_FSR/ffx_fsr1.h>
namespace AMD_FSR {
struct FSRConstants {
	uint4 Const0;
	uint4 Const1;
	uint4 Const2;
	uint4 Const3;
	uint4 Sample;
};
VENGINE_UNITY_EXTERN void AMD_FsrEasuCon(
	FSRConstants* consts,
	uint srcWidth,
	uint srcHeight,
	uint destWidth,
	uint destHeight) {
	FsrEasuCon(
		reinterpret_cast<AU1*>(&consts->Const0),
		reinterpret_cast<AU1*>(&consts->Const1),
		reinterpret_cast<AU1*>(&consts->Const2),
		reinterpret_cast<AU1*>(&consts->Const3),
		static_cast<AF1>(srcWidth),
		static_cast<AF1>(srcHeight),
		static_cast<AF1>(srcWidth),
		static_cast<AF1>(srcHeight),
		(AF1)destWidth,
		(AF1)destHeight);
}
VENGINE_UNITY_EXTERN void AMD_FsrRcasCon(
	FSRConstants* consts,
	float sharpness) {
	FsrRcasCon(reinterpret_cast<AU1*>(&consts->Const0), sharpness);
}
VENGINE_UNITY_EXTERN void AMD_FsrDispatchCount(
	uint displayWidth,
	uint displayHeight,
	uint2* ptr) {
	constexpr uint threadGroupWorkRegionDim = 16;
	uint dispatchX = (displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	uint dispatchY = (displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	*ptr = uint2(dispatchX, dispatchY);
}
}// namespace AMD_FSR

#undef A_CPU