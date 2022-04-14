
#include <Common/vstring.h>
#include <Common/Pool.h>
#include <mutex>
#include <Common/functional.h>
#include <Common/Memory.h>
#include <Common/vector.h>
#include <Common/DynamicDLL.h>
#include <Common/MetaLib.h>
//#include "BinaryLinkedAllocator.h"
#include <Common/LinkedList.h>
#include <Common/VAllocator.h>
#include <Common/DXMath/DXMath.h>
#include <Common/TreeMap.h>
#include <Common/Type.h>
namespace v_mimalloc {
vstd::funcPtr_t<void*(size_t)> mallocFunc = nullptr;
vstd::funcPtr_t<void(void*)> freeFunc = nullptr;
vstd::funcPtr_t<void*(void*, size_t)> reallocFunc = nullptr;
struct MAllocator {
	vstd::optional<DynamicDLL> dll;
	MAllocator(char const* path) {
		dll.New(path);
		dll->GetDLLFunc(mallocFunc, "mi_malloc");
		dll->GetDLLFunc(freeFunc, "mi_free");
		dll->GetDLLFunc(reallocFunc, "mi_realloc");
	}
	static char const* ReadFile() {
		static char dd[4096];
		static char const* dllName = "mimalloc.dll";
		static size_t dllNameLen = strlen(dllName);
		dd[0] = 0;
		auto handle = fopen("dll_path", "rb");
		auto disp = vstd::create_disposer([&]() {
			if (handle != nullptr)
				fclose(handle);
		});
		size_t sz = 0;
		if (handle != nullptr) {
			fseek(handle, 0L, SEEK_END);
			sz = ftell(handle);
			sz = std::min(sz, (size_t)4095 - dllNameLen);
			if (sz > 0) {
				fseek(handle, 0L, SEEK_SET);
				fread(dd, sz, 1, handle);
			}
		}
		memcpy(dd + sz, dllName, dllNameLen);
		dd[sz + dllNameLen] = 0;
		return dd;
	}
	MAllocator() : MAllocator(ReadFile()) {
	}
	~MAllocator() {
		mallocFunc = [](size_t) -> void* { return nullptr; };
		freeFunc = [](void* ptr) {};
	}
};
static MAllocator vengine_malloc_dll;
}// namespace v_mimalloc

VENGINE_C_FUNC_COMMON vstd::funcPtr_t<void(void*)> vengine_get_malloc() {
	return free;
}

VENGINE_C_FUNC_COMMON void* vengine_default_malloc(size_t sz) {
	using namespace v_mimalloc;
	return mallocFunc(sz);
}
VENGINE_C_FUNC_COMMON void vengine_default_free(void* ptr) {
	using namespace v_mimalloc;
	freeFunc(ptr);
}

VENGINE_C_FUNC_COMMON void* vengine_default_realloc(void* ptr, size_t size) {
	using namespace v_mimalloc;
	return reallocFunc(ptr, size);
}

