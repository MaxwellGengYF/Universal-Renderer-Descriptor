#pragma once
#include <stb/stb_image.h>
template<typename T>
struct StbData {
	T ptr;
	StbData(T ptr) : ptr(ptr) {}
	~StbData() {
		stbi_image_free(ptr);
	}
};