VENGINE_C_FUNC_COMMON void* vengine_malloc(size_t size) {
	using namespace v_mimalloc;
	return mallocFunc(size);
}
VENGINE_C_FUNC_COMMON void vengine_free(void* ptr) {
	using namespace v_mimalloc;
	freeFunc(ptr);
}
VENGINE_C_FUNC_COMMON void* vengine_realloc(void* ptr, size_t size) {
	using namespace v_mimalloc;
	return reallocFunc(ptr, size);
}
namespace vstd {
namespace detail {

struct Node {
	bool color;	 // 1 -> Red, 0 -> Black
	Node* parent;// pointer to the parent
	Node* left;	 // pointer to left child
	Node* right; // pointer to right child
};

void leftRotate(void* vx, void*& root) {
	Node* x = reinterpret_cast<Node*>(vx);
	Node* y = x->right;
	x->right = y->left;
	if (y->left != nullptr) {
		y->left->parent = x;
	}
	y->parent = x->parent;
	if (x->parent == nullptr) {
		root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left = x;
	x->parent = y;
}

// rotate right at node x
void rightRotate(void* vx, void*& root) {
	Node* x = reinterpret_cast<Node*>(vx);
	Node* y = x->left;
	x->left = y->right;
	if (y->right != nullptr) {
		y->right->parent = x;
	}
	y->parent = x->parent;
	if (x->parent == nullptr) {
		root = y;
	} else if (x == x->parent->right) {
		x->parent->right = y;
	} else {
		x->parent->left = y;
	}
	y->right = x;
	x->parent = y;
}
void fixDelete(void* vptr, void*& vRoot, Node*& tNullParent) {
	Node* x = reinterpret_cast<Node*>(vptr);
	Node* root = reinterpret_cast<Node*>(vRoot);
	Node* s;
	bool xIsNull;
	while (x != root && ((xIsNull = (x == nullptr)) || x->color == 0)) {
		auto&& xParent = xIsNull ? *reinterpret_cast<Node**>(&tNullParent) : x->parent;
		if (x == xParent->left) {
			s = xParent->right;
			if (s->color == 1) {
				// case 3.1
				s->color = 0;
				xParent->color = 1;
				leftRotate(xParent, vRoot);
				s = xParent->right;
			}
			bool leftNull = s->left == nullptr || s->left->color == 0;
			bool rightNull = s->right == nullptr || s->right->color == 0;
			if (leftNull && rightNull) {
				// case 3.2
				s->color = 1;
				x = xParent;
			} else {
				if (rightNull) {
					// case 3.3
					s->left->color = 0;
					s->color = 1;
					rightRotate(s, vRoot);
					s = xParent->right;
				}

				// case 3.4
				s->color = xParent->color;
				xParent->color = 0;
				s->right->color = 0;
				leftRotate(xParent, vRoot);
				x = root;
			}
		} else {
			s = xParent->left;
			if (s->color == 1) {
				// case 3.1
				s->color = 0;
				xParent->color = 1;
				rightRotate(xParent, vRoot);
				s = xParent->left;
			}
			bool leftNull = s->left == nullptr || s->left->color == 0;
			bool rightNull = s->right == nullptr || s->right->color == 0;
			if (leftNull && rightNull) {
				// case 3.2
				s->color = 1;
				x = xParent;
			} else {
				if (leftNull) {
					// case 3.3
					s->right->color = 0;
					s->color = 1;
					leftRotate(s, vRoot);
					s = xParent->left;
				}

				// case 3.4
				s->color = xParent->color;
				xParent->color = 0;
				s->left->color = 0;
				rightRotate(xParent, vRoot);
				x = root;
			}
		}
	}
	if (x != nullptr)
		x->color = 0;
}

void TreeMapUtility::fixInsert(void* vk, void*& vRoot) {
	Node* k = reinterpret_cast<Node*>(vk);
	Node* u;
	while (k->parent->color == 1) {
		if (k->parent == k->parent->parent->right) {
			u = k->parent->parent->left;// uncle
			if (u != nullptr && u->color == 1) {
				// case 3.1
				u->color = 0;
				k->parent->color = 0;
				k->parent->parent->color = 1;
				k = k->parent->parent;
			} else {
				if (k == k->parent->left) {
					// case 3.2.2
					k = k->parent;
					rightRotate(k, vRoot);
				}
				// case 3.2.1
				k->parent->color = 0;
				k->parent->parent->color = 1;
				leftRotate(k->parent->parent, vRoot);
			}
		} else {
			u = k->parent->parent->right;// uncle

			if (u != nullptr && u->color == 1) {
				// mirror case 3.1
				u->color = 0;
				k->parent->color = 0;
				k->parent->parent->color = 1;
				k = k->parent->parent;
			} else {
				if (k == k->parent->right) {
					// mirror case 3.2.2
					k = k->parent;
					leftRotate(k, vRoot);
				}
				// mirror case 3.2.1
				k->parent->color = 0;
				k->parent->parent->color = 1;
				rightRotate(k->parent->parent, vRoot);
			}
		}
		if (k == vRoot) {
			break;
		}
	}
	reinterpret_cast<Node*>(vRoot)->color = 0;
}
Node* minimum(Node* node) {
	while (node->left != nullptr) {
		node = node->left;
	}
	return node;
}

// find the node with the maximum key
Node* maximum(Node* node) {
	while (node->right != nullptr) {
		node = node->right;
	}
	return node;
}

void rbTransplant(Node* u, Node* v, void*& root, Node*& tNullParent) {
	if (u->parent == nullptr) {
		root = v;
	} else if (u == u->parent->left) {
		u->parent->left = v;
	} else {
		u->parent->right = v;
	}
	if (v == nullptr)
		tNullParent = u->parent;
	else
		v->parent = u->parent;
}

void TreeMapUtility::deleteOneNode(void* vz, void*& root) {
	Node* tNullParent = nullptr;
	Node* z = reinterpret_cast<Node*>(vz);
	Node* x;
	Node* y;
	y = z;
	int y_original_color = y->color;
	if (z->left == nullptr) {
		x = z->right;
		rbTransplant(z, z->right, root, tNullParent);
	} else if (z->right == nullptr) {
		x = z->left;
		rbTransplant(z, z->left, root, tNullParent);
	} else {
		y = minimum(z->right);
		y_original_color = y->color;
		x = y->right;
		if (y->parent == z) {
			if (x)
				x->parent = y;
			else
				tNullParent = y;
		} else {
			rbTransplant(y, y->right, root, tNullParent);
			y->right = z->right;
			y->right->parent = y;
		}

		rbTransplant(z, y, root, tNullParent);
		y->left = z->left;
		y->left->parent = y;
		y->color = z->color;
	}
	if (y_original_color == 0) {
		fixDelete(x, root, tNullParent);
	}
}

void* TreeMapUtility::getNext(void* vptr) {
	Node* ptr = reinterpret_cast<Node*>(vptr);
	if (ptr->right == nullptr) {
		Node* pNode;
		while (((pNode = ptr->parent) != nullptr) && (ptr == pNode->right)) {
			ptr = pNode;
		}
		ptr = pNode;
	} else {
		ptr = minimum(ptr->right);
	}
	return ptr;
}
void* TreeMapUtility::getLast(void* vptr) {
	Node* ptr = reinterpret_cast<Node*>(vptr);
	if (ptr->left == nullptr) {
		Node* pNode;
		while (((pNode = ptr->parent) != nullptr) && (ptr == pNode->left)) {
			ptr = pNode;
		}
		if (ptr != nullptr) {
			ptr = pNode;
		}
	} else {
		ptr = maximum(ptr->left);
	}
	return ptr;
}

}
}// namespace vstd::detail
#include <Common/Log.h>
#include <Windows.h>
DynamicDLL::DynamicDLL(char const* name) {
	inst = reinterpret_cast<size_t>(LoadLibraryA(name));
	if (inst == 0) {
		VEngine_Log(
			{"Can not find DLL ",
			 name});
		VENGINE_EXIT;
	}
}
DynamicDLL::~DynamicDLL() {
	if (inst != 0)
		FreeLibrary(reinterpret_cast<HINSTANCE>(inst));
}

DynamicDLL::DynamicDLL(DynamicDLL&& d) {
	inst = d.inst;
	d.inst = 0;
}

size_t DynamicDLL::GetFuncPtr(char const* name) const {
	return reinterpret_cast<size_t>(GetProcAddress(reinterpret_cast<HINSTANCE>(inst), name));
}

vstd::string_view operator"" sv(char const* str, size_t sz) {
	return vstd::string_view(str, sz);
}

#ifdef EXPORT_UNITY_FUNCTION
VENGINE_UNITY_EXTERN void vengine_memcpy(void* dest, void* src, uint64 sz) {
	memcpy(dest, src, sz);
}
VENGINE_UNITY_EXTERN void vengine_memset(void* dest, byte b, uint64 sz) {
	memset(dest, b, sz);
}
VENGINE_UNITY_EXTERN void vengine_memmove(void* dest, void* src, uint64 sz) {
	memmove(dest, src, sz);
}
#endif
///////////////////////CPP
namespace vstd {
inline bool UCS4ToUTF8(uint32_t c, char*& pResult) {
	if (c < 0x00000080)
		*pResult++ = (char)(uint8_t)c;
	else if (c < 0x0800) {
		*pResult++ = (char)(uint8_t)(0xC0 | (c >> 6));
		*pResult++ = (char)(uint8_t)(0x80 | (c & 0x3F));
	} else if (c <= 0x0000FFFF) {
		*pResult++ = (char)(uint8_t)(0xE0 | (c >> 12));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 6) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | (c & 0x3F));
	} else if (c <= 0x001FFFFF) {
		*pResult++ = (char)(uint8_t)(0xF0 | (c >> 18));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 12) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 6) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | (c & 0x3F));
	} else if (c <= 0x003FFFFFF) {
		*pResult++ = (char)(uint8_t)(0xF8 | (c >> 24));
		*pResult++ = (char)(uint8_t)(0x80 | (c >> 18));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 12) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 6) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | (c & 0x3F));
	} else if (c <= 0x7FFFFFFF) {
		*pResult++ = (char)(uint8_t)(0xFC | (c >> 30));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 24) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 18) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 12) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | ((c >> 6) & 0x3F));
		*pResult++ = (char)(uint8_t)(0x80 | (c & 0x3F));
	} else {
		// values >= 0x80000000 can't be converted to UTF8.
		*pResult++ = '\1';
		return false;
	}

	return true;
}

// Requires that pResult have a capacity of at least 3 chars.
// Sets pResult to '\1' in the case that c is an invalid UCS4 char.
inline bool UCS2ToUTF8(uint16_t c, char*& pResult) {
	return UCS4ToUTF8(c, pResult);
}

// Sets result to 0xffff in the case that the input UTF8 sequence is bad.
// 32 bit 0xffffffff is an invalid UCS4 code point, so we can't use that as an error return value.
inline bool UTF8ToUCS4(const char*& p, const char* pEnd, uint32_t& result) {
	// This could likely be implemented in a faster-executing way that uses tables.

	bool success = true;
	uint32_t c = 0xffff;
	const char* pNext = NULL;

	if (p < pEnd) {
		uint8_t cChar0((uint8_t)*p), cChar1, cChar2, cChar3;

		// Asserts are disabled because we don't necessarily want to interrupt runtime execution due to this.
		// assert((cChar0 != 0xFE) && (cChar0 != 0xFF));     //  No byte can be 0xFE or 0xFF
		// Code below will effectively catch this error as it goes.

		if (cChar0 < 0x80) {
			pNext = p + 1;
			c = cChar0;
		} else {
			//assert((cChar0 & 0xC0) == 0xC0);              //  The top two bits need to be equal to 1
			if ((cChar0 & 0xC0) != 0xC0) {
				success = false;
				goto Failure;
			}

			if ((cChar0 & 0xE0) == 0xC0) {
				pNext = p + 2;

				if (pNext <= pEnd) {
					c = (uint32_t)((cChar0 & 0x1F) << 6);
					cChar1 = static_cast<uint8_t>(p[1]);
					c |= cChar1 & 0x3F;

					//assert((cChar1 & 0xC0) == 0x80);          //  All subsequent code should be b10xxxxxx
					//assert(c >= 0x0080 && c < 0x0800);        //  Check that we have the smallest coding
					if (!((cChar1 & 0xC0) == 0x80) || !(c >= 0x0080 && c < 0x0800)) {
						success = false;
						goto Failure;
					}
				} else {
					success = false;
					goto Failure;
				}
			} else if ((cChar0 & 0xF0) == 0xE0) {
				pNext = p + 3;

				if (pNext <= pEnd) {
					c = (uint32_t)((cChar0 & 0xF) << 12);
					cChar1 = static_cast<uint8_t>(p[1]);
					c |= (cChar1 & 0x3F) << 6;
					cChar2 = static_cast<uint8_t>(p[2]);
					c |= cChar2 & 0x3F;

					//assert((cChar1 & 0xC0) == 0x80);            //  All subsequent code should be b10xxxxxx
					//assert((cChar2 & 0xC0) == 0x80);            //  All subsequent code should be b10xxxxxx
					//assert(c >= 0x00000800 && c <  0x00010000); //  Check that we have the smallest coding
					if (!((cChar1 & 0xC0) == 0x80) || !((cChar2 & 0xC0) == 0x80) || !(c >= 0x00000800 && c < 0x00010000)) {
						success = false;
						goto Failure;
					}
				} else {
					success = false;
					goto Failure;
				}
			} else if ((cChar0 & 0xF8) == 0xF0) {
				pNext = p + 4;

				if (pNext <= pEnd) {
					c = (uint32_t)((cChar0 & 0x7) << 18);
					cChar1 = static_cast<uint8_t>(p[1]);
					c |= (uint32_t)((cChar1 & 0x3F) << 12);
					cChar2 = static_cast<uint8_t>(p[2]);
					c |= (cChar2 & 0x3F) << 6;
					cChar3 = static_cast<uint8_t>(p[3]);
					c |= cChar3 & 0x3F;

					//assert((cChar0 & 0xf8) == 0xf0);            //  We handle the unicode but not UCS-4
					//assert((cChar1 & 0xC0) == 0x80);            //  All subsequent code should be b10xxxxxx
					//assert((cChar2 & 0xC0) == 0x80);            //  All subsequent code should be b10xxxxxx
					//assert((cChar3 & 0xC0) == 0x80);            //  All subsequent code should be b10xxxxxx
					//assert(c >= 0x00010000 && c <= 0x0010FFFF); //  Check that we have the smallest coding, Unicode and not ucs-4
					if (!((cChar0 & 0xf8) == 0xf0) || !((cChar1 & 0xC0) == 0x80) || !((cChar2 & 0xC0) == 0x80) || !(c >= 0x00010000 && c <= 0x0010FFFF)) {
						success = false;
						goto Failure;
					}
				} else {
					success = false;
					goto Failure;
				}
			} else if ((cChar0 & 0xFC) == 0xF8) {
				pNext = p + 4;

				if (pNext <= pEnd) {
					// To do. We don't currently support extended UCS4 characters.
				} else {
					success = false;
					goto Failure;
				}
			} else if ((cChar0 & 0xFE) == 0xFC) {
				pNext = p + 5;

				if (pNext <= pEnd) {
					// To do. We don't currently support extended UCS4 characters.
				} else {
					success = false;
					goto Failure;
				}
			} else {
				success = false;
				goto Failure;
			}
		}
	} else
		success = false;

Failure:
	if (success) {
		p = pNext;
		result = c;
	} else {
		p = p + 1;
		result = 0xffff;
	}

	return success;
}

// Sets result to 0xffff in the case that the input UTF8 sequence is bad.
// The effect of converting UTF8 codepoints > 0xffff to UCS2 (char16_t) is to set all
// such codepoints to 0xffff. EASTL doesn't have a concept of setting or maintaining
// error state for string conversions, though it does have a policy of converting
// impossible values to something without generating invalid strings or throwing exceptions.
inline bool UTF8ToUCS2(const char*& p, const char* pEnd, uint16_t& result) {
	uint32_t u32;

	if (UTF8ToUCS4(p, pEnd, u32)) {
		if (u32 <= 0xffff) {
			result = (uint16_t)u32;
			return true;
		}
	}

	result = 0xffff;
	return false;
}

///////////////////////////////////////////////////////////////////////////
// DecodePart
///////////////////////////////////////////////////////////////////////////

VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char*& pDest, char* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		sourceSize = destSize;

	memmove(pDest, pSrc, sourceSize * sizeof(*pSrcEnd));

	pSrc += sourceSize;
	pDest += sourceSize;// Intentionally add sourceSize here.

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
	bool success = true;

	while (success && (pSrc < pSrcEnd) && (pDest < pDestEnd))
		success = UTF8ToUCS2(pSrc, pSrcEnd, (uint16_t&)*pDest++);

	return success;
}

VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
	bool success = true;

	while (success && (pSrc < pSrcEnd) && (pDest < pDestEnd))
		success = UTF8ToUCS4(pSrc, pSrcEnd, (uint32_t&)*pDest++);

	return success;
}

VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char*& pDest, char* pDestEnd) {
	bool success = true;

	assert((pDest + 6) < pDestEnd);// The user must provide ample buffer space, preferably 256 chars or more.
	pDestEnd -= 6;				   // Do this so that we can avoid dest buffer size checking in the loop below and the function it calls.

	while (success && (pSrc < pSrcEnd) && (pDest < pDestEnd))
		success = UCS2ToUTF8(*pSrc++, pDest);

	return success;
}

VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		sourceSize = destSize;

	memmove(pDest, pSrc, sourceSize * sizeof(*pSrcEnd));

	pSrc += sourceSize;
	pDest += sourceSize;// Intentionally add sourceSize here.

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		pSrcEnd = pSrc + destSize;

	while (pSrc != pSrcEnd)// To consider: Improve this by unrolling this loop. Other tricks can improve its speed as well.
		*pDest++ = (char32_t)*pSrc++;

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char*& pDest, char* pDestEnd) {
	bool success = true;

	assert((pDest + 6) < pDestEnd);// The user must provide ample buffer space, preferably 256 chars or more.
	pDestEnd -= 6;				   // Do this so that we can avoid dest buffer size checking in the loop below and the function it calls.

	while (success && (pSrc < pSrcEnd) && (pDest < pDestEnd))
		success = UCS4ToUTF8(*pSrc++, pDest);

	return success;
}

VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		pSrcEnd = pSrc + destSize;

	while (pSrc != pSrcEnd)			 // To consider: Improve this by unrolling this loop. Other tricks can improve its speed as well.
		*pDest++ = (char16_t)*pSrc++;// This is potentially losing data. We are not converting to UTF16; we are converting to UCS2.

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		sourceSize = destSize;

	memmove(pDest, pSrc, sourceSize * sizeof(*pSrcEnd));

	pSrc += sourceSize;
	pDest += sourceSize;// Intentionally add sourceSize here.

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char*& pDest, char* pDestEnd) {
	bool success = true;

	assert((pDest + 6) < pDestEnd);// The user must provide ample buffer space, preferably 256 chars or more.
	pDestEnd -= 6;				   // Do this so that we can avoid dest buffer size checking in the loop below and the function it calls.

	while (success && (pSrc < pSrcEnd) && (pDest < pDestEnd))
		success = UCS4ToUTF8((uint32_t)(unsigned)*pSrc++, pDest);

	return success;
}

VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		pSrcEnd = pSrc + destSize;

	while (pSrc != pSrcEnd)			 // To consider: Improve this by unrolling this loop. Other tricks can improve its speed as well.
		*pDest++ = (char16_t)*pSrc++;// This is potentially losing data. We are not converting to UTF16; we are converting to UCS2.

	return true;
}

VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
	size_t sourceSize = (size_t)(pSrcEnd - pSrc);
	size_t destSize = (size_t)(pDestEnd - pDest);

	if (sourceSize > destSize)
		pSrcEnd = pSrc + destSize;

	while (pSrc != pSrcEnd)			 // To consider: Improve this by unrolling this loop. Other tricks can improve its speed as well.
		*pDest++ = (char32_t)*pSrc++;// This is potentially losing data. We are not converting to UTF16; we are converting to UCS2.

	return true;
}
bool Type::operator==(const Type& t) const noexcept {
	return mName == t.mName;
}
bool Type::operator==(const std::type_info& t) const noexcept {
	return mName == string_view(t.name());
}

int32 Type::Compare(const Type& t) const noexcept {
	return compare<string_view>()(mName, t.mName);
}
int32 Type::Compare(const std::type_info& info) const noexcept {
	return compare<string_view>()(mName, string_view(info.name()));
}
Type::Type(const std::type_info& info) noexcept
	: mName(info.name()) {
	hashValue = hash<string_view>()(mName);
}
Type::Type() noexcept {
}
}// namespace vstd
 ///////////////////////CPP