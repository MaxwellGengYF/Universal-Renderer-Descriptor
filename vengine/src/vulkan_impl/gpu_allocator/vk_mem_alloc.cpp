#include "vk_mem_alloc.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <Utility>
#include <type_traits>

#ifdef _MSC_VER
    #include <intrin.h> // For functions like __popcnt, _BitScanForward etc.
#endif
#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L // C++20
    #include <bit> // For std::popcount
#endif

/*******************************************************************************
CONFIGURATION SECTION

Define some of these macros before each #include of this header or change them
here if you need other then default behavior depending on your environment.
*/
#ifndef _VMA_CONFIGURATION

/*
Define this macro to 1 to make the library fetch pointers to Vulkan functions
internally, like:

    vulkanFunctions.vkAllocateMemory = &vkAllocateMemory;
*/
#if !defined(VMA_STATIC_VULKAN_FUNCTIONS) && !defined(VK_NO_PROTOTYPES)
    #define VMA_STATIC_VULKAN_FUNCTIONS 1
#endif

/*
Define this macro to 1 to make the library fetch pointers to Vulkan functions
internally, like:

    vulkanFunctions.vkAllocateMemory = (PFN_vkAllocateMemory)vkGetDeviceProcAddr(device, "vkAllocateMemory");

To use this feature in new versions of VMA you now have to pass
VmaVulkanFunctions::vkGetInstanceProcAddr and vkGetDeviceProcAddr as
VmaAllocatorCreateInfo::pVulkanFunctions. Other members can be null.
*/
#if !defined(VMA_DYNAMIC_VULKAN_FUNCTIONS)
    #define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#endif

#ifndef VMA_USE_STL_SHARED_MUTEX
    // Compiler conforms to C++17.
    #if __cplusplus >= 201703L
        #define VMA_USE_STL_SHARED_MUTEX 1
    // Visual studio defines __cplusplus properly only when passed additional parameter: /Zc:__cplusplus
    // Otherwise it is always 199711L, despite shared_mutex works since Visual Studio 2015 Update 2.
    #elif defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 190023918 && __cplusplus == 199711L && _MSVC_LANG >= 201703L
        #define VMA_USE_STL_SHARED_MUTEX 1
    #else
        #define VMA_USE_STL_SHARED_MUTEX 0
    #endif
#endif

/*
Define this macro to include custom header files without having to edit this file directly, e.g.:

    // Inside of "my_vma_configuration_user_includes.h":

    #include "my_custom_assert.h" // for MY_CUSTOM_ASSERT
    #include "my_custom_min.h" // for my_custom_min
    #include <algorithm>
    #include <mutex>

    // Inside a different file, which includes "vk_mem_alloc.h":

    #define VMA_CONFIGURATION_USER_INCLUDES_H "my_vma_configuration_user_includes.h"
    #define VMA_ASSERT(expr) MY_CUSTOM_ASSERT(expr)
    #define VMA_MIN(v1, v2)  (my_custom_min(v1, v2))
    #include "vk_mem_alloc.h"
    ...

The following headers are used in this CONFIGURATION section only, so feel free to
remove them if not needed.
*/
#if !defined(VMA_CONFIGURATION_USER_INCLUDES_H)
    #include <cassert> // for assert
    #include <algorithm> // for min, max
    #include <mutex>
#else
    #include VMA_CONFIGURATION_USER_INCLUDES_H
#endif

#ifndef VMA_NULL
   // Value used as null pointer. Define it to e.g.: nullptr, NULL, 0, (void*)0.
   #define VMA_NULL   nullptr
#endif

#if defined(__ANDROID_API__) && (__ANDROID_API__ < 16)
#include <cstdlib>
static void* vma_aligned_alloc(size_t alignment, size_t size)
{
    // alignment must be >= sizeof(void*)
    if(alignment < sizeof(void*))
    {
        alignment = sizeof(void*);
    }

    return memalign(alignment, size);
}
#elif defined(__APPLE__) || defined(__ANDROID__) || (defined(__linux__) && defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC))
#include <cstdlib>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

static void* vma_aligned_alloc(size_t alignment, size_t size)
{
    // Unfortunately, aligned_alloc causes VMA to crash due to it returning null pointers. (At least under 11.4)
    // Therefore, for now disable this specific exception until a proper solution is found.
    //#if defined(__APPLE__) && (defined(MAC_OS_X_VERSION_10_16) || defined(__IPHONE_14_0))
    //#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_16 || __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    //    // For C++14, usr/include/malloc/_malloc.h declares aligned_alloc()) only
    //    // with the MacOSX11.0 SDK in Xcode 12 (which is what adds
    //    // MAC_OS_X_VERSION_10_16), even though the function is marked
    //    // availabe for 10.15. That is why the preprocessor checks for 10.16 but
    //    // the __builtin_available checks for 10.15.
    //    // People who use C++17 could call aligned_alloc with the 10.15 SDK already.
    //    if (__builtin_available(macOS 10.15, iOS 13, *))
    //        return aligned_alloc(alignment, size);
    //#endif
    //#endif

    // alignment must be >= sizeof(void*)
    if(alignment < sizeof(void*))
    {
        alignment = sizeof(void*);
    }

    void *pointer;
    if(posix_memalign(&pointer, alignment, size) == 0)
        return pointer;
    return VMA_NULL;
}
#elif defined(_WIN32)
static void* vma_aligned_alloc(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
}
#else
static void* vma_aligned_alloc(size_t alignment, size_t size)
{
    return aligned_alloc(alignment, size);
}
#endif

#if defined(_WIN32)
static void vma_aligned_free(void* ptr)
{
    _aligned_free(ptr);
}
#else
static void vma_aligned_free(void* VMA_NULLABLE ptr)
{
    free(ptr);
}
#endif

// If your compiler is not compatible with C++11 and definition of
// aligned_alloc() function is missing, uncommeting following line may help:

//#include <malloc.h>

// Normal assert to check for programmer's errors, especially in Debug configuration.
#ifndef VMA_ASSERT
   #ifdef NDEBUG
       #define VMA_ASSERT(expr)
   #else
       #define VMA_ASSERT(expr)         assert(expr)
   #endif
#endif

// Assert that will be called very often, like inside data structures e.g. operator[].
// Making it non-empty can make program slow.
#ifndef VMA_HEAVY_ASSERT
   #ifdef NDEBUG
       #define VMA_HEAVY_ASSERT(expr)
   #else
       #define VMA_HEAVY_ASSERT(expr)   //VMA_ASSERT(expr)
   #endif
#endif

#ifndef VMA_ALIGN_OF
   #define VMA_ALIGN_OF(type)       (__alignof(type))
#endif

#ifndef VMA_SYSTEM_ALIGNED_MALLOC
   #define VMA_SYSTEM_ALIGNED_MALLOC(size, alignment) vma_aligned_alloc((alignment), (size))
#endif

#ifndef VMA_SYSTEM_ALIGNED_FREE
   // VMA_SYSTEM_FREE is the old name, but might have been defined by the user
   #if defined(VMA_SYSTEM_FREE)
      #define VMA_SYSTEM_ALIGNED_FREE(ptr)     VMA_SYSTEM_FREE(ptr)
   #else
      #define VMA_SYSTEM_ALIGNED_FREE(ptr)     vma_aligned_free(ptr)
    #endif
#endif

#ifndef VMA_COUNT_BITS_SET
    // Returns number of bits set to 1 in (v)
    #define VMA_COUNT_BITS_SET(v) VmaCountBitsSet(v)
#endif

#ifndef VMA_BITSCAN_LSB
    // Scans integer for index of first nonzero value from the Least Significant Bit (LSB). If mask is 0 then returns UINT8_MAX
    #define VMA_BITSCAN_LSB(mask) VmaBitScanLSB(mask)
#endif

#ifndef VMA_BITSCAN_MSB
    // Scans integer for index of first nonzero value from the Most Significant Bit (MSB). If mask is 0 then returns UINT8_MAX
    #define VMA_BITSCAN_MSB(mask) VmaBitScanMSB(mask)
#endif

#ifndef VMA_MIN
   #define VMA_MIN(v1, v2)    ((std::min)((v1), (v2)))
#endif

#ifndef VMA_MAX
   #define VMA_MAX(v1, v2)    ((std::max)((v1), (v2)))
#endif

#ifndef VMA_SWAP
   #define VMA_SWAP(v1, v2)   std::swap((v1), (v2))
#endif

#ifndef VMA_SORT
   #define VMA_SORT(beg, end, cmp)  std::sort(beg, end, cmp)
#endif

#ifndef VMA_DEBUG_LOG
   #define VMA_DEBUG_LOG(format, ...)
   /*
   #define VMA_DEBUG_LOG(format, ...) do { \
       printf(format, __VA_ARGS__); \
       printf("\n"); \
   } while(false)
   */
#endif

// Define this macro to 1 to enable functions: vmaBuildStatsString, vmaFreeStatsString.
#if VMA_STATS_STRING_ENABLED
    static inline void VmaUint32ToStr(char* VMA_NOT_NULL outStr, size_t strLen, uint32_t num)
    {
        snprintf(outStr, strLen, "%u", static_cast<unsigned int>(num));
    }
    static inline void VmaUint64ToStr(char* VMA_NOT_NULL outStr, size_t strLen, uint64_t num)
    {
        snprintf(outStr, strLen, "%llu", static_cast<unsigned long long>(num));
    }
    static inline void VmaPtrToStr(char* VMA_NOT_NULL outStr, size_t strLen, const void* ptr)
    {
        snprintf(outStr, strLen, "%p", ptr);
    }
#endif

#ifndef VMA_MUTEX
    class VmaMutex
    {
    public:
        void Lock() { m_Mutex.lock(); }
        void Unlock() { m_Mutex.unlock(); }
        bool TryLock() { return m_Mutex.try_lock(); }
    private:
        std::mutex m_Mutex;
    };
    #define VMA_MUTEX VmaMutex
#endif

// Read-write mutex, where "read" is shared access, "write" is exclusive access.
#ifndef VMA_RW_MUTEX
    #if VMA_USE_STL_SHARED_MUTEX
        // Use std::shared_mutex from C++17.
        #include <shared_mutex>
        class VmaRWMutex
        {
        public:
            void LockRead() { m_Mutex.lock_shared(); }
            void UnlockRead() { m_Mutex.unlock_shared(); }
            bool TryLockRead() { return m_Mutex.try_lock_shared(); }
            void LockWrite() { m_Mutex.lock(); }
            void UnlockWrite() { m_Mutex.unlock(); }
            bool TryLockWrite() { return m_Mutex.try_lock(); }
        private:
            std::shared_mutex m_Mutex;
        };
        #define VMA_RW_MUTEX VmaRWMutex
    #elif defined(_WIN32) && defined(WINVER) && WINVER >= 0x0600
        // Use SRWLOCK from WinAPI.
        // Minimum supported client = Windows Vista, server = Windows Server 2008.
        class VmaRWMutex
        {
        public:
            VmaRWMutex() { InitializeSRWLock(&m_Lock); }
            void LockRead() { AcquireSRWLockShared(&m_Lock); }
            void UnlockRead() { ReleaseSRWLockShared(&m_Lock); }
            bool TryLockRead() { return TryAcquireSRWLockShared(&m_Lock) != FALSE; }
            void LockWrite() { AcquireSRWLockExclusive(&m_Lock); }
            void UnlockWrite() { ReleaseSRWLockExclusive(&m_Lock); }
            bool TryLockWrite() { return TryAcquireSRWLockExclusive(&m_Lock) != FALSE; }
        private:
            SRWLOCK m_Lock;
        };
        #define VMA_RW_MUTEX VmaRWMutex
    #else
        // Less efficient fallback: Use normal mutex.
        class VmaRWMutex
        {
        public:
            void LockRead() { m_Mutex.Lock(); }
            void UnlockRead() { m_Mutex.Unlock(); }
            bool TryLockRead() { return m_Mutex.TryLock(); }
            void LockWrite() { m_Mutex.Lock(); }
            void UnlockWrite() { m_Mutex.Unlock(); }
            bool TryLockWrite() { return m_Mutex.TryLock(); }
        private:
            VMA_MUTEX m_Mutex;
        };
        #define VMA_RW_MUTEX VmaRWMutex
    #endif // #if VMA_USE_STL_SHARED_MUTEX
#endif // #ifndef VMA_RW_MUTEX

/*
If providing your own implementation, you need to implement a subset of std::atomic.
*/
#ifndef VMA_ATOMIC_UINT32
    #include <atomic>
    #define VMA_ATOMIC_UINT32 std::atomic<uint32_t>
#endif

#ifndef VMA_ATOMIC_UINT64
    #include <atomic>
    #define VMA_ATOMIC_UINT64 std::atomic<uint64_t>
#endif

#ifndef VMA_DEBUG_ALWAYS_DEDICATED_MEMORY
    /**
    Every allocation will have its own memory block.
    Define to 1 for debugging purposes only.
    */
    #define VMA_DEBUG_ALWAYS_DEDICATED_MEMORY (0)
#endif

#ifndef VMA_MIN_ALIGNMENT
    /**
    Minimum alignment of all allocations, in bytes.
    Set to more than 1 for debugging purposes. Must be power of two.
    */
    #ifdef VMA_DEBUG_ALIGNMENT // Old name
        #define VMA_MIN_ALIGNMENT VMA_DEBUG_ALIGNMENT
    #else
        #define VMA_MIN_ALIGNMENT (1)
    #endif
#endif

#ifndef VMA_DEBUG_MARGIN
    /**
    Minimum margin after every allocation, in bytes.
    Set nonzero for debugging purposes only.
    */
    #define VMA_DEBUG_MARGIN (0)
#endif

#ifndef VMA_DEBUG_INITIALIZE_ALLOCATIONS
    /**
    Define this macro to 1 to automatically fill new allocations and destroyed
    allocations with some bit pattern.
    */
    #define VMA_DEBUG_INITIALIZE_ALLOCATIONS (0)
#endif

#ifndef VMA_DEBUG_DETECT_CORRUPTION
    /**
    Define this macro to 1 together with non-zero value of VMA_DEBUG_MARGIN to
    enable writing magic value to the margin after every allocation and
    validating it, so that memory corruptions (out-of-bounds writes) are detected.
    */
    #define VMA_DEBUG_DETECT_CORRUPTION (0)
#endif

#ifndef VMA_DEBUG_GLOBAL_MUTEX
    /**
    Set this to 1 for debugging purposes only, to enable single mutex protecting all
    entry calls to the library. Can be useful for debugging multithreading issues.
    */
    #define VMA_DEBUG_GLOBAL_MUTEX (0)
#endif

#ifndef VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY
    /**
    Minimum value for VkPhysicalDeviceLimits::bufferImageGranularity.
    Set to more than 1 for debugging purposes only. Must be power of two.
    */
    #define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY (1)
#endif

#ifndef VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT
    /*
    Set this to 1 to make VMA never exceed VkPhysicalDeviceLimits::maxMemoryAllocationCount
    and return error instead of leaving up to Vulkan implementation what to do in such cases.
    */
    #define VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT (0)
#endif

#ifndef VMA_SMALL_HEAP_MAX_SIZE
   /// Maximum size of a memory heap in Vulkan to consider it "small".
   #define VMA_SMALL_HEAP_MAX_SIZE (1024ull * 1024 * 1024)
#endif

#ifndef VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE
   /// Default size of a block allocated as single VkDeviceMemory from a "large" heap.
   #define VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE (256ull * 1024 * 1024)
#endif

/*
Mapping hysteresis is a logic that launches when vmaMapMemory/vmaUnmapMemory is called
or a persistently mapped allocation is created and destroyed several times in a row.
It keeps additional +1 mapping of a device memory block to prevent calling actual
vkMapMemory/vkUnmapMemory too many times, which may improve performance and help
tools like RenderDOc.
*/
#ifndef VMA_MAPPING_HYSTERESIS_ENABLED
    #define VMA_MAPPING_HYSTERESIS_ENABLED 1
#endif

#ifndef VMA_CLASS_NO_COPY
    #define VMA_CLASS_NO_COPY(className) \
        private: \
            className(const className&) = delete; \
            className& operator=(const className&) = delete;
#endif

#define VMA_VALIDATE(cond) do { if(!(cond)) { \
        VMA_ASSERT(0 && "Validation failed: " #cond); \
        return false; \
    } } while(false)

/*******************************************************************************
END OF CONFIGURATION
*/
#endif // _VMA_CONFIGURATION


static const uint8_t VMA_ALLOCATION_FILL_PATTERN_CREATED = 0xDC;
static const uint8_t VMA_ALLOCATION_FILL_PATTERN_DESTROYED = 0xEF;
// Decimal 2139416166, float NaN, little-endian binary 66 E6 84 7F.
static const uint32_t VMA_CORRUPTION_DETECTION_MAGIC_VALUE = 0x7F84E666;

// Copy of some Vulkan definitions so we don't need to check their existence just to handle few constants.
static const uint32_t VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY = 0x00000040;
static const uint32_t VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY = 0x00000080;
static const uint32_t VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_COPY = 0x00020000;
static const uint32_t VK_IMAGE_CREATE_DISJOINT_BIT_COPY = 0x00000200;
static const int32_t VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT_COPY = 1000158000;
static const uint32_t VMA_ALLOCATION_INTERNAL_STRATEGY_MIN_OFFSET = 0x10000000u;
static const uint32_t VMA_ALLOCATION_TRY_COUNT = 32;
static const uint32_t VMA_VENDOR_ID_AMD = 4098;

// This one is tricky. Vulkan specification defines this code as available since
// Vulkan 1.0, but doesn't actually define it in Vulkan SDK earlier than 1.2.131.
// See pull request #207.
#define VK_ERROR_UNKNOWN_COPY ((VkResult)-13)


#if VMA_STATS_STRING_ENABLED
// Correspond to values of enum VmaSuballocationType.
static const char* VMA_SUBALLOCATION_TYPE_NAMES[] =
{
    "FREE",
    "UNKNOWN",
    "BUFFER",
    "IMAGE_UNKNOWN",
    "IMAGE_LINEAR",
    "IMAGE_OPTIMAL",
};
#endif

static VkAllocationCallbacks VmaEmptyAllocationCallbacks =
    { VMA_NULL, VMA_NULL, VMA_NULL, VMA_NULL, VMA_NULL, VMA_NULL };


#ifndef _VMA_ENUM_DECLARATIONS

enum VmaSuballocationType
{
    VMA_SUBALLOCATION_TYPE_FREE = 0,
    VMA_SUBALLOCATION_TYPE_UNKNOWN = 1,
    VMA_SUBALLOCATION_TYPE_BUFFER = 2,
    VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN = 3,
    VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR = 4,
    VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL = 5,
    VMA_SUBALLOCATION_TYPE_MAX_ENUM = 0x7FFFFFFF
};

enum VMA_CACHE_OPERATION
{
    VMA_CACHE_FLUSH,
    VMA_CACHE_INVALIDATE
};

enum class VmaAllocationRequestType
{
    Normal,
    TLSF,
    // Used by "Linear" algorithm.
    UpperAddress,
    EndOf1st,
    EndOf2nd,
};

#endif // _VMA_ENUM_DECLARATIONS

#ifndef _VMA_FORWARD_DECLARATIONS
// Opaque handle used by allocation algorithms to identify single allocation in any conforming way.
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VmaAllocHandle);

struct VmaMutexLock;
struct VmaMutexLockRead;
struct VmaMutexLockWrite;

template<typename T>
struct AtomicTransactionalIncrement;

template<typename T>
struct VmaStlAllocator;

template<typename T, typename AllocatorT>
class VmaVector;

template<typename T, typename AllocatorT, size_t N>
class VmaSmallVector;

template<typename T>
class VmaPoolAllocator;

template<typename T>
struct VmaListItem;

template<typename T>
class VmaRawList;

template<typename T, typename AllocatorT>
class VmaList;

template<typename ItemTypeTraits>
class VmaIntrusiveLinkedList;

// Unused in this version
#if 0
template<typename T1, typename T2>
struct VmaPair;
template<typename FirstT, typename SecondT>
struct VmaPairFirstLess;

template<typename KeyT, typename ValueT>
class VmaMap;
#endif

#if VMA_STATS_STRING_ENABLED
class VmaStringBuilder;
class VmaJsonWriter;
#endif

class VmaDeviceMemoryBlock;

struct VmaDedicatedAllocationListItemTraits;
class VmaDedicatedAllocationList;

struct VmaSuballocation;
struct VmaSuballocationOffsetLess;
struct VmaSuballocationOffsetGreater;
struct VmaSuballocationItemSizeLess;

typedef VmaList<VmaSuballocation, VmaStlAllocator<VmaSuballocation>> VmaSuballocationList;

struct VmaAllocationRequest;

class VmaBlockMetadata;
class VmaBlockMetadata_Linear;
class VmaBlockMetadata_TLSF;

class VmaBlockVector;

struct VmaPoolListItemTraits;

struct VmaCurrentBudgetData;

class VmaAllocationObjectAllocator;

#endif // _VMA_FORWARD_DECLARATIONS


#ifndef _VMA_FUNCTIONS

/*
Returns number of bits set to 1 in (v).

On specific platforms and compilers you can use instrinsics like:

Visual Studio:
    return __popcnt(v);
GCC, Clang:
    return static_cast<uint32_t>(__builtin_popcount(v));

Define macro VMA_COUNT_BITS_SET to provide your optimized implementation.
But you need to check in runtime whether user's CPU supports these, as some old processors don't.
*/
static inline uint32_t VmaCountBitsSet(uint32_t v)
{
#if __cplusplus >= 202002L || _MSVC_LANG >= 202002L // C++20
    return std::popcount(v);
#else
    uint32_t c = v - ((v >> 1) & 0x55555555);
    c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
    c = ((c >> 4) + c) & 0x0F0F0F0F;
    c = ((c >> 8) + c) & 0x00FF00FF;
    c = ((c >> 16) + c) & 0x0000FFFF;
    return c;
#endif
}

static inline uint8_t VmaBitScanLSB(uint64_t mask)
{
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long pos;
    if (_BitScanForward64(&pos, mask))
        return static_cast<uint8_t>(pos);
    return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
    return static_cast<uint8_t>(__builtin_ffsll(mask)) - 1U;
#else
    uint8_t pos = 0;
    uint64_t bit = 1;
    do
    {
        if (mask & bit)
            return pos;
        bit <<= 1;
    } while (pos++ < 63);
    return UINT8_MAX;
#endif
}

static inline uint8_t VmaBitScanLSB(uint32_t mask)
{
#ifdef _MSC_VER
    unsigned long pos;
    if (_BitScanForward(&pos, mask))
        return static_cast<uint8_t>(pos);
    return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
    return static_cast<uint8_t>(__builtin_ffs(mask)) - 1U;
#else
    uint8_t pos = 0;
    uint32_t bit = 1;
    do
    {
        if (mask & bit)
            return pos;
        bit <<= 1;
    } while (pos++ < 31);
    return UINT8_MAX;
#endif
}

static inline uint8_t VmaBitScanMSB(uint64_t mask)
{
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long pos;
    if (_BitScanReverse64(&pos, mask))
        return static_cast<uint8_t>(pos);
#elif defined __GNUC__ || defined __clang__
    if (mask)
        return 63 - static_cast<uint8_t>(__builtin_clzll(mask));
#else
    uint8_t pos = 63;
    uint64_t bit = 1ULL << 63;
    do
    {
        if (mask & bit)
            return pos;
        bit >>= 1;
    } while (pos-- > 0);
#endif
    return UINT8_MAX;
}

static inline uint8_t VmaBitScanMSB(uint32_t mask)
{
#ifdef _MSC_VER
    unsigned long pos;
    if (_BitScanReverse(&pos, mask))
        return static_cast<uint8_t>(pos);
#elif defined __GNUC__ || defined __clang__
    if (mask)
        return 31 - static_cast<uint8_t>(__builtin_clz(mask));
#else
    uint8_t pos = 31;
    uint32_t bit = 1UL << 31;
    do
    {
        if (mask & bit)
            return pos;
        bit >>= 1;
    } while (pos-- > 0);
#endif
    return UINT8_MAX;
}

/*
Returns true if given number is a power of two.
T must be unsigned integer number or signed integer but always nonnegative.
For 0 returns true.
*/
template <typename T>
inline bool VmaIsPow2(T x)
{
    return (x & (x - 1)) == 0;
}

// Aligns given value up to nearest multiply of align value. For example: VmaAlignUp(11, 8) = 16.
// Use types like uint32_t, uint64_t as T.
template <typename T>
static inline T VmaAlignUp(T val, T alignment)
{
    VMA_HEAVY_ASSERT(VmaIsPow2(alignment));
    return (val + alignment - 1) & ~(alignment - 1);
}

// Aligns given value down to nearest multiply of align value. For example: VmaAlignUp(11, 8) = 8.
// Use types like uint32_t, uint64_t as T.
template <typename T>
static inline T VmaAlignDown(T val, T alignment)
{
    VMA_HEAVY_ASSERT(VmaIsPow2(alignment));
    return val & ~(alignment - 1);
}

// Division with mathematical rounding to nearest number.
template <typename T>
static inline T VmaRoundDiv(T x, T y)
{
    return (x + (y / (T)2)) / y;
}

// Divide by 'y' and round up to nearest integer.
template <typename T>
static inline T VmaDivideRoundingUp(T x, T y)
{
    return (x + y - (T)1) / y;
}

// Returns smallest power of 2 greater or equal to v.
static inline uint32_t VmaNextPow2(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static inline uint64_t VmaNextPow2(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

// Returns largest power of 2 less or equal to v.
static inline uint32_t VmaPrevPow2(uint32_t v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v = v ^ (v >> 1);
    return v;
}

static inline uint64_t VmaPrevPow2(uint64_t v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v = v ^ (v >> 1);
    return v;
}

static inline bool VmaStrIsEmpty(const char* pStr)
{
    return pStr == VMA_NULL || *pStr == '\0';
}

/*
Returns true if two memory blocks occupy overlapping pages.
ResourceA must be in less memory offset than ResourceB.

Algorithm is based on "Vulkan 1.0.39 - A Specification (with all registered Vulkan extensions)"
chapter 11.6 "Resource Memory Association", paragraph "Buffer-Image Granularity".
*/
static inline bool VmaBlocksOnSamePage(
    VkDeviceSize resourceAOffset,
    VkDeviceSize resourceASize,
    VkDeviceSize resourceBOffset,
    VkDeviceSize pageSize)
{
    VMA_ASSERT(resourceAOffset + resourceASize <= resourceBOffset && resourceASize > 0 && pageSize > 0);
    VkDeviceSize resourceAEnd = resourceAOffset + resourceASize - 1;
    VkDeviceSize resourceAEndPage = resourceAEnd & ~(pageSize - 1);
    VkDeviceSize resourceBStart = resourceBOffset;
    VkDeviceSize resourceBStartPage = resourceBStart & ~(pageSize - 1);
    return resourceAEndPage == resourceBStartPage;
}

/*
Returns true if given suballocation types could conflict and must respect
VkPhysicalDeviceLimits::bufferImageGranularity. They conflict if one is buffer
or linear image and another one is optimal image. If type is unknown, behave
conservatively.
*/
static inline bool VmaIsBufferImageGranularityConflict(
    VmaSuballocationType suballocType1,
    VmaSuballocationType suballocType2)
{
    if (suballocType1 > suballocType2)
    {
        VMA_SWAP(suballocType1, suballocType2);
    }

    switch (suballocType1)
    {
    case VMA_SUBALLOCATION_TYPE_FREE:
        return false;
    case VMA_SUBALLOCATION_TYPE_UNKNOWN:
        return true;
    case VMA_SUBALLOCATION_TYPE_BUFFER:
        return
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
    case VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN:
        return
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR ||
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
    case VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR:
        return
            suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
    case VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL:
        return false;
    default:
        VMA_ASSERT(0);
        return true;
    }
}

static void VmaWriteMagicValue(void* pData, VkDeviceSize offset)
{
#if VMA_DEBUG_MARGIN > 0 && VMA_DEBUG_DETECT_CORRUPTION
    uint32_t* pDst = (uint32_t*)((char*)pData + offset);
    const size_t numberCount = VMA_DEBUG_MARGIN / sizeof(uint32_t);
    for (size_t i = 0; i < numberCount; ++i, ++pDst)
    {
        *pDst = VMA_CORRUPTION_DETECTION_MAGIC_VALUE;
    }
#else
    // no-op
#endif
}

static bool VmaValidateMagicValue(const void* pData, VkDeviceSize offset)
{
#if VMA_DEBUG_MARGIN > 0 && VMA_DEBUG_DETECT_CORRUPTION
    const uint32_t* pSrc = (const uint32_t*)((const char*)pData + offset);
    const size_t numberCount = VMA_DEBUG_MARGIN / sizeof(uint32_t);
    for (size_t i = 0; i < numberCount; ++i, ++pSrc)
    {
        if (*pSrc != VMA_CORRUPTION_DETECTION_MAGIC_VALUE)
        {
            return false;
        }
    }
#endif
    return true;
}

/*
Fills structure with parameters of an example buffer to be used for transfers
during GPU memory defragmentation.
*/
static void VmaFillGpuDefragmentationBufferCreateInfo(VkBufferCreateInfo& outBufCreateInfo)
{
    memset(&outBufCreateInfo, 0, sizeof(outBufCreateInfo));
    outBufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    outBufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    outBufCreateInfo.size = (VkDeviceSize)VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE; // Example size.
}


/*
Performs binary search and returns iterator to first element that is greater or
equal to (key), according to comparison (cmp).

Cmp should return true if first argument is less than second argument.

Returned value is the found element, if present in the collection or place where
new element with value (key) should be inserted.
*/
template <typename CmpLess, typename IterT, typename KeyT>
static IterT VmaBinaryFindFirstNotLess(IterT beg, IterT end, const KeyT& key, const CmpLess& cmp)
{
    size_t down = 0, up = (end - beg);
    while (down < up)
    {
        const size_t mid = down + (up - down) / 2;  // Overflow-safe midpoint calculation
        if (cmp(*(beg + mid), key))
        {
            down = mid + 1;
        }
        else
        {
            up = mid;
        }
    }
    return beg + down;
}

template<typename CmpLess, typename IterT, typename KeyT>
IterT VmaBinaryFindSorted(const IterT& beg, const IterT& end, const KeyT& value, const CmpLess& cmp)
{
    IterT it = VmaBinaryFindFirstNotLess<CmpLess, IterT, KeyT>(
        beg, end, value, cmp);
    if (it == end ||
        (!cmp(*it, value) && !cmp(value, *it)))
    {
        return it;
    }
    return end;
}

/*
Returns true if all pointers in the array are not-null and unique.
Warning! O(n^2) complexity. Use only inside VMA_HEAVY_ASSERT.
T must be pointer type, e.g. VmaAllocation, VmaPool.
*/
template<typename T>
static bool VmaValidatePointerArray(uint32_t count, const T* arr)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        const T iPtr = arr[i];
        if (iPtr == VMA_NULL)
        {
            return false;
        }
        for (uint32_t j = i + 1; j < count; ++j)
        {
            if (iPtr == arr[j])
            {
                return false;
            }
        }
    }
    return true;
}

template<typename MainT, typename NewT>
static inline void VmaPnextChainPushFront(MainT* mainStruct, NewT* newStruct)
{
    newStruct->pNext = mainStruct->pNext;
    mainStruct->pNext = newStruct;
}

// This is the main algorithm that guides the selection of a memory type best for an allocation -
// converts usage to required/preferred/not preferred flags.
static bool FindMemoryPreferences(
    bool isIntegratedGPU,
    const VmaAllocationCreateInfo& allocCreateInfo,
    VkFlags bufImgUsage, // VkBufferCreateInfo::usage or VkImageCreateInfo::usage. UINT32_MAX if unknown.
    VkMemoryPropertyFlags& outRequiredFlags,
    VkMemoryPropertyFlags& outPreferredFlags,
    VkMemoryPropertyFlags& outNotPreferredFlags)
{
    outRequiredFlags = allocCreateInfo.requiredFlags;
    outPreferredFlags = allocCreateInfo.preferredFlags;
    outNotPreferredFlags = 0;

    switch(allocCreateInfo.usage)
    {
    case VMA_MEMORY_USAGE_UNKNOWN:
        break;
    case VMA_MEMORY_USAGE_GPU_ONLY:
        if(!isIntegratedGPU || (outPreferredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
        {
            outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        break;
    case VMA_MEMORY_USAGE_CPU_ONLY:
        outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    case VMA_MEMORY_USAGE_CPU_TO_GPU:
        outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        if(!isIntegratedGPU || (outPreferredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
        {
            outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        break;
    case VMA_MEMORY_USAGE_GPU_TO_CPU:
        outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        outPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    case VMA_MEMORY_USAGE_CPU_COPY:
        outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED:
        outRequiredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        break;
    case VMA_MEMORY_USAGE_AUTO:
    case VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE:
    case VMA_MEMORY_USAGE_AUTO_PREFER_HOST:
    {
        if(bufImgUsage == UINT32_MAX)
        {
            VMA_ASSERT(0 && "VMA_MEMORY_USAGE_AUTO* values can only be used with functions like vmaCreateBuffer, vmaCreateImage so that the details of the created resource are known.");
            return false;
        }
        // This relies on values of VK_IMAGE_USAGE_TRANSFER* being the same VK_BUFFER_IMAGE_TRANSFER*.
        const bool deviceAccess = (bufImgUsage & ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) != 0;
        const bool hostAccessSequentialWrite = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) != 0;
        const bool hostAccessRandom = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
        const bool hostAccessAllowTransferInstead = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT) != 0;
        const bool preferDevice = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        const bool preferHost = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

        // CPU random access - e.g. a buffer written to or transferred from GPU to read back on CPU.
        if(hostAccessRandom)
        {
            if(!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost)
            {
                // Nice if it will end up in HOST_VISIBLE, but more importantly prefer DEVICE_LOCAL.
                // Omitting HOST_VISIBLE here is intentional.
                // In case there is DEVICE_LOCAL | HOST_VISIBLE | HOST_CACHED, it will pick that one.
                // Otherwise, this will give same weight to DEVICE_LOCAL as HOST_VISIBLE | HOST_CACHED and select the former if occurs first on the list.
                outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            else
            {
                // Always CPU memory, cached.
                outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
        }
        // CPU sequential write - may be CPU or host-visible GPU memory, uncached and write-combined.
        else if(hostAccessSequentialWrite)
        {
            // Want uncached and write-combined.
            outNotPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

            if(!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost)
            {
                outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }
            else
            {
                outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                // Direct GPU access, CPU sequential write (e.g. a dynamic uniform buffer updated every frame)
                if(deviceAccess)
                {
                    // Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose GPU memory.
                    if(preferHost)
                        outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    else
                        outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                }
                // GPU no direct access, CPU sequential write (e.g. an upload buffer to be transferred to the GPU)
                else
                {
                    // Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose CPU memory.
                    if(preferDevice)
                        outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    else
                        outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                }
            }
        }
        // No CPU access
        else
        {
            // GPU access, no CPU access (e.g. a color attachment image) - prefer GPU memory
            if(deviceAccess)
            {
                // ...unless there is a clear preference from the user not to do so.
                if(preferHost)
                    outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                else
                    outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }
            // No direct GPU access, no CPU access, just transfers.
            // It may be staging copy intended for e.g. preserving image for next frame (then better GPU memory) or
            // a "swap file" copy to free some GPU memory (then better CPU memory).
            // Up to the user to decide. If no preferece, assume the former and choose GPU memory.
            if(preferHost)
                outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            else
                outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        break;
    }
    default:
        VMA_ASSERT(0);
    }

    // Avoid DEVICE_COHERENT unless explicitly requested.
    if(((allocCreateInfo.requiredFlags | allocCreateInfo.preferredFlags) &
        (VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY)) == 0)
    {
        outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Memory allocation

static void* VmaMalloc(const VkAllocationCallbacks* pAllocationCallbacks, size_t size, size_t alignment)
{
    void* result = VMA_NULL;
    if ((pAllocationCallbacks != VMA_NULL) &&
        (pAllocationCallbacks->pfnAllocation != VMA_NULL))
    {
        result = (*pAllocationCallbacks->pfnAllocation)(
            pAllocationCallbacks->pUserData,
            size,
            alignment,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    }
    else
    {
        result = VMA_SYSTEM_ALIGNED_MALLOC(size, alignment);
    }
    VMA_ASSERT(result != VMA_NULL && "CPU memory allocation failed.");
    return result;
}

static void VmaFree(const VkAllocationCallbacks* pAllocationCallbacks, void* ptr)
{
    if ((pAllocationCallbacks != VMA_NULL) &&
        (pAllocationCallbacks->pfnFree != VMA_NULL))
    {
        (*pAllocationCallbacks->pfnFree)(pAllocationCallbacks->pUserData, ptr);
    }
    else
    {
        VMA_SYSTEM_ALIGNED_FREE(ptr);
    }
}

template<typename T>
static T* VmaAllocate(const VkAllocationCallbacks* pAllocationCallbacks)
{
    return (T*)VmaMalloc(pAllocationCallbacks, sizeof(T), VMA_ALIGN_OF(T));
}

template<typename T>
static T* VmaAllocateArray(const VkAllocationCallbacks* pAllocationCallbacks, size_t count)
{
    return (T*)VmaMalloc(pAllocationCallbacks, sizeof(T) * count, VMA_ALIGN_OF(T));
}

#define vma_new(allocator, type)   new(VmaAllocate<type>(allocator))(type)

#define vma_new_array(allocator, type, count)   new(VmaAllocateArray<type>((allocator), (count)))(type)

template<typename T>
static void vma_delete(const VkAllocationCallbacks* pAllocationCallbacks, T* ptr)
{
    ptr->~T();
    VmaFree(pAllocationCallbacks, ptr);
}

template<typename T>
static void vma_delete_array(const VkAllocationCallbacks* pAllocationCallbacks, T* ptr, size_t count)
{
    if (ptr != VMA_NULL)
    {
        for (size_t i = count; i--; )
        {
            ptr[i].~T();
        }
        VmaFree(pAllocationCallbacks, ptr);
    }
}

static char* VmaCreateStringCopy(const VkAllocationCallbacks* allocs, const char* srcStr)
{
    if (srcStr != VMA_NULL)
    {
        const size_t len = strlen(srcStr);
        char* const result = vma_new_array(allocs, char, len + 1);
        memcpy(result, srcStr, len + 1);
        return result;
    }
    return VMA_NULL;
}

#if VMA_STATS_STRING_ENABLED
static char* VmaCreateStringCopy(const VkAllocationCallbacks* allocs, const char* srcStr, size_t strLen)
{
    if (srcStr != VMA_NULL)
    {
        char* const result = vma_new_array(allocs, char, strLen + 1);
        memcpy(result, srcStr, strLen);
        result[strLen] = '\0';
        return result;
    }
    return VMA_NULL;
}
#endif // VMA_STATS_STRING_ENABLED

static void VmaFreeString(const VkAllocationCallbacks* allocs, char* str)
{
    if (str != VMA_NULL)
    {
        const size_t len = strlen(str);
        vma_delete_array(allocs, str, len + 1);
    }
}

template<typename CmpLess, typename VectorT>
size_t VmaVectorInsertSorted(VectorT& vector, const typename VectorT::value_type& value)
{
    const size_t indexToInsert = VmaBinaryFindFirstNotLess(
        vector.data(),
        vector.data() + vector.size(),
        value,
        CmpLess()) - vector.data();
    VmaVectorInsert(vector, indexToInsert, value);
    return indexToInsert;
}

template<typename CmpLess, typename VectorT>
bool VmaVectorRemoveSorted(VectorT& vector, const typename VectorT::value_type& value)
{
    CmpLess comparator;
    typename VectorT::iterator it = VmaBinaryFindFirstNotLess(
        vector.begin(),
        vector.end(),
        value,
        comparator);
    if ((it != vector.end()) && !comparator(*it, value) && !comparator(value, *it))
    {
        size_t indexToRemove = it - vector.begin();
        VmaVectorRemove(vector, indexToRemove);
        return true;
    }
    return false;
}
#endif // _VMA_FUNCTIONS

#ifndef _VMA_STATISTICS_FUNCTIONS

static void VmaClearStatistics(VmaStatistics& outStats)
{
    outStats.blockCount = 0;
    outStats.allocationCount = 0;
    outStats.blockBytes = 0;
    outStats.allocationBytes = 0;
}

static void VmaAddStatistics(VmaStatistics& inoutStats, const VmaStatistics& src)
{
    inoutStats.blockCount += src.blockCount;
    inoutStats.allocationCount += src.allocationCount;
    inoutStats.blockBytes += src.blockBytes;
    inoutStats.allocationBytes += src.allocationBytes;
}

static void VmaClearDetailedStatistics(VmaDetailedStatistics& outStats)
{
    VmaClearStatistics(outStats.statistics);
    outStats.unusedRangeCount = 0;
    outStats.allocationSizeMin = VK_WHOLE_SIZE;
    outStats.allocationSizeMax = 0;
    outStats.unusedRangeSizeMin = VK_WHOLE_SIZE;
    outStats.unusedRangeSizeMax = 0;
}

static void VmaAddDetailedStatisticsAllocation(VmaDetailedStatistics& inoutStats, VkDeviceSize size)
{
    inoutStats.statistics.allocationCount++;
    inoutStats.statistics.allocationBytes += size;
    inoutStats.allocationSizeMin = VMA_MIN(inoutStats.allocationSizeMin, size);
    inoutStats.allocationSizeMax = VMA_MAX(inoutStats.allocationSizeMax, size);
}

static void VmaAddDetailedStatisticsUnusedRange(VmaDetailedStatistics& inoutStats, VkDeviceSize size)
{
    inoutStats.unusedRangeCount++;
    inoutStats.unusedRangeSizeMin = VMA_MIN(inoutStats.unusedRangeSizeMin, size);
    inoutStats.unusedRangeSizeMax = VMA_MAX(inoutStats.unusedRangeSizeMax, size);
}

static void VmaAddDetailedStatistics(VmaDetailedStatistics& inoutStats, const VmaDetailedStatistics& src)
{
    VmaAddStatistics(inoutStats.statistics, src.statistics);
    inoutStats.unusedRangeCount += src.unusedRangeCount;
    inoutStats.allocationSizeMin = VMA_MIN(inoutStats.allocationSizeMin, src.allocationSizeMin);
    inoutStats.allocationSizeMax = VMA_MAX(inoutStats.allocationSizeMax, src.allocationSizeMax);
    inoutStats.unusedRangeSizeMin = VMA_MIN(inoutStats.unusedRangeSizeMin, src.unusedRangeSizeMin);
    inoutStats.unusedRangeSizeMax = VMA_MAX(inoutStats.unusedRangeSizeMax, src.unusedRangeSizeMax);
}

#endif // _VMA_STATISTICS_FUNCTIONS

#ifndef _VMA_MUTEX_LOCK
// Helper RAII class to lock a mutex in constructor and unlock it in destructor (at the end of scope).
struct VmaMutexLock
{
    VMA_CLASS_NO_COPY(VmaMutexLock)
public:
    VmaMutexLock(VMA_MUTEX& mutex, bool useMutex = true) :
        m_pMutex(useMutex ? &mutex : VMA_NULL)
    {
        if (m_pMutex) { m_pMutex->Lock(); }
    }
    ~VmaMutexLock() {  if (m_pMutex) { m_pMutex->Unlock(); } }

private:
    VMA_MUTEX* m_pMutex;
};

// Helper RAII class to lock a RW mutex in constructor and unlock it in destructor (at the end of scope), for reading.
struct VmaMutexLockRead
{
    VMA_CLASS_NO_COPY(VmaMutexLockRead)
public:
    VmaMutexLockRead(VMA_RW_MUTEX& mutex, bool useMutex) :
        m_pMutex(useMutex ? &mutex : VMA_NULL)
    {
        if (m_pMutex) { m_pMutex->LockRead(); }
    }
    ~VmaMutexLockRead() { if (m_pMutex) { m_pMutex->UnlockRead(); } }

private:
    VMA_RW_MUTEX* m_pMutex;
};

// Helper RAII class to lock a RW mutex in constructor and unlock it in destructor (at the end of scope), for writing.
struct VmaMutexLockWrite
{
    VMA_CLASS_NO_COPY(VmaMutexLockWrite)
public:
    VmaMutexLockWrite(VMA_RW_MUTEX& mutex, bool useMutex)
        : m_pMutex(useMutex ? &mutex : VMA_NULL)
    {
        if (m_pMutex) { m_pMutex->LockWrite(); }
    }
    ~VmaMutexLockWrite() { if (m_pMutex) { m_pMutex->UnlockWrite(); } }

private:
    VMA_RW_MUTEX* m_pMutex;
};

#if VMA_DEBUG_GLOBAL_MUTEX
    static VMA_MUTEX gDebugGlobalMutex;
    #define VMA_DEBUG_GLOBAL_MUTEX_LOCK VmaMutexLock debugGlobalMutexLock(gDebugGlobalMutex, true);
#else
    #define VMA_DEBUG_GLOBAL_MUTEX_LOCK
#endif
#endif // _VMA_MUTEX_LOCK

#ifndef _VMA_ATOMIC_TRANSACTIONAL_INCREMENT
// An object that increments given atomic but decrements it back in the destructor unless Commit() is called.
template<typename T>
struct AtomicTransactionalIncrement
{
public:
    typedef std::atomic<T> AtomicT;

    ~AtomicTransactionalIncrement()
    {
        if(m_Atomic)
            --(*m_Atomic);
    }

    void Commit() { m_Atomic = nullptr; }
    T Increment(AtomicT* atomic)
    {
        m_Atomic = atomic;
        return m_Atomic->fetch_add(1);
    }

private:
    AtomicT* m_Atomic = nullptr;
};
#endif // _VMA_ATOMIC_TRANSACTIONAL_INCREMENT

#ifndef _VMA_STL_ALLOCATOR
// STL-compatible allocator.
template<typename T>
struct VmaStlAllocator
{
    const VkAllocationCallbacks* const m_pCallbacks;
    typedef T value_type;

    VmaStlAllocator(const VkAllocationCallbacks* pCallbacks) : m_pCallbacks(pCallbacks) {}
    template<typename U>
    VmaStlAllocator(const VmaStlAllocator<U>& src) : m_pCallbacks(src.m_pCallbacks) {}
    VmaStlAllocator(const VmaStlAllocator&) = default;
    VmaStlAllocator& operator=(const VmaStlAllocator&) = delete;

    T* allocate(size_t n) { return VmaAllocateArray<T>(m_pCallbacks, n); }
    void deallocate(T* p, size_t n) { VmaFree(m_pCallbacks, p); }

    template<typename U>
    bool operator==(const VmaStlAllocator<U>& rhs) const
    {
        return m_pCallbacks == rhs.m_pCallbacks;
    }
    template<typename U>
    bool operator!=(const VmaStlAllocator<U>& rhs) const
    {
        return m_pCallbacks != rhs.m_pCallbacks;
    }
};
#endif // _VMA_STL_ALLOCATOR

#ifndef _VMA_VECTOR
/* Class with interface compatible with subset of std::vector.
T must be POD because constructors and destructors are not called and memcpy is
used for these objects. */
template<typename T, typename AllocatorT>
class VmaVector
{
public:
    typedef T value_type;
    typedef T* iterator;
    typedef const T* const_iterator;

    VmaVector(const AllocatorT& allocator);
    VmaVector(size_t count, const AllocatorT& allocator);
    // This version of the constructor is here for compatibility with pre-C++14 std::vector.
    // value is unused.
    VmaVector(size_t count, const T& value, const AllocatorT& allocator) : VmaVector(count, allocator) {}
    VmaVector(const VmaVector<T, AllocatorT>& src);
    VmaVector& operator=(const VmaVector& rhs);
    ~VmaVector() { VmaFree(m_Allocator.m_pCallbacks, m_pArray); }

    bool empty() const { return m_Count == 0; }
    size_t size() const { return m_Count; }
    T* data() { return m_pArray; }
    T& front() { VMA_HEAVY_ASSERT(m_Count > 0); return m_pArray[0]; }
    T& back() { VMA_HEAVY_ASSERT(m_Count > 0); return m_pArray[m_Count - 1]; }
    const T* data() const { return m_pArray; }
    const T& front() const { VMA_HEAVY_ASSERT(m_Count > 0); return m_pArray[0]; }
    const T& back() const { VMA_HEAVY_ASSERT(m_Count > 0); return m_pArray[m_Count - 1]; }

    iterator begin() { return m_pArray; }
    iterator end() { return m_pArray + m_Count; }
    const_iterator cbegin() const { return m_pArray; }
    const_iterator cend() const { return m_pArray + m_Count; }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

    void pop_front() { VMA_HEAVY_ASSERT(m_Count > 0); remove(0); }
    void pop_back() { VMA_HEAVY_ASSERT(m_Count > 0); resize(size() - 1); }
    void push_front(const T& src) { insert(0, src); }

    void push_back(const T& src);
    void reserve(size_t newCapacity, bool freeMemory = false);
    void resize(size_t newCount);
    void clear() { resize(0); }
    void shrink_to_fit();
    void insert(size_t index, const T& src);
    void remove(size_t index);

    T& operator[](size_t index) { VMA_HEAVY_ASSERT(index < m_Count); return m_pArray[index]; }
    const T& operator[](size_t index) const { VMA_HEAVY_ASSERT(index < m_Count); return m_pArray[index]; }

private:
    AllocatorT m_Allocator;
    T* m_pArray;
    size_t m_Count;
    size_t m_Capacity;
};

#ifndef _VMA_VECTOR_FUNCTIONS
template<typename T, typename AllocatorT>
VmaVector<T, AllocatorT>::VmaVector(const AllocatorT& allocator)
    : m_Allocator(allocator),
    m_pArray(VMA_NULL),
    m_Count(0),
    m_Capacity(0) {}

template<typename T, typename AllocatorT>
VmaVector<T, AllocatorT>::VmaVector(size_t count, const AllocatorT& allocator)
    : m_Allocator(allocator),
    m_pArray(count ? (T*)VmaAllocateArray<T>(allocator.m_pCallbacks, count) : VMA_NULL),
    m_Count(count),
    m_Capacity(count) {}

template<typename T, typename AllocatorT>
VmaVector<T, AllocatorT>::VmaVector(const VmaVector& src)
    : m_Allocator(src.m_Allocator),
    m_pArray(src.m_Count ? (T*)VmaAllocateArray<T>(src.m_Allocator.m_pCallbacks, src.m_Count) : VMA_NULL),
    m_Count(src.m_Count),
    m_Capacity(src.m_Count)
{
    if (m_Count != 0)
    {
        memcpy(m_pArray, src.m_pArray, m_Count * sizeof(T));
    }
}

template<typename T, typename AllocatorT>
VmaVector<T, AllocatorT>& VmaVector<T, AllocatorT>::operator=(const VmaVector& rhs)
{
    if (&rhs != this)
    {
        resize(rhs.m_Count);
        if (m_Count != 0)
        {
            memcpy(m_pArray, rhs.m_pArray, m_Count * sizeof(T));
        }
    }
    return *this;
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::push_back(const T& src)
{
    const size_t newIndex = size();
    resize(newIndex + 1);
    m_pArray[newIndex] = src;
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::reserve(size_t newCapacity, bool freeMemory)
{
    newCapacity = VMA_MAX(newCapacity, m_Count);

    if ((newCapacity < m_Capacity) && !freeMemory)
    {
        newCapacity = m_Capacity;
    }

    if (newCapacity != m_Capacity)
    {
        T* const newArray = newCapacity ? VmaAllocateArray<T>(m_Allocator, newCapacity) : VMA_NULL;
        if (m_Count != 0)
        {
            memcpy(newArray, m_pArray, m_Count * sizeof(T));
        }
        VmaFree(m_Allocator.m_pCallbacks, m_pArray);
        m_Capacity = newCapacity;
        m_pArray = newArray;
    }
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::resize(size_t newCount)
{
    size_t newCapacity = m_Capacity;
    if (newCount > m_Capacity)
    {
        newCapacity = VMA_MAX(newCount, VMA_MAX(m_Capacity * 3 / 2, (size_t)8));
    }

    if (newCapacity != m_Capacity)
    {
        T* const newArray = newCapacity ? VmaAllocateArray<T>(m_Allocator.m_pCallbacks, newCapacity) : VMA_NULL;
        const size_t elementsToCopy = VMA_MIN(m_Count, newCount);
        if (elementsToCopy != 0)
        {
            memcpy(newArray, m_pArray, elementsToCopy * sizeof(T));
        }
        VmaFree(m_Allocator.m_pCallbacks, m_pArray);
        m_Capacity = newCapacity;
        m_pArray = newArray;
    }

    m_Count = newCount;
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::shrink_to_fit()
{
    if (m_Capacity > m_Count)
    {
        T* newArray = VMA_NULL;
        if (m_Count > 0)
        {
            newArray = VmaAllocateArray<T>(m_Allocator.m_pCallbacks, m_Count);
            memcpy(newArray, m_pArray, m_Count * sizeof(T));
        }
        VmaFree(m_Allocator.m_pCallbacks, m_pArray);
        m_Capacity = m_Count;
        m_pArray = newArray;
    }
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::insert(size_t index, const T& src)
{
    VMA_HEAVY_ASSERT(index <= m_Count);
    const size_t oldCount = size();
    resize(oldCount + 1);
    if (index < oldCount)
    {
        memmove(m_pArray + (index + 1), m_pArray + index, (oldCount - index) * sizeof(T));
    }
    m_pArray[index] = src;
}

template<typename T, typename AllocatorT>
void VmaVector<T, AllocatorT>::remove(size_t index)
{
    VMA_HEAVY_ASSERT(index < m_Count);
    const size_t oldCount = size();
    if (index < oldCount - 1)
    {
        memmove(m_pArray + index, m_pArray + (index + 1), (oldCount - index - 1) * sizeof(T));
    }
    resize(oldCount - 1);
}
#endif // _VMA_VECTOR_FUNCTIONS

template<typename T, typename allocatorT>
static void VmaVectorInsert(VmaVector<T, allocatorT>& vec, size_t index, const T& item)
{
    vec.insert(index, item);
}

template<typename T, typename allocatorT>
static void VmaVectorRemove(VmaVector<T, allocatorT>& vec, size_t index)
{
    vec.remove(index);
}
#endif // _VMA_VECTOR

#ifndef _VMA_SMALL_VECTOR
/*
This is a vector (a variable-sized array), optimized for the case when the array is small.

It contains some number of elements in-place, which allows it to avoid heap allocation
when the actual number of elements is below that threshold. This allows normal "small"
cases to be fast without losing generality for large inputs.
*/
template<typename T, typename AllocatorT, size_t N>
class VmaSmallVector
{
public:
    typedef T value_type;
    typedef T* iterator;

    VmaSmallVector(const AllocatorT& allocator);
    VmaSmallVector(size_t count, const AllocatorT& allocator);
    template<typename SrcT, typename SrcAllocatorT, size_t SrcN>
    VmaSmallVector(const VmaSmallVector<SrcT, SrcAllocatorT, SrcN>&) = delete;
    template<typename SrcT, typename SrcAllocatorT, size_t SrcN>
    VmaSmallVector<T, AllocatorT, N>& operator=(const VmaSmallVector<SrcT, SrcAllocatorT, SrcN>&) = delete;
    ~VmaSmallVector() = default;

    bool empty() const { return m_Count == 0; }
    size_t size() const { return m_Count; }
    T* data() { return m_Count > N ? m_DynamicArray.data() : m_StaticArray; }
    T& front() { VMA_HEAVY_ASSERT(m_Count > 0); return data()[0]; }
    T& back() { VMA_HEAVY_ASSERT(m_Count > 0); return data()[m_Count - 1]; }
    const T* data() const { return m_Count > N ? m_DynamicArray.data() : m_StaticArray; }
    const T& front() const { VMA_HEAVY_ASSERT(m_Count > 0); return data()[0]; }
    const T& back() const { VMA_HEAVY_ASSERT(m_Count > 0); return data()[m_Count - 1]; }

    iterator begin() { return data(); }
    iterator end() { return data() + m_Count; }

    void pop_front() { VMA_HEAVY_ASSERT(m_Count > 0); remove(0); }
    void pop_back() { VMA_HEAVY_ASSERT(m_Count > 0); resize(size() - 1); }
    void push_front(const T& src) { insert(0, src); }

    void push_back(const T& src);
    void resize(size_t newCount, bool freeMemory = false);
    void clear(bool freeMemory = false);
    void insert(size_t index, const T& src);
    void remove(size_t index);

    T& operator[](size_t index) { VMA_HEAVY_ASSERT(index < m_Count); return data()[index]; }
    const T& operator[](size_t index) const { VMA_HEAVY_ASSERT(index < m_Count); return data()[index]; }

private:
    size_t m_Count;
    T m_StaticArray[N]; // Used when m_Size <= N
    VmaVector<T, AllocatorT> m_DynamicArray; // Used when m_Size > N
};

#ifndef _VMA_SMALL_VECTOR_FUNCTIONS
template<typename T, typename AllocatorT, size_t N>
VmaSmallVector<T, AllocatorT, N>::VmaSmallVector(const AllocatorT& allocator)
    : m_Count(0),
    m_DynamicArray(allocator) {}

template<typename T, typename AllocatorT, size_t N>
VmaSmallVector<T, AllocatorT, N>::VmaSmallVector(size_t count, const AllocatorT& allocator)
    : m_Count(count),
    m_DynamicArray(count > N ? count : 0, allocator) {}

template<typename T, typename AllocatorT, size_t N>
void VmaSmallVector<T, AllocatorT, N>::push_back(const T& src)
{
    const size_t newIndex = size();
    resize(newIndex + 1);
    data()[newIndex] = src;
}

template<typename T, typename AllocatorT, size_t N>
void VmaSmallVector<T, AllocatorT, N>::resize(size_t newCount, bool freeMemory)
{
    if (newCount > N && m_Count > N)
    {
        // Any direction, staying in m_DynamicArray
        m_DynamicArray.resize(newCount);
        if (freeMemory)
        {
            m_DynamicArray.shrink_to_fit();
        }
    }
    else if (newCount > N && m_Count <= N)
    {
        // Growing, moving from m_StaticArray to m_DynamicArray
        m_DynamicArray.resize(newCount);
        if (m_Count > 0)
        {
            memcpy(m_DynamicArray.data(), m_StaticArray, m_Count * sizeof(T));
        }
    }
    else if (newCount <= N && m_Count > N)
    {
        // Shrinking, moving from m_DynamicArray to m_StaticArray
        if (newCount > 0)
        {
            memcpy(m_StaticArray, m_DynamicArray.data(), newCount * sizeof(T));
        }
        m_DynamicArray.resize(0);
        if (freeMemory)
        {
            m_DynamicArray.shrink_to_fit();
        }
    }
    else
    {
        // Any direction, staying in m_StaticArray - nothing to do here
    }
    m_Count = newCount;
}

template<typename T, typename AllocatorT, size_t N>
void VmaSmallVector<T, AllocatorT, N>::clear(bool freeMemory)
{
    m_DynamicArray.clear();
    if (freeMemory)
    {
        m_DynamicArray.shrink_to_fit();
    }
    m_Count = 0;
}

template<typename T, typename AllocatorT, size_t N>
void VmaSmallVector<T, AllocatorT, N>::insert(size_t index, const T& src)
{
    VMA_HEAVY_ASSERT(index <= m_Count);
    const size_t oldCount = size();
    resize(oldCount + 1);
    T* const dataPtr = data();
    if (index < oldCount)
    {
        //  I know, this could be more optimal for case where memmove can be memcpy directly from m_StaticArray to m_DynamicArray.
        memmove(dataPtr + (index + 1), dataPtr + index, (oldCount - index) * sizeof(T));
    }
    dataPtr[index] = src;
}

template<typename T, typename AllocatorT, size_t N>
void VmaSmallVector<T, AllocatorT, N>::remove(size_t index)
{
    VMA_HEAVY_ASSERT(index < m_Count);
    const size_t oldCount = size();
    if (index < oldCount - 1)
    {
        //  I know, this could be more optimal for case where memmove can be memcpy directly from m_DynamicArray to m_StaticArray.
        T* const dataPtr = data();
        memmove(dataPtr + index, dataPtr + (index + 1), (oldCount - index - 1) * sizeof(T));
    }
    resize(oldCount - 1);
}
#endif // _VMA_SMALL_VECTOR_FUNCTIONS
#endif // _VMA_SMALL_VECTOR

#ifndef _VMA_POOL_ALLOCATOR
/*
Allocator for objects of type T using a list of arrays (pools) to speed up
allocation. Number of elements that can be allocated is not bounded because
allocator can create multiple blocks.
*/
template<typename T>
class VmaPoolAllocator
{
    VMA_CLASS_NO_COPY(VmaPoolAllocator)
public:
    VmaPoolAllocator(const VkAllocationCallbacks* pAllocationCallbacks, uint32_t firstBlockCapacity);
    ~VmaPoolAllocator();
    template<typename... Types> T* Alloc(Types&&... args);
    void Free(T* ptr);

private:
    union Item
    {
        uint32_t NextFreeIndex;
        alignas(T) char Value[sizeof(T)];
    };
    struct ItemBlock
    {
        Item* pItems;
        uint32_t Capacity;
        uint32_t FirstFreeIndex;
    };

    const VkAllocationCallbacks* m_pAllocationCallbacks;
    const uint32_t m_FirstBlockCapacity;
    VmaVector<ItemBlock, VmaStlAllocator<ItemBlock>> m_ItemBlocks;

    ItemBlock& CreateNewBlock();
};

#ifndef _VMA_POOL_ALLOCATOR_FUNCTIONS
template<typename T>
VmaPoolAllocator<T>::VmaPoolAllocator(const VkAllocationCallbacks* pAllocationCallbacks, uint32_t firstBlockCapacity)
    : m_pAllocationCallbacks(pAllocationCallbacks),
    m_FirstBlockCapacity(firstBlockCapacity),
    m_ItemBlocks(VmaStlAllocator<ItemBlock>(pAllocationCallbacks))
{
    VMA_ASSERT(m_FirstBlockCapacity > 1);
}

template<typename T>
VmaPoolAllocator<T>::~VmaPoolAllocator()
{
    for (size_t i = m_ItemBlocks.size(); i--;)
        vma_delete_array(m_pAllocationCallbacks, m_ItemBlocks[i].pItems, m_ItemBlocks[i].Capacity);
    m_ItemBlocks.clear();
}

template<typename T>
template<typename... Types> T* VmaPoolAllocator<T>::Alloc(Types&&... args)
{
    for (size_t i = m_ItemBlocks.size(); i--; )
    {
        ItemBlock& block = m_ItemBlocks[i];
        // This block has some free items: Use first one.
        if (block.FirstFreeIndex != UINT32_MAX)
        {
            Item* const pItem = &block.pItems[block.FirstFreeIndex];
            block.FirstFreeIndex = pItem->NextFreeIndex;
            T* result = (T*)&pItem->Value;
            new(result)T(std::forward<Types>(args)...); // Explicit constructor call.
            return result;
        }
    }

    // No block has free item: Create new one and use it.
    ItemBlock& newBlock = CreateNewBlock();
    Item* const pItem = &newBlock.pItems[0];
    newBlock.FirstFreeIndex = pItem->NextFreeIndex;
    T* result = (T*)&pItem->Value;
    new(result) T(std::forward<Types>(args)...); // Explicit constructor call.
    return result;
}

template<typename T>
void VmaPoolAllocator<T>::Free(T* ptr)
{
    // Search all memory blocks to find ptr.
    for (size_t i = m_ItemBlocks.size(); i--; )
    {
        ItemBlock& block = m_ItemBlocks[i];

        // Casting to union.
        Item* pItemPtr;
        memcpy(&pItemPtr, &ptr, sizeof(pItemPtr));

        // Check if pItemPtr is in address range of this block.
        if ((pItemPtr >= block.pItems) && (pItemPtr < block.pItems + block.Capacity))
        {
            ptr->~T(); // Explicit destructor call.
            const uint32_t index = static_cast<uint32_t>(pItemPtr - block.pItems);
            pItemPtr->NextFreeIndex = block.FirstFreeIndex;
            block.FirstFreeIndex = index;
            return;
        }
    }
    VMA_ASSERT(0 && "Pointer doesn't belong to this memory pool.");
}

template<typename T>
typename VmaPoolAllocator<T>::ItemBlock& VmaPoolAllocator<T>::CreateNewBlock()
{
    const uint32_t newBlockCapacity = m_ItemBlocks.empty() ?
        m_FirstBlockCapacity : m_ItemBlocks.back().Capacity * 3 / 2;

    const ItemBlock newBlock =
    {
        vma_new_array(m_pAllocationCallbacks, Item, newBlockCapacity),
        newBlockCapacity,
        0
    };

    m_ItemBlocks.push_back(newBlock);

    // Setup singly-linked list of all free items in this block.
    for (uint32_t i = 0; i < newBlockCapacity - 1; ++i)
        newBlock.pItems[i].NextFreeIndex = i + 1;
    newBlock.pItems[newBlockCapacity - 1].NextFreeIndex = UINT32_MAX;
    return m_ItemBlocks.back();
}
#endif // _VMA_POOL_ALLOCATOR_FUNCTIONS
#endif // _VMA_POOL_ALLOCATOR

#ifndef _VMA_RAW_LIST
template<typename T>
struct VmaListItem
{
    VmaListItem* pPrev;
    VmaListItem* pNext;
    T Value;
};

// Doubly linked list.
template<typename T>
class VmaRawList
{
    VMA_CLASS_NO_COPY(VmaRawList)
public:
    typedef VmaListItem<T> ItemType;

    VmaRawList(const VkAllocationCallbacks* pAllocationCallbacks);
    // Intentionally not calling Clear, because that would be unnecessary
    // computations to return all items to m_ItemAllocator as free.
    ~VmaRawList() = default;

    size_t GetCount() const { return m_Count; }
    bool IsEmpty() const { return m_Count == 0; }

    ItemType* Front() { return m_pFront; }
    ItemType* Back() { return m_pBack; }
    const ItemType* Front() const { return m_pFront; }
    const ItemType* Back() const { return m_pBack; }

    ItemType* PushFront();
    ItemType* PushBack();
    ItemType* PushFront(const T& value);
    ItemType* PushBack(const T& value);
    void PopFront();
    void PopBack();

    // Item can be null - it means PushBack.
    ItemType* InsertBefore(ItemType* pItem);
    // Item can be null - it means PushFront.
    ItemType* InsertAfter(ItemType* pItem);
    ItemType* InsertBefore(ItemType* pItem, const T& value);
    ItemType* InsertAfter(ItemType* pItem, const T& value);

    void Clear();
    void Remove(ItemType* pItem);

private:
    const VkAllocationCallbacks* const m_pAllocationCallbacks;
    VmaPoolAllocator<ItemType> m_ItemAllocator;
    ItemType* m_pFront;
    ItemType* m_pBack;
    size_t m_Count;
};

#ifndef _VMA_RAW_LIST_FUNCTIONS
template<typename T>
VmaRawList<T>::VmaRawList(const VkAllocationCallbacks* pAllocationCallbacks)
    : m_pAllocationCallbacks(pAllocationCallbacks),
    m_ItemAllocator(pAllocationCallbacks, 128),
    m_pFront(VMA_NULL),
    m_pBack(VMA_NULL),
    m_Count(0) {}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushFront()
{
    ItemType* const pNewItem = m_ItemAllocator.Alloc();
    pNewItem->pPrev = VMA_NULL;
    if (IsEmpty())
    {
        pNewItem->pNext = VMA_NULL;
        m_pFront = pNewItem;
        m_pBack = pNewItem;
        m_Count = 1;
    }
    else
    {
        pNewItem->pNext = m_pFront;
        m_pFront->pPrev = pNewItem;
        m_pFront = pNewItem;
        ++m_Count;
    }
    return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushBack()
{
    ItemType* const pNewItem = m_ItemAllocator.Alloc();
    pNewItem->pNext = VMA_NULL;
    if(IsEmpty())
    {
        pNewItem->pPrev = VMA_NULL;
        m_pFront = pNewItem;
        m_pBack = pNewItem;
        m_Count = 1;
    }
    else
    {
        pNewItem->pPrev = m_pBack;
        m_pBack->pNext = pNewItem;
        m_pBack = pNewItem;
        ++m_Count;
    }
    return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushFront(const T& value)
{
    ItemType* const pNewItem = PushFront();
    pNewItem->Value = value;
    return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushBack(const T& value)
{
    ItemType* const pNewItem = PushBack();
    pNewItem->Value = value;
    return pNewItem;
}

template<typename T>
void VmaRawList<T>::PopFront()
{
    VMA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const pFrontItem = m_pFront;
    ItemType* const pNextItem = pFrontItem->pNext;
    if (pNextItem != VMA_NULL)
    {
        pNextItem->pPrev = VMA_NULL;
    }
    m_pFront = pNextItem;
    m_ItemAllocator.Free(pFrontItem);
    --m_Count;
}

template<typename T>
void VmaRawList<T>::PopBack()
{
    VMA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const pBackItem = m_pBack;
    ItemType* const pPrevItem = pBackItem->pPrev;
    if(pPrevItem != VMA_NULL)
    {
        pPrevItem->pNext = VMA_NULL;
    }
    m_pBack = pPrevItem;
    m_ItemAllocator.Free(pBackItem);
    --m_Count;
}

template<typename T>
void VmaRawList<T>::Clear()
{
    if (IsEmpty() == false)
    {
        ItemType* pItem = m_pBack;
        while (pItem != VMA_NULL)
        {
            ItemType* const pPrevItem = pItem->pPrev;
            m_ItemAllocator.Free(pItem);
            pItem = pPrevItem;
        }
        m_pFront = VMA_NULL;
        m_pBack = VMA_NULL;
        m_Count = 0;
    }
}

template<typename T>
void VmaRawList<T>::Remove(ItemType* pItem)
{
    VMA_HEAVY_ASSERT(pItem != VMA_NULL);
    VMA_HEAVY_ASSERT(m_Count > 0);

    if(pItem->pPrev != VMA_NULL)
    {
        pItem->pPrev->pNext = pItem->pNext;
    }
    else
    {
        VMA_HEAVY_ASSERT(m_pFront == pItem);
        m_pFront = pItem->pNext;
    }

    if(pItem->pNext != VMA_NULL)
    {
        pItem->pNext->pPrev = pItem->pPrev;
    }
    else
    {
        VMA_HEAVY_ASSERT(m_pBack == pItem);
        m_pBack = pItem->pPrev;
    }

    m_ItemAllocator.Free(pItem);
    --m_Count;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertBefore(ItemType* pItem)
{
    if(pItem != VMA_NULL)
    {
        ItemType* const prevItem = pItem->pPrev;
        ItemType* const newItem = m_ItemAllocator.Alloc();
        newItem->pPrev = prevItem;
        newItem->pNext = pItem;
        pItem->pPrev = newItem;
        if(prevItem != VMA_NULL)
        {
            prevItem->pNext = newItem;
        }
        else
        {
            VMA_HEAVY_ASSERT(m_pFront == pItem);
            m_pFront = newItem;
        }
        ++m_Count;
        return newItem;
    }
    else
        return PushBack();
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertAfter(ItemType* pItem)
{
    if(pItem != VMA_NULL)
    {
        ItemType* const nextItem = pItem->pNext;
        ItemType* const newItem = m_ItemAllocator.Alloc();
        newItem->pNext = nextItem;
        newItem->pPrev = pItem;
        pItem->pNext = newItem;
        if(nextItem != VMA_NULL)
        {
            nextItem->pPrev = newItem;
        }
        else
        {
            VMA_HEAVY_ASSERT(m_pBack == pItem);
            m_pBack = newItem;
        }
        ++m_Count;
        return newItem;
    }
    else
        return PushFront();
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertBefore(ItemType* pItem, const T& value)
{
    ItemType* const newItem = InsertBefore(pItem);
    newItem->Value = value;
    return newItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertAfter(ItemType* pItem, const T& value)
{
    ItemType* const newItem = InsertAfter(pItem);
    newItem->Value = value;
    return newItem;
}
#endif // _VMA_RAW_LIST_FUNCTIONS
#endif // _VMA_RAW_LIST

#ifndef _VMA_LIST
template<typename T, typename AllocatorT>
class VmaList
{
    VMA_CLASS_NO_COPY(VmaList)
public:
    class reverse_iterator;
    class const_iterator;
    class const_reverse_iterator;

    class iterator
    {
        friend class const_iterator;
        friend class VmaList<T, AllocatorT>;
    public:
        iterator() :  m_pList(VMA_NULL), m_pItem(VMA_NULL) {}
        iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return m_pItem->Value; }
        T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return &m_pItem->Value; }

        bool operator==(const iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
        bool operator!=(const iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

        iterator operator++(int) { iterator result = *this; ++*this; return result; }
        iterator operator--(int) { iterator result = *this; --*this; return result; }

        iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); m_pItem = m_pItem->pNext; return *this; }
        iterator& operator--();

    private:
        VmaRawList<T>* m_pList;
        VmaListItem<T>* m_pItem;

        iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
    };
    class reverse_iterator
    {
        friend class const_reverse_iterator;
        friend class VmaList<T, AllocatorT>;
    public:
        reverse_iterator() : m_pList(VMA_NULL), m_pItem(VMA_NULL) {}
        reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return m_pItem->Value; }
        T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return &m_pItem->Value; }

        bool operator==(const reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
        bool operator!=(const reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

        reverse_iterator operator++(int) { reverse_iterator result = *this; ++* this; return result; }
        reverse_iterator operator--(int) { reverse_iterator result = *this; --* this; return result; }

        reverse_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); m_pItem = m_pItem->pPrev; return *this; }
        reverse_iterator& operator--();

    private:
        VmaRawList<T>* m_pList;
        VmaListItem<T>* m_pItem;

        reverse_iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
    };
    class const_iterator
    {
        friend class VmaList<T, AllocatorT>;
    public:
        const_iterator() : m_pList(VMA_NULL), m_pItem(VMA_NULL) {}
        const_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

        const T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return m_pItem->Value; }
        const T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return &m_pItem->Value; }

        bool operator==(const const_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
        bool operator!=(const const_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

        const_iterator operator++(int) { const_iterator result = *this; ++* this; return result; }
        const_iterator operator--(int) { const_iterator result = *this; --* this; return result; }

        const_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); m_pItem = m_pItem->pNext; return *this; }
        const_iterator& operator--();

    private:
        const VmaRawList<T>* m_pList;
        const VmaListItem<T>* m_pItem;

        const_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
    };
    class const_reverse_iterator
    {
        friend class VmaList<T, AllocatorT>;
    public:
        const_reverse_iterator() : m_pList(VMA_NULL), m_pItem(VMA_NULL) {}
        const_reverse_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        reverse_iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

        const T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return m_pItem->Value; }
        const T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); return &m_pItem->Value; }

        bool operator==(const const_reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
        bool operator!=(const const_reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

        const_reverse_iterator operator++(int) { const_reverse_iterator result = *this; ++* this; return result; }
        const_reverse_iterator operator--(int) { const_reverse_iterator result = *this; --* this; return result; }

        const_reverse_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != VMA_NULL); m_pItem = m_pItem->pPrev; return *this; }
        const_reverse_iterator& operator--();

    private:
        const VmaRawList<T>* m_pList;
        const VmaListItem<T>* m_pItem;

        const_reverse_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
    };

    VmaList(const AllocatorT& allocator) : m_RawList(allocator.m_pCallbacks) {}

    bool empty() const { return m_RawList.IsEmpty(); }
    size_t size() const { return m_RawList.GetCount(); }

    iterator begin() { return iterator(&m_RawList, m_RawList.Front()); }
    iterator end() { return iterator(&m_RawList, VMA_NULL); }

    const_iterator cbegin() const { return const_iterator(&m_RawList, m_RawList.Front()); }
    const_iterator cend() const { return const_iterator(&m_RawList, VMA_NULL); }

    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

    reverse_iterator rbegin() { return reverse_iterator(&m_RawList, m_RawList.Back()); }
    reverse_iterator rend() { return reverse_iterator(&m_RawList, VMA_NULL); }

    const_reverse_iterator crbegin() const { return const_reverse_iterator(&m_RawList, m_RawList.Back()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(&m_RawList, VMA_NULL); }

    const_reverse_iterator rbegin() const { return crbegin(); }
    const_reverse_iterator rend() const { return crend(); }

    void push_back(const T& value) { m_RawList.PushBack(value); }
    iterator insert(iterator it, const T& value) { return iterator(&m_RawList, m_RawList.InsertBefore(it.m_pItem, value)); }

    void clear() { m_RawList.Clear(); }
    void erase(iterator it) { m_RawList.Remove(it.m_pItem); }

private:
    VmaRawList<T> m_RawList;
};

#ifndef _VMA_LIST_FUNCTIONS
template<typename T, typename AllocatorT>
typename VmaList<T, AllocatorT>::iterator& VmaList<T, AllocatorT>::iterator::operator--()
{
    if (m_pItem != VMA_NULL)
    {
        m_pItem = m_pItem->pPrev;
    }
    else
    {
        VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Back();
    }
    return *this;
}

template<typename T, typename AllocatorT>
typename VmaList<T, AllocatorT>::reverse_iterator& VmaList<T, AllocatorT>::reverse_iterator::operator--()
{
    if (m_pItem != VMA_NULL)
    {
        m_pItem = m_pItem->pNext;
    }
    else
    {
        VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Front();
    }
    return *this;
}

template<typename T, typename AllocatorT>
typename VmaList<T, AllocatorT>::const_iterator& VmaList<T, AllocatorT>::const_iterator::operator--()
{
    if (m_pItem != VMA_NULL)
    {
        m_pItem = m_pItem->pPrev;
    }
    else
    {
        VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Back();
    }
    return *this;
}

template<typename T, typename AllocatorT>
typename VmaList<T, AllocatorT>::const_reverse_iterator& VmaList<T, AllocatorT>::const_reverse_iterator::operator--()
{
    if (m_pItem != VMA_NULL)
    {
        m_pItem = m_pItem->pNext;
    }
    else
    {
        VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Back();
    }
    return *this;
}
#endif // _VMA_LIST_FUNCTIONS
#endif // _VMA_LIST

#ifndef _VMA_INTRUSIVE_LINKED_LIST
/*
Expected interface of ItemTypeTraits:
struct MyItemTypeTraits
{
    typedef MyItem ItemType;
    static ItemType* GetPrev(const ItemType* item) { return item->myPrevPtr; }
    static ItemType* GetNext(const ItemType* item) { return item->myNextPtr; }
    static ItemType*& AccessPrev(ItemType* item) { return item->myPrevPtr; }
    static ItemType*& AccessNext(ItemType* item) { return item->myNextPtr; }
};
*/
template<typename ItemTypeTraits>
class VmaIntrusiveLinkedList
{
public:
    typedef typename ItemTypeTraits::ItemType ItemType;
    static ItemType* GetPrev(const ItemType* item) { return ItemTypeTraits::GetPrev(item); }
    static ItemType* GetNext(const ItemType* item) { return ItemTypeTraits::GetNext(item); }

    // Movable, not copyable.
    VmaIntrusiveLinkedList() = default;
    VmaIntrusiveLinkedList(VmaIntrusiveLinkedList && src);
    VmaIntrusiveLinkedList(const VmaIntrusiveLinkedList&) = delete;
    VmaIntrusiveLinkedList& operator=(VmaIntrusiveLinkedList&& src);
    VmaIntrusiveLinkedList& operator=(const VmaIntrusiveLinkedList&) = delete;
    ~VmaIntrusiveLinkedList() { VMA_HEAVY_ASSERT(IsEmpty()); }

    size_t GetCount() const { return m_Count; }
    bool IsEmpty() const { return m_Count == 0; }
    ItemType* Front() { return m_Front; }
    ItemType* Back() { return m_Back; }
    const ItemType* Front() const { return m_Front; }
    const ItemType* Back() const { return m_Back; }

    void PushBack(ItemType* item);
    void PushFront(ItemType* item);
    ItemType* PopBack();
    ItemType* PopFront();

    // MyItem can be null - it means PushBack.
    void InsertBefore(ItemType* existingItem, ItemType* newItem);
    // MyItem can be null - it means PushFront.
    void InsertAfter(ItemType* existingItem, ItemType* newItem);
    void Remove(ItemType* item);
    void RemoveAll();

private:
    ItemType* m_Front = VMA_NULL;
    ItemType* m_Back = VMA_NULL;
    size_t m_Count = 0;
};

#ifndef _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>::VmaIntrusiveLinkedList(VmaIntrusiveLinkedList&& src)
    : m_Front(src.m_Front), m_Back(src.m_Back), m_Count(src.m_Count)
{
    src.m_Front = src.m_Back = VMA_NULL;
    src.m_Count = 0;
}

template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>& VmaIntrusiveLinkedList<ItemTypeTraits>::operator=(VmaIntrusiveLinkedList&& src)
{
    if (&src != this)
    {
        VMA_HEAVY_ASSERT(IsEmpty());
        m_Front = src.m_Front;
        m_Back = src.m_Back;
        m_Count = src.m_Count;
        src.m_Front = src.m_Back = VMA_NULL;
        src.m_Count = 0;
    }
    return *this;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::PushBack(ItemType* item)
{
    VMA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == VMA_NULL && ItemTypeTraits::GetNext(item) == VMA_NULL);
    if (IsEmpty())
    {
        m_Front = item;
        m_Back = item;
        m_Count = 1;
    }
    else
    {
        ItemTypeTraits::AccessPrev(item) = m_Back;
        ItemTypeTraits::AccessNext(m_Back) = item;
        m_Back = item;
        ++m_Count;
    }
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::PushFront(ItemType* item)
{
    VMA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == VMA_NULL && ItemTypeTraits::GetNext(item) == VMA_NULL);
    if (IsEmpty())
    {
        m_Front = item;
        m_Back = item;
        m_Count = 1;
    }
    else
    {
        ItemTypeTraits::AccessNext(item) = m_Front;
        ItemTypeTraits::AccessPrev(m_Front) = item;
        m_Front = item;
        ++m_Count;
    }
}

template<typename ItemTypeTraits>
typename VmaIntrusiveLinkedList<ItemTypeTraits>::ItemType* VmaIntrusiveLinkedList<ItemTypeTraits>::PopBack()
{
    VMA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const backItem = m_Back;
    ItemType* const prevItem = ItemTypeTraits::GetPrev(backItem);
    if (prevItem != VMA_NULL)
    {
        ItemTypeTraits::AccessNext(prevItem) = VMA_NULL;
    }
    m_Back = prevItem;
    --m_Count;
    ItemTypeTraits::AccessPrev(backItem) = VMA_NULL;
    ItemTypeTraits::AccessNext(backItem) = VMA_NULL;
    return backItem;
}

template<typename ItemTypeTraits>
typename VmaIntrusiveLinkedList<ItemTypeTraits>::ItemType* VmaIntrusiveLinkedList<ItemTypeTraits>::PopFront()
{
    VMA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const frontItem = m_Front;
    ItemType* const nextItem = ItemTypeTraits::GetNext(frontItem);
    if (nextItem != VMA_NULL)
    {
        ItemTypeTraits::AccessPrev(nextItem) = VMA_NULL;
    }
    m_Front = nextItem;
    --m_Count;
    ItemTypeTraits::AccessPrev(frontItem) = VMA_NULL;
    ItemTypeTraits::AccessNext(frontItem) = VMA_NULL;
    return frontItem;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::InsertBefore(ItemType* existingItem, ItemType* newItem)
{
    VMA_HEAVY_ASSERT(newItem != VMA_NULL && ItemTypeTraits::GetPrev(newItem) == VMA_NULL && ItemTypeTraits::GetNext(newItem) == VMA_NULL);
    if (existingItem != VMA_NULL)
    {
        ItemType* const prevItem = ItemTypeTraits::GetPrev(existingItem);
        ItemTypeTraits::AccessPrev(newItem) = prevItem;
        ItemTypeTraits::AccessNext(newItem) = existingItem;
        ItemTypeTraits::AccessPrev(existingItem) = newItem;
        if (prevItem != VMA_NULL)
        {
            ItemTypeTraits::AccessNext(prevItem) = newItem;
        }
        else
        {
            VMA_HEAVY_ASSERT(m_Front == existingItem);
            m_Front = newItem;
        }
        ++m_Count;
    }
    else
        PushBack(newItem);
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::InsertAfter(ItemType* existingItem, ItemType* newItem)
{
    VMA_HEAVY_ASSERT(newItem != VMA_NULL && ItemTypeTraits::GetPrev(newItem) == VMA_NULL && ItemTypeTraits::GetNext(newItem) == VMA_NULL);
    if (existingItem != VMA_NULL)
    {
        ItemType* const nextItem = ItemTypeTraits::GetNext(existingItem);
        ItemTypeTraits::AccessNext(newItem) = nextItem;
        ItemTypeTraits::AccessPrev(newItem) = existingItem;
        ItemTypeTraits::AccessNext(existingItem) = newItem;
        if (nextItem != VMA_NULL)
        {
            ItemTypeTraits::AccessPrev(nextItem) = newItem;
        }
        else
        {
            VMA_HEAVY_ASSERT(m_Back == existingItem);
            m_Back = newItem;
        }
        ++m_Count;
    }
    else
        return PushFront(newItem);
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::Remove(ItemType* item)
{
    VMA_HEAVY_ASSERT(item != VMA_NULL && m_Count > 0);
    if (ItemTypeTraits::GetPrev(item) != VMA_NULL)
    {
        ItemTypeTraits::AccessNext(ItemTypeTraits::AccessPrev(item)) = ItemTypeTraits::GetNext(item);
    }
    else
    {
        VMA_HEAVY_ASSERT(m_Front == item);
        m_Front = ItemTypeTraits::GetNext(item);
    }

    if (ItemTypeTraits::GetNext(item) != VMA_NULL)
    {
        ItemTypeTraits::AccessPrev(ItemTypeTraits::AccessNext(item)) = ItemTypeTraits::GetPrev(item);
    }
    else
    {
        VMA_HEAVY_ASSERT(m_Back == item);
        m_Back = ItemTypeTraits::GetPrev(item);
    }
    ItemTypeTraits::AccessPrev(item) = VMA_NULL;
    ItemTypeTraits::AccessNext(item) = VMA_NULL;
    --m_Count;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::RemoveAll()
{
    if (!IsEmpty())
    {
        ItemType* item = m_Back;
        while (item != VMA_NULL)
        {
            ItemType* const prevItem = ItemTypeTraits::AccessPrev(item);
            ItemTypeTraits::AccessPrev(item) = VMA_NULL;
            ItemTypeTraits::AccessNext(item) = VMA_NULL;
            item = prevItem;
        }
        m_Front = VMA_NULL;
        m_Back = VMA_NULL;
        m_Count = 0;
    }
}
#endif // _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
#endif // _VMA_INTRUSIVE_LINKED_LIST

// Unused in this version.
#if 0

#ifndef _VMA_PAIR
template<typename T1, typename T2>
struct VmaPair
{
    T1 first;
    T2 second;

    VmaPair() : first(), second() {}
    VmaPair(const T1& firstSrc, const T2& secondSrc) : first(firstSrc), second(secondSrc) {}
};

template<typename FirstT, typename SecondT>
struct VmaPairFirstLess
{
    bool operator()(const VmaPair<FirstT, SecondT>& lhs, const VmaPair<FirstT, SecondT>& rhs) const
    {
        return lhs.first < rhs.first;
    }
    bool operator()(const VmaPair<FirstT, SecondT>& lhs, const FirstT& rhsFirst) const
    {
        return lhs.first < rhsFirst;
    }
};
#endif // _VMA_PAIR

#ifndef _VMA_MAP
/* Class compatible with subset of interface of std::unordered_map.
KeyT, ValueT must be POD because they will be stored in VmaVector.
*/
template<typename KeyT, typename ValueT>
class VmaMap
{
public:
    typedef VmaPair<KeyT, ValueT> PairType;
    typedef PairType* iterator;

    VmaMap(const VmaStlAllocator<PairType>& allocator) : m_Vector(allocator) {}

    iterator begin() { return m_Vector.begin(); }
    iterator end() { return m_Vector.end(); }
    size_t size() { return m_Vector.size(); }

    void insert(const PairType& pair);
    iterator find(const KeyT& key);
    void erase(iterator it);

private:
    VmaVector< PairType, VmaStlAllocator<PairType>> m_Vector;
};

#ifndef _VMA_MAP_FUNCTIONS
template<typename KeyT, typename ValueT>
void VmaMap<KeyT, ValueT>::insert(const PairType& pair)
{
    const size_t indexToInsert = VmaBinaryFindFirstNotLess(
        m_Vector.data(),
        m_Vector.data() + m_Vector.size(),
        pair,
        VmaPairFirstLess<KeyT, ValueT>()) - m_Vector.data();
    VmaVectorInsert(m_Vector, indexToInsert, pair);
}

template<typename KeyT, typename ValueT>
VmaPair<KeyT, ValueT>* VmaMap<KeyT, ValueT>::find(const KeyT& key)
{
    PairType* it = VmaBinaryFindFirstNotLess(
        m_Vector.data(),
        m_Vector.data() + m_Vector.size(),
        key,
        VmaPairFirstLess<KeyT, ValueT>());
    if ((it != m_Vector.end()) && (it->first == key))
    {
        return it;
    }
    else
    {
        return m_Vector.end();
    }
}

template<typename KeyT, typename ValueT>
void VmaMap<KeyT, ValueT>::erase(iterator it)
{
    VmaVectorRemove(m_Vector, it - m_Vector.begin());
}
#endif // _VMA_MAP_FUNCTIONS
#endif // _VMA_MAP

#endif // #if 0

#if !defined(_VMA_STRING_BUILDER) && VMA_STATS_STRING_ENABLED
class VmaStringBuilder
{
public:
    VmaStringBuilder(const VkAllocationCallbacks* allocationCallbacks) : m_Data(VmaStlAllocator<char>(allocationCallbacks)) {}
    ~VmaStringBuilder() = default;

    size_t GetLength() const { return m_Data.size(); }
    const char* GetData() const { return m_Data.data(); }
    void AddNewLine() { Add('\n'); }
    void Add(char ch) { m_Data.push_back(ch); }

    void Add(const char* pStr);
    void AddNumber(uint32_t num);
    void AddNumber(uint64_t num);
    void AddPointer(const void* ptr);

private:
    VmaVector<char, VmaStlAllocator<char>> m_Data;
};

#ifndef _VMA_STRING_BUILDER_FUNCTIONS
void VmaStringBuilder::Add(const char* pStr)
{
    const size_t strLen = strlen(pStr);
    if (strLen > 0)
    {
        const size_t oldCount = m_Data.size();
        m_Data.resize(oldCount + strLen);
        memcpy(m_Data.data() + oldCount, pStr, strLen);
    }
}

void VmaStringBuilder::AddNumber(uint32_t num)
{
    char buf[11];
    buf[10] = '\0';
    char* p = &buf[10];
    do
    {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    Add(p);
}

void VmaStringBuilder::AddNumber(uint64_t num)
{
    char buf[21];
    buf[20] = '\0';
    char* p = &buf[20];
    do
    {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    Add(p);
}

void VmaStringBuilder::AddPointer(const void* ptr)
{
    char buf[21];
    VmaPtrToStr(buf, sizeof(buf), ptr);
    Add(buf);
}
#endif //_VMA_STRING_BUILDER_FUNCTIONS
#endif // _VMA_STRING_BUILDER

#if !defined(_VMA_JSON_WRITER) && VMA_STATS_STRING_ENABLED
/*
Allows to conveniently build a correct JSON document to be written to the
VmaStringBuilder passed to the constructor.
*/
class VmaJsonWriter
{
    VMA_CLASS_NO_COPY(VmaJsonWriter)
public:
    // sb - string builder to write the document to. Must remain alive for the whole lifetime of this object.
    VmaJsonWriter(const VkAllocationCallbacks* pAllocationCallbacks, VmaStringBuilder& sb);
    ~VmaJsonWriter();

    // Begins object by writing "{".
    // Inside an object, you must call pairs of WriteString and a value, e.g.:
    // j.BeginObject(true); j.WriteString("A"); j.WriteNumber(1); j.WriteString("B"); j.WriteNumber(2); j.EndObject();
    // Will write: { "A": 1, "B": 2 }
    void BeginObject(bool singleLine = false);
    // Ends object by writing "}".
    void EndObject();

    // Begins array by writing "[".
    // Inside an array, you can write a sequence of any values.
    void BeginArray(bool singleLine = false);
    // Ends array by writing "[".
    void EndArray();

    // Writes a string value inside "".
    // pStr can contain any ANSI characters, including '"', new line etc. - they will be properly escaped.
    void WriteString(const char* pStr);

    // Begins writing a string value.
    // Call BeginString, ContinueString, ContinueString, ..., EndString instead of
    // WriteString to conveniently build the string content incrementally, made of
    // parts including numbers.
    void BeginString(const char* pStr = VMA_NULL);
    // Posts next part of an open string.
    void ContinueString(const char* pStr);
    // Posts next part of an open string. The number is converted to decimal characters.
    void ContinueString(uint32_t n);
    void ContinueString(uint64_t n);
    void ContinueString_Size(size_t n);
    // Posts next part of an open string. Pointer value is converted to characters
    // using "%p" formatting - shown as hexadecimal number, e.g.: 000000081276Ad00
    void ContinueString_Pointer(const void* ptr);
    // Ends writing a string value by writing '"'.
    void EndString(const char* pStr = VMA_NULL);

    // Writes a number value.
    void WriteNumber(uint32_t n);
    void WriteNumber(uint64_t n);
    void WriteSize(size_t n);
    // Writes a boolean value - false or true.
    void WriteBool(bool b);
    // Writes a null value.
    void WriteNull();

private:
    enum COLLECTION_TYPE
    {
        COLLECTION_TYPE_OBJECT,
        COLLECTION_TYPE_ARRAY,
    };
    struct StackItem
    {
        COLLECTION_TYPE type;
        uint32_t valueCount;
        bool singleLineMode;
    };

    static const char* const INDENT;

    VmaStringBuilder& m_SB;
    VmaVector< StackItem, VmaStlAllocator<StackItem> > m_Stack;
    bool m_InsideString;

    // Write size_t for less than 64bits
    void WriteSize(size_t n, std::integral_constant<bool, false>) { m_SB.AddNumber(static_cast<uint32_t>(n)); }
    // Write size_t for 64bits
    void WriteSize(size_t n, std::integral_constant<bool, true>) { m_SB.AddNumber(static_cast<uint64_t>(n)); }

    void BeginValue(bool isString);
    void WriteIndent(bool oneLess = false);
};
const char* const VmaJsonWriter::INDENT = "  ";

#ifndef _VMA_JSON_WRITER_FUNCTIONS
VmaJsonWriter::VmaJsonWriter(const VkAllocationCallbacks* pAllocationCallbacks, VmaStringBuilder& sb)
    : m_SB(sb),
    m_Stack(VmaStlAllocator<StackItem>(pAllocationCallbacks)),
    m_InsideString(false) {}

VmaJsonWriter::~VmaJsonWriter()
{
    VMA_ASSERT(!m_InsideString);
    VMA_ASSERT(m_Stack.empty());
}

void VmaJsonWriter::BeginObject(bool singleLine)
{
    VMA_ASSERT(!m_InsideString);

    BeginValue(false);
    m_SB.Add('{');

    StackItem item;
    item.type = COLLECTION_TYPE_OBJECT;
    item.valueCount = 0;
    item.singleLineMode = singleLine;
    m_Stack.push_back(item);
}

void VmaJsonWriter::EndObject()
{
    VMA_ASSERT(!m_InsideString);

    WriteIndent(true);
    m_SB.Add('}');

    VMA_ASSERT(!m_Stack.empty() && m_Stack.back().type == COLLECTION_TYPE_OBJECT);
    m_Stack.pop_back();
}

void VmaJsonWriter::BeginArray(bool singleLine)
{
    VMA_ASSERT(!m_InsideString);

    BeginValue(false);
    m_SB.Add('[');

    StackItem item;
    item.type = COLLECTION_TYPE_ARRAY;
    item.valueCount = 0;
    item.singleLineMode = singleLine;
    m_Stack.push_back(item);
}

void VmaJsonWriter::EndArray()
{
    VMA_ASSERT(!m_InsideString);

    WriteIndent(true);
    m_SB.Add(']');

    VMA_ASSERT(!m_Stack.empty() && m_Stack.back().type == COLLECTION_TYPE_ARRAY);
    m_Stack.pop_back();
}

void VmaJsonWriter::WriteString(const char* pStr)
{
    BeginString(pStr);
    EndString();
}

void VmaJsonWriter::BeginString(const char* pStr)
{
    VMA_ASSERT(!m_InsideString);

    BeginValue(true);
    m_SB.Add('"');
    m_InsideString = true;
    if (pStr != VMA_NULL && pStr[0] != '\0')
    {
        ContinueString(pStr);
    }
}

void VmaJsonWriter::ContinueString(const char* pStr)
{
    VMA_ASSERT(m_InsideString);

    const size_t strLen = strlen(pStr);
    for (size_t i = 0; i < strLen; ++i)
    {
        char ch = pStr[i];
        if (ch == '\\')
        {
            m_SB.Add("\\\\");
        }
        else if (ch == '"')
        {
            m_SB.Add("\\\"");
        }
        else if (ch >= 32)
        {
            m_SB.Add(ch);
        }
        else switch (ch)
        {
        case '\b':
            m_SB.Add("\\b");
            break;
        case '\f':
            m_SB.Add("\\f");
            break;
        case '\n':
            m_SB.Add("\\n");
            break;
        case '\r':
            m_SB.Add("\\r");
            break;
        case '\t':
            m_SB.Add("\\t");
            break;
        default:
            VMA_ASSERT(0 && "Character not currently supported.");
            break;
        }
    }
}

void VmaJsonWriter::ContinueString(uint32_t n)
{
    VMA_ASSERT(m_InsideString);
    m_SB.AddNumber(n);
}

void VmaJsonWriter::ContinueString(uint64_t n)
{
    VMA_ASSERT(m_InsideString);
    m_SB.AddNumber(n);
}

void VmaJsonWriter::ContinueString_Size(size_t n)
{
    VMA_ASSERT(m_InsideString);
    // Fix for AppleClang incorrect type casting
    // TODO: Change to if constexpr when C++17 used as minimal standard
    WriteSize(n, std::is_same<size_t, uint64_t>{});
}

void VmaJsonWriter::ContinueString_Pointer(const void* ptr)
{
    VMA_ASSERT(m_InsideString);
    m_SB.AddPointer(ptr);
}

void VmaJsonWriter::EndString(const char* pStr)
{
    VMA_ASSERT(m_InsideString);
    if (pStr != VMA_NULL && pStr[0] != '\0')
    {
        ContinueString(pStr);
    }
    m_SB.Add('"');
    m_InsideString = false;
}

void VmaJsonWriter::WriteNumber(uint32_t n)
{
    VMA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.AddNumber(n);
}

void VmaJsonWriter::WriteNumber(uint64_t n)
{
    VMA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.AddNumber(n);
}

void VmaJsonWriter::WriteSize(size_t n)
{
    VMA_ASSERT(!m_InsideString);
    BeginValue(false);
    // Fix for AppleClang incorrect type casting
    // TODO: Change to if constexpr when C++17 used as minimal standard
    WriteSize(n, std::is_same<size_t, uint64_t>{});
}

void VmaJsonWriter::WriteBool(bool b)
{
    VMA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.Add(b ? "true" : "false");
}

void VmaJsonWriter::WriteNull()
{
    VMA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.Add("null");
}

void VmaJsonWriter::BeginValue(bool isString)
{
    if (!m_Stack.empty())
    {
        StackItem& currItem = m_Stack.back();
        if (currItem.type == COLLECTION_TYPE_OBJECT &&
            currItem.valueCount % 2 == 0)
        {
            VMA_ASSERT(isString);
        }

        if (currItem.type == COLLECTION_TYPE_OBJECT &&
            currItem.valueCount % 2 != 0)
        {
            m_SB.Add(": ");
        }
        else if (currItem.valueCount > 0)
        {
            m_SB.Add(", ");
            WriteIndent();
        }
        else
        {
            WriteIndent();
        }
        ++currItem.valueCount;
    }
}

void VmaJsonWriter::WriteIndent(bool oneLess)
{
    if (!m_Stack.empty() && !m_Stack.back().singleLineMode)
    {
        m_SB.AddNewLine();

        size_t count = m_Stack.size();
        if (count > 0 && oneLess)
        {
            --count;
        }
        for (size_t i = 0; i < count; ++i)
        {
            m_SB.Add(INDENT);
        }
    }
}
#endif // _VMA_JSON_WRITER_FUNCTIONS

static void VmaPrintDetailedStatistics(VmaJsonWriter& json, const VmaDetailedStatistics& stat)
{
    json.BeginObject();

    json.WriteString("BlockCount");
    json.WriteNumber(stat.statistics.blockCount);
    json.WriteString("BlockBytes");
    json.WriteNumber(stat.statistics.blockBytes);
    json.WriteString("AllocationCount");
    json.WriteNumber(stat.statistics.allocationCount);
    json.WriteString("AllocationBytes");
    json.WriteNumber(stat.statistics.allocationBytes);
    json.WriteString("UnusedRangeCount");
    json.WriteNumber(stat.unusedRangeCount);

    if (stat.statistics.allocationCount > 1)
    {
        json.WriteString("AllocationSizeMin");
        json.WriteNumber(stat.allocationSizeMin);
        json.WriteString("AllocationSizeMax");
        json.WriteNumber(stat.allocationSizeMax);
    }
    if (stat.unusedRangeCount > 1)
    {
        json.WriteString("UnusedRangeSizeMin");
        json.WriteNumber(stat.unusedRangeSizeMin);
        json.WriteString("UnusedRangeSizeMax");
        json.WriteNumber(stat.unusedRangeSizeMax);
    }
    json.EndObject();
}
#endif // _VMA_JSON_WRITER

#ifndef _VMA_MAPPING_HYSTERESIS

class VmaMappingHysteresis
{
    VMA_CLASS_NO_COPY(VmaMappingHysteresis)
public:
    VmaMappingHysteresis() = default;

    uint32_t GetExtraMapping() const { return m_ExtraMapping; }

    // Call when Map was called.
    // Returns true if switched to extra +1 mapping reference count.
    bool PostMap()
    {
#if VMA_MAPPING_HYSTERESIS_ENABLED
        if(m_ExtraMapping == 0)
        {
            ++m_MajorCounter;
            if(m_MajorCounter >= COUNTER_MIN_EXTRA_MAPPING)
            {
                m_ExtraMapping = 1;
                m_MajorCounter = 0;
                m_MinorCounter = 0;
                return true;
            }
        }
        else // m_ExtraMapping == 1
            PostMinorCounter();
#endif // #if VMA_MAPPING_HYSTERESIS_ENABLED
        return false;
    }

    // Call when Unmap was called.
    void PostUnmap()
    {
#if VMA_MAPPING_HYSTERESIS_ENABLED
        if(m_ExtraMapping == 0)
            ++m_MajorCounter;
        else // m_ExtraMapping == 1
            PostMinorCounter();
#endif // #if VMA_MAPPING_HYSTERESIS_ENABLED
    }

    // Call when allocation was made from the memory block.
    void PostAlloc()
    {
#if VMA_MAPPING_HYSTERESIS_ENABLED
        if(m_ExtraMapping == 1)
            ++m_MajorCounter;
        else // m_ExtraMapping == 0
            PostMinorCounter();
#endif // #if VMA_MAPPING_HYSTERESIS_ENABLED
    }

    // Call when allocation was freed from the memory block.
    // Returns true if switched to extra -1 mapping reference count.
    bool PostFree()
    {
#if VMA_MAPPING_HYSTERESIS_ENABLED
        if(m_ExtraMapping == 1)
        {
            ++m_MajorCounter;
            if(m_MajorCounter >= COUNTER_MIN_EXTRA_MAPPING &&
                m_MajorCounter > m_MinorCounter + 1)
            {
                m_ExtraMapping = 0;
                m_MajorCounter = 0;
                m_MinorCounter = 0;
                return true;
            }
        }
        else // m_ExtraMapping == 0
            PostMinorCounter();
#endif // #if VMA_MAPPING_HYSTERESIS_ENABLED
        return false;
    }

private:
    static const int32_t COUNTER_MIN_EXTRA_MAPPING = 7;

    uint32_t m_MinorCounter = 0;
    uint32_t m_MajorCounter = 0;
    uint32_t m_ExtraMapping = 0; // 0 or 1.

    void PostMinorCounter()
    {
        if(m_MinorCounter < m_MajorCounter)
        {
            ++m_MinorCounter;
        }
        else if(m_MajorCounter > 0)
        {
            --m_MajorCounter;
            --m_MinorCounter;
        }
    }
};

#endif // _VMA_MAPPING_HYSTERESIS

#ifndef _VMA_DEVICE_MEMORY_BLOCK
/*
Represents a single block of device memory (`VkDeviceMemory`) with all the
data about its regions (aka suballocations, #VmaAllocation), assigned and free.

Thread-safety:
- Access to m_pMetadata must be externally synchronized.
- Map, Unmap, Bind* are synchronized internally.
*/
class VmaDeviceMemoryBlock
{
    VMA_CLASS_NO_COPY(VmaDeviceMemoryBlock)
public:
    VmaBlockMetadata* m_pMetadata;

    VmaDeviceMemoryBlock(VmaAllocator hAllocator);
    ~VmaDeviceMemoryBlock();

    // Always call after construction.
    void Init(
        VmaAllocator hAllocator,
        VmaPool hParentPool,
        uint32_t newMemoryTypeIndex,
        VkDeviceMemory newMemory,
        VkDeviceSize newSize,
        uint32_t id,
        uint32_t algorithm,
        VkDeviceSize bufferImageGranularity);
    // Always call before destruction.
    void Destroy(VmaAllocator allocator);

    VmaPool GetParentPool() const { return m_hParentPool; }
    VkDeviceMemory GetDeviceMemory() const { return m_hMemory; }
    uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }
    uint32_t GetId() const { return m_Id; }
    void* GetMappedData() const { return m_pMappedData; }
    uint32_t GetMapRefCount() const { return m_MapCount; }

    // Call when allocation/free was made from m_pMetadata.
    // Used for m_MappingHysteresis.
    void PostAlloc() { m_MappingHysteresis.PostAlloc(); }
    void PostFree(VmaAllocator hAllocator);

    // Validates all data structures inside this object. If not valid, returns false.
    bool Validate() const;
    VkResult CheckCorruption(VmaAllocator hAllocator);

    // ppData can be null.
    VkResult Map(VmaAllocator hAllocator, uint32_t count, void** ppData);
    void Unmap(VmaAllocator hAllocator, uint32_t count);

    VkResult WriteMagicValueAfterAllocation(VmaAllocator hAllocator, VkDeviceSize allocOffset, VkDeviceSize allocSize);
    VkResult ValidateMagicValueAfterAllocation(VmaAllocator hAllocator, VkDeviceSize allocOffset, VkDeviceSize allocSize);

    VkResult BindBufferMemory(
        const VmaAllocator hAllocator,
        const VmaAllocation hAllocation,
        VkDeviceSize allocationLocalOffset,
        VkBuffer hBuffer,
        const void* pNext);
    VkResult BindImageMemory(
        const VmaAllocator hAllocator,
        const VmaAllocation hAllocation,
        VkDeviceSize allocationLocalOffset,
        VkImage hImage,
        const void* pNext);

private:
    VmaPool m_hParentPool; // VK_NULL_HANDLE if not belongs to custom pool.
    uint32_t m_MemoryTypeIndex;
    uint32_t m_Id;
    VkDeviceMemory m_hMemory;

    /*
    Protects access to m_hMemory so it is not used by multiple threads simultaneously, e.g. vkMapMemory, vkBindBufferMemory.
    Also protects m_MapCount, m_pMappedData.
    Allocations, deallocations, any change in m_pMetadata is protected by parent's VmaBlockVector::m_Mutex.
    */
    VMA_MUTEX m_MapAndBindMutex;
    VmaMappingHysteresis m_MappingHysteresis;
    uint32_t m_MapCount;
    void* m_pMappedData;
};
#endif // _VMA_DEVICE_MEMORY_BLOCK

#ifndef _VMA_ALLOCATION_T
struct VmaAllocation_T
{
    friend struct VmaDedicatedAllocationListItemTraits;

    enum FLAGS
    {
        FLAG_PERSISTENT_MAP   = 0x01,
        FLAG_MAPPING_ALLOWED  = 0x02,
    };

public:
    enum ALLOCATION_TYPE
    {
        ALLOCATION_TYPE_NONE,
        ALLOCATION_TYPE_BLOCK,
        ALLOCATION_TYPE_DEDICATED,
    };

    // This struct is allocated using VmaPoolAllocator.
    VmaAllocation_T(bool mappingAllowed);
    ~VmaAllocation_T();

    void InitBlockAllocation(
        VmaDeviceMemoryBlock* block,
        VmaAllocHandle allocHandle,
        VkDeviceSize alignment,
        VkDeviceSize size,
        uint32_t memoryTypeIndex,
        VmaSuballocationType suballocationType,
        bool mapped);
    // pMappedData not null means allocation is created with MAPPED flag.
    void InitDedicatedAllocation(
        VmaPool hParentPool,
        uint32_t memoryTypeIndex,
        VkDeviceMemory hMemory,
        VmaSuballocationType suballocationType,
        void* pMappedData,
        VkDeviceSize size);

    ALLOCATION_TYPE GetType() const { return (ALLOCATION_TYPE)m_Type; }
    VkDeviceSize GetAlignment() const { return m_Alignment; }
    VkDeviceSize GetSize() const { return m_Size; }
    void* GetUserData() const { return m_pUserData; }
    const char* GetName() const { return m_pName; }
    VmaSuballocationType GetSuballocationType() const { return (VmaSuballocationType)m_SuballocationType; }

    VmaDeviceMemoryBlock* GetBlock() const { VMA_ASSERT(m_Type == ALLOCATION_TYPE_BLOCK); return m_BlockAllocation.m_Block; }
    uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }
    bool IsPersistentMap() const { return (m_Flags & FLAG_PERSISTENT_MAP) != 0; }
    bool IsMappingAllowed() const { return (m_Flags & FLAG_MAPPING_ALLOWED) != 0; }

    void SetUserData(VmaAllocator hAllocator, void* pUserData) { m_pUserData = pUserData; }
    void SetName(VmaAllocator hAllocator, const char* pName);
    void FreeName(VmaAllocator hAllocator);
    uint8_t SwapBlockAllocation(VmaAllocator hAllocator, VmaAllocation allocation);
    VmaAllocHandle GetAllocHandle() const;
    VkDeviceSize GetOffset() const;
    VmaPool GetParentPool() const;
    VkDeviceMemory GetMemory() const;
    void* GetMappedData() const;

    void BlockAllocMap();
    void BlockAllocUnmap();
    VkResult DedicatedAllocMap(VmaAllocator hAllocator, void** ppData);
    void DedicatedAllocUnmap(VmaAllocator hAllocator);

#if VMA_STATS_STRING_ENABLED
    uint32_t GetBufferImageUsage() const { return m_BufferImageUsage; }

    void InitBufferImageUsage(uint32_t bufferImageUsage);
    void PrintParameters(class VmaJsonWriter& json) const;
#endif

private:
    // Allocation out of VmaDeviceMemoryBlock.
    struct BlockAllocation
    {
        VmaDeviceMemoryBlock* m_Block;
        VmaAllocHandle m_AllocHandle;
    };
    // Allocation for an object that has its own private VkDeviceMemory.
    struct DedicatedAllocation
    {
        VmaPool m_hParentPool; // VK_NULL_HANDLE if not belongs to custom pool.
        VkDeviceMemory m_hMemory;
        void* m_pMappedData; // Not null means memory is mapped.
        VmaAllocation_T* m_Prev;
        VmaAllocation_T* m_Next;
    };
    union
    {
        // Allocation out of VmaDeviceMemoryBlock.
        BlockAllocation m_BlockAllocation;
        // Allocation for an object that has its own private VkDeviceMemory.
        DedicatedAllocation m_DedicatedAllocation;
    };

    VkDeviceSize m_Alignment;
    VkDeviceSize m_Size;
    void* m_pUserData;
    char* m_pName;
    uint32_t m_MemoryTypeIndex;
    uint8_t m_Type; // ALLOCATION_TYPE
    uint8_t m_SuballocationType; // VmaSuballocationType
    // Reference counter for vmaMapMemory()/vmaUnmapMemory().
    uint8_t m_MapCount;
    uint8_t m_Flags; // enum FLAGS
#if VMA_STATS_STRING_ENABLED
    uint32_t m_BufferImageUsage; // 0 if unknown.
#endif
};
#endif // _VMA_ALLOCATION_T

#ifndef _VMA_DEDICATED_ALLOCATION_LIST_ITEM_TRAITS
struct VmaDedicatedAllocationListItemTraits
{
    typedef VmaAllocation_T ItemType;

    static ItemType* GetPrev(const ItemType* item)
    {
        VMA_HEAVY_ASSERT(item->GetType() == VmaAllocation_T::ALLOCATION_TYPE_DEDICATED);
        return item->m_DedicatedAllocation.m_Prev;
    }
    static ItemType* GetNext(const ItemType* item)
    {
        VMA_HEAVY_ASSERT(item->GetType() == VmaAllocation_T::ALLOCATION_TYPE_DEDICATED);
        return item->m_DedicatedAllocation.m_Next;
    }
    static ItemType*& AccessPrev(ItemType* item)
    {
        VMA_HEAVY_ASSERT(item->GetType() == VmaAllocation_T::ALLOCATION_TYPE_DEDICATED);
        return item->m_DedicatedAllocation.m_Prev;
    }
    static ItemType*& AccessNext(ItemType* item)
    {
        VMA_HEAVY_ASSERT(item->GetType() == VmaAllocation_T::ALLOCATION_TYPE_DEDICATED);
        return item->m_DedicatedAllocation.m_Next;
    }
};
#endif // _VMA_DEDICATED_ALLOCATION_LIST_ITEM_TRAITS

#ifndef _VMA_DEDICATED_ALLOCATION_LIST
/*
Stores linked list of VmaAllocation_T objects.
Thread-safe, synchronized internally.
*/
class VmaDedicatedAllocationList
{
public:
    VmaDedicatedAllocationList() {}
    ~VmaDedicatedAllocationList();

    void Init(bool useMutex) { m_UseMutex = useMutex; }
    bool Validate();

    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats);
    void AddStatistics(VmaStatistics& inoutStats);
#if VMA_STATS_STRING_ENABLED
    // Writes JSON array with the list of allocations.
    void BuildStatsString(VmaJsonWriter& json);
#endif

    bool IsEmpty();
    void Register(VmaAllocation alloc);
    void Unregister(VmaAllocation alloc);

private:
    typedef VmaIntrusiveLinkedList<VmaDedicatedAllocationListItemTraits> DedicatedAllocationLinkedList;

    bool m_UseMutex = true;
    VMA_RW_MUTEX m_Mutex;
    DedicatedAllocationLinkedList m_AllocationList;
};

#ifndef _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS

VmaDedicatedAllocationList::~VmaDedicatedAllocationList()
{
    VMA_HEAVY_ASSERT(Validate());

    if (!m_AllocationList.IsEmpty())
    {
        VMA_ASSERT(false && "Unfreed dedicated allocations found!");
    }
}

bool VmaDedicatedAllocationList::Validate()
{
    const size_t declaredCount = m_AllocationList.GetCount();
    size_t actualCount = 0;
    VmaMutexLockRead lock(m_Mutex, m_UseMutex);
    for (VmaAllocation alloc = m_AllocationList.Front();
        alloc != VMA_NULL; alloc = m_AllocationList.GetNext(alloc))
    {
        ++actualCount;
    }
    VMA_VALIDATE(actualCount == declaredCount);

    return true;
}

void VmaDedicatedAllocationList::AddDetailedStatistics(VmaDetailedStatistics& inoutStats)
{
    for(auto* item = m_AllocationList.Front(); item != nullptr; item = DedicatedAllocationLinkedList::GetNext(item))
    {
        const VkDeviceSize size = item->GetSize();
        inoutStats.statistics.blockCount++;
        inoutStats.statistics.blockBytes += size;
        VmaAddDetailedStatisticsAllocation(inoutStats, item->GetSize());
    }
}

void VmaDedicatedAllocationList::AddStatistics(VmaStatistics& inoutStats)
{
    VmaMutexLockRead lock(m_Mutex, m_UseMutex);

    const uint32_t allocCount = (uint32_t)m_AllocationList.GetCount();
    inoutStats.blockCount += allocCount;
    inoutStats.allocationCount += allocCount;

    for(auto* item = m_AllocationList.Front(); item != nullptr; item = DedicatedAllocationLinkedList::GetNext(item))
    {
        const VkDeviceSize size = item->GetSize();
        inoutStats.blockBytes += size;
        inoutStats.allocationBytes += size;
    }
}

#if VMA_STATS_STRING_ENABLED
void VmaDedicatedAllocationList::BuildStatsString(VmaJsonWriter& json)
{
    VmaMutexLockRead lock(m_Mutex, m_UseMutex);
    json.BeginArray();
    for (VmaAllocation alloc = m_AllocationList.Front();
        alloc != VMA_NULL; alloc = m_AllocationList.GetNext(alloc))
    {
        json.BeginObject(true);
        alloc->PrintParameters(json);
        json.EndObject();
    }
    json.EndArray();
}
#endif // VMA_STATS_STRING_ENABLED

bool VmaDedicatedAllocationList::IsEmpty()
{
    VmaMutexLockRead lock(m_Mutex, m_UseMutex);
    return m_AllocationList.IsEmpty();
}

void VmaDedicatedAllocationList::Register(VmaAllocation alloc)
{
    VmaMutexLockWrite lock(m_Mutex, m_UseMutex);
    m_AllocationList.PushBack(alloc);
}

void VmaDedicatedAllocationList::Unregister(VmaAllocation alloc)
{
    VmaMutexLockWrite lock(m_Mutex, m_UseMutex);
    m_AllocationList.Remove(alloc);
}
#endif // _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS
#endif // _VMA_DEDICATED_ALLOCATION_LIST

#ifndef _VMA_SUBALLOCATION
/*
Represents a region of VmaDeviceMemoryBlock that is either assigned and returned as
allocated memory block or free.
*/
struct VmaSuballocation
{
    VkDeviceSize offset;
    VkDeviceSize size;
    void* userData;
    VmaSuballocationType type;
};

// Comparator for offsets.
struct VmaSuballocationOffsetLess
{
    bool operator()(const VmaSuballocation& lhs, const VmaSuballocation& rhs) const
    {
        return lhs.offset < rhs.offset;
    }
};

struct VmaSuballocationOffsetGreater
{
    bool operator()(const VmaSuballocation& lhs, const VmaSuballocation& rhs) const
    {
        return lhs.offset > rhs.offset;
    }
};

struct VmaSuballocationItemSizeLess
{
    bool operator()(const VmaSuballocationList::iterator lhs,
        const VmaSuballocationList::iterator rhs) const
    {
        return lhs->size < rhs->size;
    }

    bool operator()(const VmaSuballocationList::iterator lhs,
        VkDeviceSize rhsSize) const
    {
        return lhs->size < rhsSize;
    }
};
#endif // _VMA_SUBALLOCATION

#ifndef _VMA_ALLOCATION_REQUEST
/*
Parameters of planned allocation inside a VmaDeviceMemoryBlock.
item points to a FREE suballocation.
*/
struct VmaAllocationRequest
{
    VmaAllocHandle allocHandle;
    VkDeviceSize size;
    VmaSuballocationList::iterator item;
    void* customData;
    uint64_t algorithmData;
    VmaAllocationRequestType type;
};
#endif // _VMA_ALLOCATION_REQUEST

#ifndef _VMA_BLOCK_METADATA
/*
Data structure used for bookkeeping of allocations and unused ranges of memory
in a single VkDeviceMemory block.
*/
class VmaBlockMetadata
{
public:
    // pAllocationCallbacks, if not null, must be owned externally - alive and unchanged for the whole lifetime of this object.
    VmaBlockMetadata(const VkAllocationCallbacks* pAllocationCallbacks,
        VkDeviceSize bufferImageGranularity, bool isVirtual);
    virtual ~VmaBlockMetadata() = default;

    virtual void Init(VkDeviceSize size) { m_Size = size; }
    bool IsVirtual() const { return m_IsVirtual; }
    VkDeviceSize GetSize() const { return m_Size; }

    // Validates all data structures inside this object. If not valid, returns false.
    virtual bool Validate() const = 0;
    virtual size_t GetAllocationCount() const = 0;
    virtual size_t GetFreeRegionsCount() const = 0;
    virtual VkDeviceSize GetSumFreeSize() const = 0;
    // Returns true if this block is empty - contains only single free suballocation.
    virtual bool IsEmpty() const = 0;
    virtual void GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) = 0;
    virtual VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const = 0;
    virtual void* GetAllocationUserData(VmaAllocHandle allocHandle) const = 0;

    virtual VmaAllocHandle GetAllocationListBegin() const = 0;
    virtual VmaAllocHandle GetNextAllocation(VmaAllocHandle prevAlloc) const = 0;
    virtual VkDeviceSize GetNextFreeRegionSize(VmaAllocHandle alloc) const = 0;

    // Shouldn't modify blockCount.
    virtual void AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const = 0;
    virtual void AddStatistics(VmaStatistics& inoutStats) const = 0;

#if VMA_STATS_STRING_ENABLED
    virtual void PrintDetailedMap(class VmaJsonWriter& json) const = 0;
#endif

    // Tries to find a place for suballocation with given parameters inside this block.
    // If succeeded, fills pAllocationRequest and returns true.
    // If failed, returns false.
    virtual bool CreateAllocationRequest(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        bool upperAddress,
        VmaSuballocationType allocType,
        // Always one of VMA_ALLOCATION_CREATE_STRATEGY_* or VMA_ALLOCATION_INTERNAL_STRATEGY_* flags.
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest) = 0;

    virtual VkResult CheckCorruption(const void* pBlockData) = 0;

    // Makes actual allocation based on request. Request must already be checked and valid.
    virtual void Alloc(
        const VmaAllocationRequest& request,
        VmaSuballocationType type,
        void* userData) = 0;

    // Frees suballocation assigned to given memory region.
    virtual void Free(VmaAllocHandle allocHandle) = 0;

    // Frees all allocations.
    // Careful! Don't call it if there are VmaAllocation objects owned by userData of cleared allocations!
    virtual void Clear() = 0;

    virtual void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) = 0;
    virtual void DebugLogAllAllocations() const = 0;

protected:
    const VkAllocationCallbacks* GetAllocationCallbacks() const { return m_pAllocationCallbacks; }
    VkDeviceSize GetBufferImageGranularity() const { return m_BufferImageGranularity; }
    VkDeviceSize GetDebugMargin() const { return IsVirtual() ? 0 : VMA_DEBUG_MARGIN; }

    void DebugLogAllocation(VkDeviceSize offset, VkDeviceSize size, void* userData) const;
#if VMA_STATS_STRING_ENABLED
    // mapRefCount == UINT32_MAX means unspecified.
    void PrintDetailedMap_Begin(class VmaJsonWriter& json,
        VkDeviceSize unusedBytes,
        size_t allocationCount,
        size_t unusedRangeCount) const;
    void PrintDetailedMap_Allocation(class VmaJsonWriter& json,
        VkDeviceSize offset, VkDeviceSize size, void* userData) const;
    void PrintDetailedMap_UnusedRange(class VmaJsonWriter& json,
        VkDeviceSize offset,
        VkDeviceSize size) const;
    void PrintDetailedMap_End(class VmaJsonWriter& json) const;
#endif

private:
    VkDeviceSize m_Size;
    const VkAllocationCallbacks* m_pAllocationCallbacks;
    const VkDeviceSize m_BufferImageGranularity;
    const bool m_IsVirtual;
};

#ifndef _VMA_BLOCK_METADATA_FUNCTIONS
VmaBlockMetadata::VmaBlockMetadata(const VkAllocationCallbacks* pAllocationCallbacks,
    VkDeviceSize bufferImageGranularity, bool isVirtual)
    : m_Size(0),
    m_pAllocationCallbacks(pAllocationCallbacks),
    m_BufferImageGranularity(bufferImageGranularity),
    m_IsVirtual(isVirtual) {}

void VmaBlockMetadata::DebugLogAllocation(VkDeviceSize offset, VkDeviceSize size, void* userData) const
{
    if (IsVirtual())
    {
        VMA_DEBUG_LOG("UNFREED VIRTUAL ALLOCATION; Offset: %llu; Size: %llu; UserData: %p", offset, size, userData);
    }
    else
    {
        VMA_ASSERT(userData != VMA_NULL);
        VmaAllocation allocation = reinterpret_cast<VmaAllocation>(userData);

        userData = allocation->GetUserData();
        const char* name = allocation->GetName();

#if VMA_STATS_STRING_ENABLED
        VMA_DEBUG_LOG("UNFREED ALLOCATION; Offset: %llu; Size: %llu; UserData: %p; Name: %s; Type: %s; Usage: %u",
            offset, size, userData, name ? name : "vma_empty",
            VMA_SUBALLOCATION_TYPE_NAMES[allocation->GetSuballocationType()],
            allocation->GetBufferImageUsage());
#else
        VMA_DEBUG_LOG("UNFREED ALLOCATION; Offset: %llu; Size: %llu; UserData: %p; Name: %s; Type: %u",
            offset, size, userData, name ? name : "vma_empty",
            (uint32_t)allocation->GetSuballocationType());
#endif // VMA_STATS_STRING_ENABLED
    }

}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata::PrintDetailedMap_Begin(class VmaJsonWriter& json,
    VkDeviceSize unusedBytes, size_t allocationCount, size_t unusedRangeCount) const
{
    json.WriteString("TotalBytes");
    json.WriteNumber(GetSize());

    json.WriteString("UnusedBytes");
    json.WriteSize(unusedBytes);

    json.WriteString("Allocations");
    json.WriteSize(allocationCount);

    json.WriteString("UnusedRanges");
    json.WriteSize(unusedRangeCount);

    json.WriteString("Suballocations");
    json.BeginArray();
}

void VmaBlockMetadata::PrintDetailedMap_Allocation(class VmaJsonWriter& json,
    VkDeviceSize offset, VkDeviceSize size, void* userData) const
{
    json.BeginObject(true);

    json.WriteString("Offset");
    json.WriteNumber(offset);

    if (IsVirtual())
    {
        json.WriteString("Size");
        json.WriteNumber(size);
        if (userData)
        {
            json.WriteString("CustomData");
            json.BeginString();
            json.ContinueString_Pointer(userData);
            json.EndString();
        }
    }
    else
    {
        ((VmaAllocation)userData)->PrintParameters(json);
    }

    json.EndObject();
}

void VmaBlockMetadata::PrintDetailedMap_UnusedRange(class VmaJsonWriter& json,
    VkDeviceSize offset, VkDeviceSize size) const
{
    json.BeginObject(true);

    json.WriteString("Offset");
    json.WriteNumber(offset);

    json.WriteString("Type");
    json.WriteString(VMA_SUBALLOCATION_TYPE_NAMES[VMA_SUBALLOCATION_TYPE_FREE]);

    json.WriteString("Size");
    json.WriteNumber(size);

    json.EndObject();
}

void VmaBlockMetadata::PrintDetailedMap_End(class VmaJsonWriter& json) const
{
    json.EndArray();
}
#endif // VMA_STATS_STRING_ENABLED
#endif // _VMA_BLOCK_METADATA_FUNCTIONS
#endif // _VMA_BLOCK_METADATA

#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY
// Before deleting object of this class remember to call 'Destroy()'
class VmaBlockBufferImageGranularity final
{
public:
    struct ValidationContext
    {
        const VkAllocationCallbacks* allocCallbacks;
        uint16_t* pageAllocs;
    };

    VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity);
    ~VmaBlockBufferImageGranularity();

    bool IsEnabled() const { return m_BufferImageGranularity > MAX_LOW_BUFFER_IMAGE_GRANULARITY; }

    void Init(const VkAllocationCallbacks* pAllocationCallbacks, VkDeviceSize size);
    // Before destroying object you must call free it's memory
    void Destroy(const VkAllocationCallbacks* pAllocationCallbacks);

    void RoundupAllocRequest(VmaSuballocationType allocType,
        VkDeviceSize& inOutAllocSize,
        VkDeviceSize& inOutAllocAlignment) const;

    bool CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
        VkDeviceSize allocSize,
        VkDeviceSize blockOffset,
        VkDeviceSize blockSize,
        VmaSuballocationType allocType) const;

    void AllocPages(uint8_t allocType, VkDeviceSize offset, VkDeviceSize size);
    void FreePages(VkDeviceSize offset, VkDeviceSize size);
    void Clear();

    ValidationContext StartValidation(const VkAllocationCallbacks* pAllocationCallbacks,
        bool isVirutal) const;
    bool Validate(ValidationContext& ctx, VkDeviceSize offset, VkDeviceSize size) const;
    bool FinishValidation(ValidationContext& ctx) const;

private:
    static const uint16_t MAX_LOW_BUFFER_IMAGE_GRANULARITY = 256;

    struct RegionInfo
    {
        uint8_t allocType;
        uint16_t allocCount;
    };

    VkDeviceSize m_BufferImageGranularity;
    uint32_t m_RegionCount;
    RegionInfo* m_RegionInfo;

    uint32_t GetStartPage(VkDeviceSize offset) const { return OffsetToPageIndex(offset & ~(m_BufferImageGranularity - 1)); }
    uint32_t GetEndPage(VkDeviceSize offset, VkDeviceSize size) const { return OffsetToPageIndex((offset + size - 1) & ~(m_BufferImageGranularity - 1)); }

    uint32_t OffsetToPageIndex(VkDeviceSize offset) const;
    void AllocPage(RegionInfo& page, uint8_t allocType);
};

#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
VmaBlockBufferImageGranularity::VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity)
    : m_BufferImageGranularity(bufferImageGranularity),
    m_RegionCount(0),
    m_RegionInfo(VMA_NULL) {}

VmaBlockBufferImageGranularity::~VmaBlockBufferImageGranularity()
{
    VMA_ASSERT(m_RegionInfo == VMA_NULL && "Free not called before destroying object!");
}

void VmaBlockBufferImageGranularity::Init(const VkAllocationCallbacks* pAllocationCallbacks, VkDeviceSize size)
{
    if (IsEnabled())
    {
        m_RegionCount = static_cast<uint32_t>(VmaDivideRoundingUp(size, m_BufferImageGranularity));
        m_RegionInfo = vma_new_array(pAllocationCallbacks, RegionInfo, m_RegionCount);
        memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
    }
}

void VmaBlockBufferImageGranularity::Destroy(const VkAllocationCallbacks* pAllocationCallbacks)
{
    if (m_RegionInfo)
    {
        vma_delete_array(pAllocationCallbacks, m_RegionInfo, m_RegionCount);
        m_RegionInfo = VMA_NULL;
    }
}

void VmaBlockBufferImageGranularity::RoundupAllocRequest(VmaSuballocationType allocType,
    VkDeviceSize& inOutAllocSize,
    VkDeviceSize& inOutAllocAlignment) const
{
    if (m_BufferImageGranularity > 1 &&
        m_BufferImageGranularity <= MAX_LOW_BUFFER_IMAGE_GRANULARITY)
    {
        if (allocType == VMA_SUBALLOCATION_TYPE_UNKNOWN ||
            allocType == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
            allocType == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL)
        {
            inOutAllocAlignment = VMA_MAX(inOutAllocAlignment, m_BufferImageGranularity);
            inOutAllocSize = VmaAlignUp(inOutAllocSize, m_BufferImageGranularity);
        }
    }
}

bool VmaBlockBufferImageGranularity::CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
    VkDeviceSize allocSize,
    VkDeviceSize blockOffset,
    VkDeviceSize blockSize,
    VmaSuballocationType allocType) const
{
    if (IsEnabled())
    {
        uint32_t startPage = GetStartPage(inOutAllocOffset);
        if (m_RegionInfo[startPage].allocCount > 0 &&
            VmaIsBufferImageGranularityConflict(static_cast<VmaSuballocationType>(m_RegionInfo[startPage].allocType), allocType))
        {
            inOutAllocOffset = VmaAlignUp(inOutAllocOffset, m_BufferImageGranularity);
            if (blockSize < allocSize + inOutAllocOffset - blockOffset)
                return true;
            ++startPage;
        }
        uint32_t endPage = GetEndPage(inOutAllocOffset, allocSize);
        if (endPage != startPage &&
            m_RegionInfo[endPage].allocCount > 0 &&
            VmaIsBufferImageGranularityConflict(static_cast<VmaSuballocationType>(m_RegionInfo[endPage].allocType), allocType))
        {
            return true;
        }
    }
    return false;
}

void VmaBlockBufferImageGranularity::AllocPages(uint8_t allocType, VkDeviceSize offset, VkDeviceSize size)
{
    if (IsEnabled())
    {
        uint32_t startPage = GetStartPage(offset);
        AllocPage(m_RegionInfo[startPage], allocType);

        uint32_t endPage = GetEndPage(offset, size);
        if (startPage != endPage)
            AllocPage(m_RegionInfo[endPage], allocType);
    }
}

void VmaBlockBufferImageGranularity::FreePages(VkDeviceSize offset, VkDeviceSize size)
{
    if (IsEnabled())
    {
        uint32_t startPage = GetStartPage(offset);
        --m_RegionInfo[startPage].allocCount;
        if (m_RegionInfo[startPage].allocCount == 0)
            m_RegionInfo[startPage].allocType = VMA_SUBALLOCATION_TYPE_FREE;
        uint32_t endPage = GetEndPage(offset, size);
        if (startPage != endPage)
        {
            --m_RegionInfo[endPage].allocCount;
            if (m_RegionInfo[endPage].allocCount == 0)
                m_RegionInfo[endPage].allocType = VMA_SUBALLOCATION_TYPE_FREE;
        }
    }
}

void VmaBlockBufferImageGranularity::Clear()
{
    if (m_RegionInfo)
        memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
}

VmaBlockBufferImageGranularity::ValidationContext VmaBlockBufferImageGranularity::StartValidation(
    const VkAllocationCallbacks* pAllocationCallbacks, bool isVirutal) const
{
    ValidationContext ctx{ pAllocationCallbacks, VMA_NULL };
    if (!isVirutal && IsEnabled())
    {
        ctx.pageAllocs = vma_new_array(pAllocationCallbacks, uint16_t, m_RegionCount);
        memset(ctx.pageAllocs, 0, m_RegionCount * sizeof(uint16_t));
    }
    return ctx;
}

bool VmaBlockBufferImageGranularity::Validate(ValidationContext& ctx,
    VkDeviceSize offset, VkDeviceSize size) const
{
    if (IsEnabled())
    {
        uint32_t start = GetStartPage(offset);
        ++ctx.pageAllocs[start];
        VMA_VALIDATE(m_RegionInfo[start].allocCount > 0);

        uint32_t end = GetEndPage(offset, size);
        if (start != end)
        {
            ++ctx.pageAllocs[end];
            VMA_VALIDATE(m_RegionInfo[end].allocCount > 0);
        }
    }
    return true;
}

bool VmaBlockBufferImageGranularity::FinishValidation(ValidationContext& ctx) const
{
    // Check proper page structure
    if (IsEnabled())
    {
        VMA_ASSERT(ctx.pageAllocs != VMA_NULL && "Validation context not initialized!");

        for (uint32_t page = 0; page < m_RegionCount; ++page)
        {
            VMA_VALIDATE(ctx.pageAllocs[page] == m_RegionInfo[page].allocCount);
        }
        vma_delete_array(ctx.allocCallbacks, ctx.pageAllocs, m_RegionCount);
        ctx.pageAllocs = VMA_NULL;
    }
    return true;
}

uint32_t VmaBlockBufferImageGranularity::OffsetToPageIndex(VkDeviceSize offset) const
{
    return static_cast<uint32_t>(offset >> VMA_BITSCAN_MSB(m_BufferImageGranularity));
}

void VmaBlockBufferImageGranularity::AllocPage(RegionInfo& page, uint8_t allocType)
{
    // When current alloc type is free then it can be overriden by new type
    if (page.allocCount == 0 || (page.allocCount > 0 && page.allocType == VMA_SUBALLOCATION_TYPE_FREE))
        page.allocType = allocType;

    ++page.allocCount;
}
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY

#if 0
#ifndef _VMA_BLOCK_METADATA_GENERIC
class VmaBlockMetadata_Generic : public VmaBlockMetadata
{
    friend class VmaDefragmentationAlgorithm_Generic;
    friend class VmaDefragmentationAlgorithm_Fast;
    VMA_CLASS_NO_COPY(VmaBlockMetadata_Generic)
public:
    VmaBlockMetadata_Generic(const VkAllocationCallbacks* pAllocationCallbacks,
        VkDeviceSize bufferImageGranularity, bool isVirtual);
    virtual ~VmaBlockMetadata_Generic() = default;

    size_t GetAllocationCount() const override { return m_Suballocations.size() - m_FreeCount; }
    VkDeviceSize GetSumFreeSize() const override { return m_SumFreeSize; }
    bool IsEmpty() const override { return (m_Suballocations.size() == 1) && (m_FreeCount == 1); }
    void Free(VmaAllocHandle allocHandle) override { FreeSuballocation(FindAtOffset((VkDeviceSize)allocHandle - 1)); }
    VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return (VkDeviceSize)allocHandle - 1; };

    void Init(VkDeviceSize size) override;
    bool Validate() const override;

    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const override;
    void AddStatistics(VmaStatistics& inoutStats) const override;

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json, uint32_t mapRefCount) const override;
#endif

    bool CreateAllocationRequest(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        bool upperAddress,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest) override;

    VkResult CheckCorruption(const void* pBlockData) override;

    void Alloc(
        const VmaAllocationRequest& request,
        VmaSuballocationType type,
        void* userData) override;

    void GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) override;
    void* GetAllocationUserData(VmaAllocHandle allocHandle) const override;
    VmaAllocHandle GetAllocationListBegin() const override;
    VmaAllocHandle GetNextAllocation(VmaAllocHandle prevAlloc) const override;
    void Clear() override;
    void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;
    void DebugLogAllAllocations() const override;

private:
    uint32_t m_FreeCount;
    VkDeviceSize m_SumFreeSize;
    VmaSuballocationList m_Suballocations;
    // Suballocations that are free. Sorted by size, ascending.
    VmaVector<VmaSuballocationList::iterator, VmaStlAllocator<VmaSuballocationList::iterator>> m_FreeSuballocationsBySize;

    VkDeviceSize AlignAllocationSize(VkDeviceSize size) const { return IsVirtual() ? size : VmaAlignUp(size, (VkDeviceSize)16); }

    VmaSuballocationList::iterator FindAtOffset(VkDeviceSize offset) const;
    bool ValidateFreeSuballocationList() const;

    // Checks if requested suballocation with given parameters can be placed in given pFreeSuballocItem.
    // If yes, fills pOffset and returns true. If no, returns false.
    bool CheckAllocation(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        VmaSuballocationType allocType,
        VmaSuballocationList::const_iterator suballocItem,
        VmaAllocHandle* pAllocHandle) const;

    // Given free suballocation, it merges it with following one, which must also be free.
    void MergeFreeWithNext(VmaSuballocationList::iterator item);
    // Releases given suballocation, making it free.
    // Merges it with adjacent free suballocations if applicable.
    // Returns iterator to new free suballocation at this place.
    VmaSuballocationList::iterator FreeSuballocation(VmaSuballocationList::iterator suballocItem);
    // Given free suballocation, it inserts it into sorted list of
    // m_FreeSuballocationsBySize if it is suitable.
    void RegisterFreeSuballocation(VmaSuballocationList::iterator item);
    // Given free suballocation, it removes it from sorted list of
    // m_FreeSuballocationsBySize if it is suitable.
    void UnregisterFreeSuballocation(VmaSuballocationList::iterator item);
};

#ifndef _VMA_BLOCK_METADATA_GENERIC_FUNCTIONS
VmaBlockMetadata_Generic::VmaBlockMetadata_Generic(const VkAllocationCallbacks* pAllocationCallbacks,
    VkDeviceSize bufferImageGranularity, bool isVirtual)
    : VmaBlockMetadata(pAllocationCallbacks, bufferImageGranularity, isVirtual),
    m_FreeCount(0),
    m_SumFreeSize(0),
    m_Suballocations(VmaStlAllocator<VmaSuballocation>(pAllocationCallbacks)),
    m_FreeSuballocationsBySize(VmaStlAllocator<VmaSuballocationList::iterator>(pAllocationCallbacks)) {}

void VmaBlockMetadata_Generic::Init(VkDeviceSize size)
{
    VmaBlockMetadata::Init(size);

    m_FreeCount = 1;
    m_SumFreeSize = size;

    VmaSuballocation suballoc = {};
    suballoc.offset = 0;
    suballoc.size = size;
    suballoc.type = VMA_SUBALLOCATION_TYPE_FREE;

    m_Suballocations.push_back(suballoc);
    m_FreeSuballocationsBySize.push_back(m_Suballocations.begin());
}

bool VmaBlockMetadata_Generic::Validate() const
{
    VMA_VALIDATE(!m_Suballocations.empty());

    // Expected offset of new suballocation as calculated from previous ones.
    VkDeviceSize calculatedOffset = 0;
    // Expected number of free suballocations as calculated from traversing their list.
    uint32_t calculatedFreeCount = 0;
    // Expected sum size of free suballocations as calculated from traversing their list.
    VkDeviceSize calculatedSumFreeSize = 0;
    // Expected number of free suballocations that should be registered in
    // m_FreeSuballocationsBySize calculated from traversing their list.
    size_t freeSuballocationsToRegister = 0;
    // True if previous visited suballocation was free.
    bool prevFree = false;

    const VkDeviceSize debugMargin = GetDebugMargin();

    for (const auto& subAlloc : m_Suballocations)
    {
        // Actual offset of this suballocation doesn't match expected one.
        VMA_VALIDATE(subAlloc.offset == calculatedOffset);

        const bool currFree = (subAlloc.type == VMA_SUBALLOCATION_TYPE_FREE);
        // Two adjacent free suballocations are invalid. They should be merged.
        VMA_VALIDATE(!prevFree || !currFree);

        VmaAllocation alloc = (VmaAllocation)subAlloc.userData;
        if (!IsVirtual())
        {
            VMA_VALIDATE(currFree == (alloc == VK_NULL_HANDLE));
        }

        if (currFree)
        {
            calculatedSumFreeSize += subAlloc.size;
            ++calculatedFreeCount;
            ++freeSuballocationsToRegister;

            // Margin required between allocations - every free space must be at least that large.
            VMA_VALIDATE(subAlloc.size >= debugMargin);
        }
        else
        {
            if (!IsVirtual())
            {
                VMA_VALIDATE((VkDeviceSize)alloc->GetAllocHandle() == subAlloc.offset + 1);
                VMA_VALIDATE(alloc->GetSize() == subAlloc.size);
            }

            // Margin required between allocations - previous allocation must be free.
            VMA_VALIDATE(debugMargin == 0 || prevFree);
        }

        calculatedOffset += subAlloc.size;
        prevFree = currFree;
    }

    // Number of free suballocations registered in m_FreeSuballocationsBySize doesn't
    // match expected one.
    VMA_VALIDATE(m_FreeSuballocationsBySize.size() == freeSuballocationsToRegister);

    VkDeviceSize lastSize = 0;
    for (size_t i = 0; i < m_FreeSuballocationsBySize.size(); ++i)
    {
        VmaSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[i];

        // Only free suballocations can be registered in m_FreeSuballocationsBySize.
        VMA_VALIDATE(suballocItem->type == VMA_SUBALLOCATION_TYPE_FREE);
        // They must be sorted by size ascending.
        VMA_VALIDATE(suballocItem->size >= lastSize);

        lastSize = suballocItem->size;
    }

    // Check if totals match calculated values.
    VMA_VALIDATE(ValidateFreeSuballocationList());
    VMA_VALIDATE(calculatedOffset == GetSize());
    VMA_VALIDATE(calculatedSumFreeSize == m_SumFreeSize);
    VMA_VALIDATE(calculatedFreeCount == m_FreeCount);

    return true;
}

void VmaBlockMetadata_Generic::AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const
{
    const uint32_t rangeCount = (uint32_t)m_Suballocations.size();
    inoutStats.statistics.blockCount++;
    inoutStats.statistics.blockBytes += GetSize();

    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
            VmaAddDetailedStatisticsAllocation(inoutStats, suballoc.size);
        else
            VmaAddDetailedStatisticsUnusedRange(inoutStats, suballoc.size);
    }
}

void VmaBlockMetadata_Generic::AddStatistics(VmaStatistics& inoutStats) const
{
    inoutStats.blockCount++;
    inoutStats.allocationCount += (uint32_t)m_Suballocations.size() - m_FreeCount;
    inoutStats.blockBytes += GetSize();
    inoutStats.allocationBytes += GetSize() - m_SumFreeSize;
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata_Generic::PrintDetailedMap(class VmaJsonWriter& json, uint32_t mapRefCount) const
{
    PrintDetailedMap_Begin(json,
        m_SumFreeSize, // unusedBytes
        m_Suballocations.size() - (size_t)m_FreeCount, // allocationCount
        m_FreeCount, // unusedRangeCount
        mapRefCount);

    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            PrintDetailedMap_UnusedRange(json, suballoc.offset, suballoc.size);
        }
        else
        {
            PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.userData);
        }
    }

    PrintDetailedMap_End(json);
}
#endif // VMA_STATS_STRING_ENABLED

bool VmaBlockMetadata_Generic::CreateAllocationRequest(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    bool upperAddress,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    VMA_ASSERT(allocSize > 0);
    VMA_ASSERT(!upperAddress);
    VMA_ASSERT(allocType != VMA_SUBALLOCATION_TYPE_FREE);
    VMA_ASSERT(pAllocationRequest != VMA_NULL);
    VMA_HEAVY_ASSERT(Validate());

    allocSize = AlignAllocationSize(allocSize);

    pAllocationRequest->type = VmaAllocationRequestType::Normal;
    pAllocationRequest->size = allocSize;

    const VkDeviceSize debugMargin = GetDebugMargin();

    // There is not enough total free space in this block to fulfill the request: Early return.
    if (m_SumFreeSize < allocSize + debugMargin)
    {
        return false;
    }

    // New algorithm, efficiently searching freeSuballocationsBySize.
    const size_t freeSuballocCount = m_FreeSuballocationsBySize.size();
    if (freeSuballocCount > 0)
    {
        if (strategy == 0 ||
            strategy == VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT)
        {
            // Find first free suballocation with size not less than allocSize + debugMargin.
            VmaSuballocationList::iterator* const it = VmaBinaryFindFirstNotLess(
                m_FreeSuballocationsBySize.data(),
                m_FreeSuballocationsBySize.data() + freeSuballocCount,
                allocSize + debugMargin,
                VmaSuballocationItemSizeLess());
            size_t index = it - m_FreeSuballocationsBySize.data();
            for (; index < freeSuballocCount; ++index)
            {
                if (CheckAllocation(
                    allocSize,
                    allocAlignment,
                    allocType,
                    m_FreeSuballocationsBySize[index],
                    &pAllocationRequest->allocHandle))
                {
                    pAllocationRequest->item = m_FreeSuballocationsBySize[index];
                    return true;
                }
            }
        }
        else if (strategy == VMA_ALLOCATION_INTERNAL_STRATEGY_MIN_OFFSET)
        {
            for (VmaSuballocationList::iterator it = m_Suballocations.begin();
                it != m_Suballocations.end();
                ++it)
            {
                if (it->type == VMA_SUBALLOCATION_TYPE_FREE && CheckAllocation(
                    allocSize,
                    allocAlignment,
                    allocType,
                    it,
                    &pAllocationRequest->allocHandle))
                {
                    pAllocationRequest->item = it;
                    return true;
                }
            }
        }
        else
        {
            VMA_ASSERT(strategy & (VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT ));
            // Search staring from biggest suballocations.
            for (size_t index = freeSuballocCount; index--; )
            {
                if (CheckAllocation(
                    allocSize,
                    allocAlignment,
                    allocType,
                    m_FreeSuballocationsBySize[index],
                    &pAllocationRequest->allocHandle))
                {
                    pAllocationRequest->item = m_FreeSuballocationsBySize[index];
                    return true;
                }
            }
        }
    }

    return false;
}

VkResult VmaBlockMetadata_Generic::CheckCorruption(const void* pBlockData)
{
    for (auto& suballoc : m_Suballocations)
    {
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
        {
            if (!VmaValidateMagicValue(pBlockData, suballoc.offset + suballoc.size))
            {
                VMA_ASSERT(0 && "MEMORY CORRUPTION DETECTED AFTER VALIDATED ALLOCATION!");
                return VK_ERROR_UNKNOWN_COPY;
            }
        }
    }

    return VK_SUCCESS;
}

void VmaBlockMetadata_Generic::Alloc(
    const VmaAllocationRequest& request,
    VmaSuballocationType type,
    void* userData)
{
    VMA_ASSERT(request.type == VmaAllocationRequestType::Normal);
    VMA_ASSERT(request.item != m_Suballocations.end());
    VmaSuballocation& suballoc = *request.item;
    // Given suballocation is a free block.
    VMA_ASSERT(suballoc.type == VMA_SUBALLOCATION_TYPE_FREE);

    // Given offset is inside this suballocation.
    VMA_ASSERT((VkDeviceSize)request.allocHandle - 1 >= suballoc.offset);
    const VkDeviceSize paddingBegin = (VkDeviceSize)request.allocHandle - suballoc.offset - 1;
    VMA_ASSERT(suballoc.size >= paddingBegin + request.size);
    const VkDeviceSize paddingEnd = suballoc.size - paddingBegin - request.size;

    // Unregister this free suballocation from m_FreeSuballocationsBySize and update
    // it to become used.
    UnregisterFreeSuballocation(request.item);

    suballoc.offset = (VkDeviceSize)request.allocHandle - 1;
    suballoc.size = request.size;
    suballoc.type = type;
    suballoc.userData = userData;

    // If there are any free bytes remaining at the end, insert new free suballocation after current one.
    if (paddingEnd)
    {
        VmaSuballocation paddingSuballoc = {};
        paddingSuballoc.offset = suballoc.offset + suballoc.size;
        paddingSuballoc.size = paddingEnd;
        paddingSuballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
        VmaSuballocationList::iterator next = request.item;
        ++next;
        const VmaSuballocationList::iterator paddingEndItem =
            m_Suballocations.insert(next, paddingSuballoc);
        RegisterFreeSuballocation(paddingEndItem);
    }

    // If there are any free bytes remaining at the beginning, insert new free suballocation before current one.
    if (paddingBegin)
    {
        VmaSuballocation paddingSuballoc = {};
        paddingSuballoc.offset = suballoc.offset - paddingBegin;
        paddingSuballoc.size = paddingBegin;
        paddingSuballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
        const VmaSuballocationList::iterator paddingBeginItem =
            m_Suballocations.insert(request.item, paddingSuballoc);
        RegisterFreeSuballocation(paddingBeginItem);
    }

    // Update totals.
    m_FreeCount = m_FreeCount - 1;
    if (paddingBegin > 0)
    {
        ++m_FreeCount;
    }
    if (paddingEnd > 0)
    {
        ++m_FreeCount;
    }
    m_SumFreeSize -= request.size;
}

void VmaBlockMetadata_Generic::GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo)
{
    outInfo.offset = (VkDeviceSize)allocHandle - 1;
    const VmaSuballocation& suballoc = *FindAtOffset(outInfo.offset);
    outInfo.size = suballoc.size;
    outInfo.pUserData = suballoc.userData;
}

void* VmaBlockMetadata_Generic::GetAllocationUserData(VmaAllocHandle allocHandle) const
{
    return FindAtOffset((VkDeviceSize)allocHandle - 1)->userData;
}

VmaAllocHandle VmaBlockMetadata_Generic::GetAllocationListBegin() const
{
    if (IsEmpty())
        return VK_NULL_HANDLE;

    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
            return (VmaAllocHandle)(suballoc.offset + 1);
    }
    VMA_ASSERT(false && "Should contain at least 1 allocation!");
    return VK_NULL_HANDLE;
}

VmaAllocHandle VmaBlockMetadata_Generic::GetNextAllocation(VmaAllocHandle prevAlloc) const
{
    VmaSuballocationList::const_iterator prev = FindAtOffset((VkDeviceSize)prevAlloc - 1);

    for (VmaSuballocationList::const_iterator it = ++prev; it != m_Suballocations.end(); ++it)
    {
        if (it->type != VMA_SUBALLOCATION_TYPE_FREE)
            return (VmaAllocHandle)(it->offset + 1);
    }
    return VK_NULL_HANDLE;
}

void VmaBlockMetadata_Generic::Clear()
{
    const VkDeviceSize size = GetSize();

    VMA_ASSERT(IsVirtual());
    m_FreeCount = 1;
    m_SumFreeSize = size;
    m_Suballocations.clear();
    m_FreeSuballocationsBySize.clear();

    VmaSuballocation suballoc = {};
    suballoc.offset = 0;
    suballoc.size = size;
    suballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
    m_Suballocations.push_back(suballoc);

    m_FreeSuballocationsBySize.push_back(m_Suballocations.begin());
}

void VmaBlockMetadata_Generic::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
    VmaSuballocation& suballoc = *FindAtOffset((VkDeviceSize)allocHandle - 1);
    suballoc.userData = userData;
}

void VmaBlockMetadata_Generic::DebugLogAllAllocations() const
{
    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
            DebugLogAllocation(suballoc.offset, suballoc.size, suballoc.userData);
    }
}

VmaSuballocationList::iterator VmaBlockMetadata_Generic::FindAtOffset(VkDeviceSize offset) const
{
    VMA_HEAVY_ASSERT(!m_Suballocations.empty());
    const VkDeviceSize last = m_Suballocations.rbegin()->offset;
    if (last == offset)
        return m_Suballocations.rbegin().drop_const();
    const VkDeviceSize first = m_Suballocations.begin()->offset;
    if (first == offset)
        return m_Suballocations.begin().drop_const();

    const size_t suballocCount = m_Suballocations.size();
    const VkDeviceSize step = (last - first + m_Suballocations.begin()->size) / suballocCount;
    auto findSuballocation = [&](auto begin, auto end) -> VmaSuballocationList::iterator
    {
        for (auto suballocItem = begin;
            suballocItem != end;
            ++suballocItem)
        {
            if (suballocItem->offset == offset)
                return suballocItem.drop_const();
        }
        VMA_ASSERT(false && "Not found!");
        return m_Suballocations.end().drop_const();
    };
    // If requested offset is closer to the end of range, search from the end
    if (offset - first > suballocCount * step / 2)
    {
        return findSuballocation(m_Suballocations.rbegin(), m_Suballocations.rend());
    }
    return findSuballocation(m_Suballocations.begin(), m_Suballocations.end());
}

bool VmaBlockMetadata_Generic::ValidateFreeSuballocationList() const
{
    VkDeviceSize lastSize = 0;
    for (size_t i = 0, count = m_FreeSuballocationsBySize.size(); i < count; ++i)
    {
        const VmaSuballocationList::iterator it = m_FreeSuballocationsBySize[i];

        VMA_VALIDATE(it->type == VMA_SUBALLOCATION_TYPE_FREE);
        VMA_VALIDATE(it->size >= lastSize);
        lastSize = it->size;
    }
    return true;
}

bool VmaBlockMetadata_Generic::CheckAllocation(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    VmaSuballocationType allocType,
    VmaSuballocationList::const_iterator suballocItem,
    VmaAllocHandle* pAllocHandle) const
{
    VMA_ASSERT(allocSize > 0);
    VMA_ASSERT(allocType != VMA_SUBALLOCATION_TYPE_FREE);
    VMA_ASSERT(suballocItem != m_Suballocations.cend());
    VMA_ASSERT(pAllocHandle != VMA_NULL);

    const VkDeviceSize debugMargin = GetDebugMargin();
    const VkDeviceSize bufferImageGranularity = GetBufferImageGranularity();

    const VmaSuballocation& suballoc = *suballocItem;
    VMA_ASSERT(suballoc.type == VMA_SUBALLOCATION_TYPE_FREE);

    // Size of this suballocation is too small for this request: Early return.
    if (suballoc.size < allocSize)
    {
        return false;
    }

    // Start from offset equal to beginning of this suballocation.
    VkDeviceSize offset = suballoc.offset + (suballocItem == m_Suballocations.cbegin() ? 0 : GetDebugMargin());

    // Apply debugMargin from the end of previous alloc.
    if (debugMargin > 0)
    {
        offset += debugMargin;
    }

    // Apply alignment.
    offset = VmaAlignUp(offset, allocAlignment);

    // Check previous suballocations for BufferImageGranularity conflicts.
    // Make bigger alignment if necessary.
    if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment)
    {
        bool bufferImageGranularityConflict = false;
        VmaSuballocationList::const_iterator prevSuballocItem = suballocItem;
        while (prevSuballocItem != m_Suballocations.cbegin())
        {
            --prevSuballocItem;
            const VmaSuballocation& prevSuballoc = *prevSuballocItem;
            if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, offset, bufferImageGranularity))
            {
                if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
                {
                    bufferImageGranularityConflict = true;
                    break;
                }
            }
            else
                // Already on previous page.
                break;
        }
        if (bufferImageGranularityConflict)
        {
            offset = VmaAlignUp(offset, bufferImageGranularity);
        }
    }

    // Calculate padding at the beginning based on current offset.
    const VkDeviceSize paddingBegin = offset - suballoc.offset;

    // Fail if requested size plus margin after is bigger than size of this suballocation.
    if (paddingBegin + allocSize + debugMargin > suballoc.size)
    {
        return false;
    }

    // Check next suballocations for BufferImageGranularity conflicts.
    // If conflict exists, allocation cannot be made here.
    if (allocSize % bufferImageGranularity || offset % bufferImageGranularity)
    {
        VmaSuballocationList::const_iterator nextSuballocItem = suballocItem;
        ++nextSuballocItem;
        while (nextSuballocItem != m_Suballocations.cend())
        {
            const VmaSuballocation& nextSuballoc = *nextSuballocItem;
            if (VmaBlocksOnSamePage(offset, allocSize, nextSuballoc.offset, bufferImageGranularity))
            {
                if (VmaIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
                {
                    return false;
                }
            }
            else
            {
                // Already on next page.
                break;
            }
            ++nextSuballocItem;
        }
    }

    *pAllocHandle = (VmaAllocHandle)(offset + 1);
    // All tests passed: Success. pAllocHandle is already filled.
    return true;
}

void VmaBlockMetadata_Generic::MergeFreeWithNext(VmaSuballocationList::iterator item)
{
    VMA_ASSERT(item != m_Suballocations.end());
    VMA_ASSERT(item->type == VMA_SUBALLOCATION_TYPE_FREE);

    VmaSuballocationList::iterator nextItem = item;
    ++nextItem;
    VMA_ASSERT(nextItem != m_Suballocations.end());
    VMA_ASSERT(nextItem->type == VMA_SUBALLOCATION_TYPE_FREE);

    item->size += nextItem->size;
    --m_FreeCount;
    m_Suballocations.erase(nextItem);
}

VmaSuballocationList::iterator VmaBlockMetadata_Generic::FreeSuballocation(VmaSuballocationList::iterator suballocItem)
{
    // Change this suballocation to be marked as free.
    VmaSuballocation& suballoc = *suballocItem;
    suballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
    suballoc.userData = VMA_NULL;

    // Update totals.
    ++m_FreeCount;
    m_SumFreeSize += suballoc.size;

    // Merge with previous and/or next suballocation if it's also free.
    bool mergeWithNext = false;
    bool mergeWithPrev = false;

    VmaSuballocationList::iterator nextItem = suballocItem;
    ++nextItem;
    if ((nextItem != m_Suballocations.end()) && (nextItem->type == VMA_SUBALLOCATION_TYPE_FREE))
    {
        mergeWithNext = true;
    }

    VmaSuballocationList::iterator prevItem = suballocItem;
    if (suballocItem != m_Suballocations.begin())
    {
        --prevItem;
        if (prevItem->type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            mergeWithPrev = true;
        }
    }

    if (mergeWithNext)
    {
        UnregisterFreeSuballocation(nextItem);
        MergeFreeWithNext(suballocItem);
    }

    if (mergeWithPrev)
    {
        UnregisterFreeSuballocation(prevItem);
        MergeFreeWithNext(prevItem);
        RegisterFreeSuballocation(prevItem);
        return prevItem;
    }
    else
    {
        RegisterFreeSuballocation(suballocItem);
        return suballocItem;
    }
}

void VmaBlockMetadata_Generic::RegisterFreeSuballocation(VmaSuballocationList::iterator item)
{
    VMA_ASSERT(item->type == VMA_SUBALLOCATION_TYPE_FREE);
    VMA_ASSERT(item->size > 0);

    // You may want to enable this validation at the beginning or at the end of
    // this function, depending on what do you want to check.
    VMA_HEAVY_ASSERT(ValidateFreeSuballocationList());

    if (m_FreeSuballocationsBySize.empty())
    {
        m_FreeSuballocationsBySize.push_back(item);
    }
    else
    {
        VmaVectorInsertSorted<VmaSuballocationItemSizeLess>(m_FreeSuballocationsBySize, item);
    }

    //VMA_HEAVY_ASSERT(ValidateFreeSuballocationList());
}

void VmaBlockMetadata_Generic::UnregisterFreeSuballocation(VmaSuballocationList::iterator item)
{
    VMA_ASSERT(item->type == VMA_SUBALLOCATION_TYPE_FREE);
    VMA_ASSERT(item->size > 0);

    // You may want to enable this validation at the beginning or at the end of
    // this function, depending on what do you want to check.
    VMA_HEAVY_ASSERT(ValidateFreeSuballocationList());

    VmaSuballocationList::iterator* const it = VmaBinaryFindFirstNotLess(
        m_FreeSuballocationsBySize.data(),
        m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(),
        item,
        VmaSuballocationItemSizeLess());
    for (size_t index = it - m_FreeSuballocationsBySize.data();
        index < m_FreeSuballocationsBySize.size();
        ++index)
    {
        if (m_FreeSuballocationsBySize[index] == item)
        {
            VmaVectorRemove(m_FreeSuballocationsBySize, index);
            return;
        }
        VMA_ASSERT((m_FreeSuballocationsBySize[index]->size == item->size) && "Not found.");
    }
    VMA_ASSERT(0 && "Not found.");

    //VMA_HEAVY_ASSERT(ValidateFreeSuballocationList());
}
#endif // _VMA_BLOCK_METADATA_GENERIC_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_GENERIC
#endif // #if 0

#ifndef _VMA_BLOCK_METADATA_LINEAR
/*
Allocations and their references in internal data structure look like this:

if(m_2ndVectorMode == SECOND_VECTOR_EMPTY):

        0 +-------+
          |       |
          |       |
          |       |
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount]
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount + 1]
          +-------+
          |  ...  |
          +-------+
          | Alloc |  1st[1st.size() - 1]
          +-------+
          |       |
          |       |
          |       |
GetSize() +-------+

if(m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER):

        0 +-------+
          | Alloc |  2nd[0]
          +-------+
          | Alloc |  2nd[1]
          +-------+
          |  ...  |
          +-------+
          | Alloc |  2nd[2nd.size() - 1]
          +-------+
          |       |
          |       |
          |       |
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount]
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount + 1]
          +-------+
          |  ...  |
          +-------+
          | Alloc |  1st[1st.size() - 1]
          +-------+
          |       |
GetSize() +-------+

if(m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK):

        0 +-------+
          |       |
          |       |
          |       |
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount]
          +-------+
          | Alloc |  1st[m_1stNullItemsBeginCount + 1]
          +-------+
          |  ...  |
          +-------+
          | Alloc |  1st[1st.size() - 1]
          +-------+
          |       |
          |       |
          |       |
          +-------+
          | Alloc |  2nd[2nd.size() - 1]
          +-------+
          |  ...  |
          +-------+
          | Alloc |  2nd[1]
          +-------+
          | Alloc |  2nd[0]
GetSize() +-------+

*/
class VmaBlockMetadata_Linear : public VmaBlockMetadata
{
    VMA_CLASS_NO_COPY(VmaBlockMetadata_Linear)
public:
    VmaBlockMetadata_Linear(const VkAllocationCallbacks* pAllocationCallbacks,
        VkDeviceSize bufferImageGranularity, bool isVirtual);
    virtual ~VmaBlockMetadata_Linear() = default;

    VkDeviceSize GetSumFreeSize() const override { return m_SumFreeSize; }
    bool IsEmpty() const override { return GetAllocationCount() == 0; }
    VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return (VkDeviceSize)allocHandle - 1; };

    void Init(VkDeviceSize size) override;
    bool Validate() const override;
    size_t GetAllocationCount() const override;
    size_t GetFreeRegionsCount() const override;

    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const override;
    void AddStatistics(VmaStatistics& inoutStats) const override;

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json) const override;
#endif

    bool CreateAllocationRequest(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        bool upperAddress,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest) override;

    VkResult CheckCorruption(const void* pBlockData) override;

    void Alloc(
        const VmaAllocationRequest& request,
        VmaSuballocationType type,
        void* userData) override;

    void Free(VmaAllocHandle allocHandle) override;
    void GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) override;
    void* GetAllocationUserData(VmaAllocHandle allocHandle) const override;
    VmaAllocHandle GetAllocationListBegin() const override;
    VmaAllocHandle GetNextAllocation(VmaAllocHandle prevAlloc) const override;
    VkDeviceSize GetNextFreeRegionSize(VmaAllocHandle alloc) const override;
    void Clear() override;
    void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;
    void DebugLogAllAllocations() const override;

private:
    /*
    There are two suballocation vectors, used in ping-pong way.
    The one with index m_1stVectorIndex is called 1st.
    The one with index (m_1stVectorIndex ^ 1) is called 2nd.
    2nd can be non-empty only when 1st is not empty.
    When 2nd is not empty, m_2ndVectorMode indicates its mode of operation.
    */
    typedef VmaVector<VmaSuballocation, VmaStlAllocator<VmaSuballocation>> SuballocationVectorType;

    enum SECOND_VECTOR_MODE
    {
        SECOND_VECTOR_EMPTY,
        /*
        Suballocations in 2nd vector are created later than the ones in 1st, but they
        all have smaller offset.
        */
        SECOND_VECTOR_RING_BUFFER,
        /*
        Suballocations in 2nd vector are upper side of double stack.
        They all have offsets higher than those in 1st vector.
        Top of this stack means smaller offsets, but higher indices in this vector.
        */
        SECOND_VECTOR_DOUBLE_STACK,
    };

    VkDeviceSize m_SumFreeSize;
    SuballocationVectorType m_Suballocations0, m_Suballocations1;
    uint32_t m_1stVectorIndex;
    SECOND_VECTOR_MODE m_2ndVectorMode;
    // Number of items in 1st vector with hAllocation = null at the beginning.
    size_t m_1stNullItemsBeginCount;
    // Number of other items in 1st vector with hAllocation = null somewhere in the middle.
    size_t m_1stNullItemsMiddleCount;
    // Number of items in 2nd vector with hAllocation = null.
    size_t m_2ndNullItemsCount;

    SuballocationVectorType& AccessSuballocations1st() { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
    SuballocationVectorType& AccessSuballocations2nd() { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }
    const SuballocationVectorType& AccessSuballocations1st() const { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
    const SuballocationVectorType& AccessSuballocations2nd() const { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }

    VmaSuballocation& FindSuballocation(VkDeviceSize offset) const;
    bool ShouldCompact1st() const;
    void CleanupAfterFree();

    bool CreateAllocationRequest_LowerAddress(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest);
    bool CreateAllocationRequest_UpperAddress(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest);
};

#ifndef _VMA_BLOCK_METADATA_LINEAR_FUNCTIONS
VmaBlockMetadata_Linear::VmaBlockMetadata_Linear(const VkAllocationCallbacks* pAllocationCallbacks,
    VkDeviceSize bufferImageGranularity, bool isVirtual)
    : VmaBlockMetadata(pAllocationCallbacks, bufferImageGranularity, isVirtual),
    m_SumFreeSize(0),
    m_Suballocations0(VmaStlAllocator<VmaSuballocation>(pAllocationCallbacks)),
    m_Suballocations1(VmaStlAllocator<VmaSuballocation>(pAllocationCallbacks)),
    m_1stVectorIndex(0),
    m_2ndVectorMode(SECOND_VECTOR_EMPTY),
    m_1stNullItemsBeginCount(0),
    m_1stNullItemsMiddleCount(0),
    m_2ndNullItemsCount(0) {}

void VmaBlockMetadata_Linear::Init(VkDeviceSize size)
{
    VmaBlockMetadata::Init(size);
    m_SumFreeSize = size;
}

bool VmaBlockMetadata_Linear::Validate() const
{
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    VMA_VALIDATE(suballocations2nd.empty() == (m_2ndVectorMode == SECOND_VECTOR_EMPTY));
    VMA_VALIDATE(!suballocations1st.empty() ||
        suballocations2nd.empty() ||
        m_2ndVectorMode != SECOND_VECTOR_RING_BUFFER);

    if (!suballocations1st.empty())
    {
        // Null item at the beginning should be accounted into m_1stNullItemsBeginCount.
        VMA_VALIDATE(suballocations1st[m_1stNullItemsBeginCount].type != VMA_SUBALLOCATION_TYPE_FREE);
        // Null item at the end should be just pop_back().
        VMA_VALIDATE(suballocations1st.back().type != VMA_SUBALLOCATION_TYPE_FREE);
    }
    if (!suballocations2nd.empty())
    {
        // Null item at the end should be just pop_back().
        VMA_VALIDATE(suballocations2nd.back().type != VMA_SUBALLOCATION_TYPE_FREE);
    }

    VMA_VALIDATE(m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount <= suballocations1st.size());
    VMA_VALIDATE(m_2ndNullItemsCount <= suballocations2nd.size());

    VkDeviceSize sumUsedSize = 0;
    const size_t suballoc1stCount = suballocations1st.size();
    const VkDeviceSize debugMargin = GetDebugMargin();
    VkDeviceSize offset = 0;

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const size_t suballoc2ndCount = suballocations2nd.size();
        size_t nullItem2ndCount = 0;
        for (size_t i = 0; i < suballoc2ndCount; ++i)
        {
            const VmaSuballocation& suballoc = suballocations2nd[i];
            const bool currFree = (suballoc.type == VMA_SUBALLOCATION_TYPE_FREE);

            VmaAllocation const alloc = (VmaAllocation)suballoc.userData;
            if (!IsVirtual())
            {
                VMA_VALIDATE(currFree == (alloc == VK_NULL_HANDLE));
            }
            VMA_VALIDATE(suballoc.offset >= offset);

            if (!currFree)
            {
                if (!IsVirtual())
                {
                    VMA_VALIDATE((VkDeviceSize)alloc->GetAllocHandle() == suballoc.offset + 1);
                    VMA_VALIDATE(alloc->GetSize() == suballoc.size);
                }
                sumUsedSize += suballoc.size;
            }
            else
            {
                ++nullItem2ndCount;
            }

            offset = suballoc.offset + suballoc.size + debugMargin;
        }

        VMA_VALIDATE(nullItem2ndCount == m_2ndNullItemsCount);
    }

    for (size_t i = 0; i < m_1stNullItemsBeginCount; ++i)
    {
        const VmaSuballocation& suballoc = suballocations1st[i];
        VMA_VALIDATE(suballoc.type == VMA_SUBALLOCATION_TYPE_FREE &&
            suballoc.userData == VMA_NULL);
    }

    size_t nullItem1stCount = m_1stNullItemsBeginCount;

    for (size_t i = m_1stNullItemsBeginCount; i < suballoc1stCount; ++i)
    {
        const VmaSuballocation& suballoc = suballocations1st[i];
        const bool currFree = (suballoc.type == VMA_SUBALLOCATION_TYPE_FREE);

        VmaAllocation const alloc = (VmaAllocation)suballoc.userData;
        if (!IsVirtual())
        {
            VMA_VALIDATE(currFree == (alloc == VK_NULL_HANDLE));
        }
        VMA_VALIDATE(suballoc.offset >= offset);
        VMA_VALIDATE(i >= m_1stNullItemsBeginCount || currFree);

        if (!currFree)
        {
            if (!IsVirtual())
            {
                VMA_VALIDATE((VkDeviceSize)alloc->GetAllocHandle() == suballoc.offset + 1);
                VMA_VALIDATE(alloc->GetSize() == suballoc.size);
            }
            sumUsedSize += suballoc.size;
        }
        else
        {
            ++nullItem1stCount;
        }

        offset = suballoc.offset + suballoc.size + debugMargin;
    }
    VMA_VALIDATE(nullItem1stCount == m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount);

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        const size_t suballoc2ndCount = suballocations2nd.size();
        size_t nullItem2ndCount = 0;
        for (size_t i = suballoc2ndCount; i--; )
        {
            const VmaSuballocation& suballoc = suballocations2nd[i];
            const bool currFree = (suballoc.type == VMA_SUBALLOCATION_TYPE_FREE);

            VmaAllocation const alloc = (VmaAllocation)suballoc.userData;
            if (!IsVirtual())
            {
                VMA_VALIDATE(currFree == (alloc == VK_NULL_HANDLE));
            }
            VMA_VALIDATE(suballoc.offset >= offset);

            if (!currFree)
            {
                if (!IsVirtual())
                {
                    VMA_VALIDATE((VkDeviceSize)alloc->GetAllocHandle() == suballoc.offset + 1);
                    VMA_VALIDATE(alloc->GetSize() == suballoc.size);
                }
                sumUsedSize += suballoc.size;
            }
            else
            {
                ++nullItem2ndCount;
            }

            offset = suballoc.offset + suballoc.size + debugMargin;
        }

        VMA_VALIDATE(nullItem2ndCount == m_2ndNullItemsCount);
    }

    VMA_VALIDATE(offset <= GetSize());
    VMA_VALIDATE(m_SumFreeSize == GetSize() - sumUsedSize);

    return true;
}

size_t VmaBlockMetadata_Linear::GetAllocationCount() const
{
    return AccessSuballocations1st().size() - m_1stNullItemsBeginCount - m_1stNullItemsMiddleCount +
        AccessSuballocations2nd().size() - m_2ndNullItemsCount;
}

size_t VmaBlockMetadata_Linear::GetFreeRegionsCount() const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    VMA_ASSERT(0);
    return SIZE_MAX;
}

void VmaBlockMetadata_Linear::AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const
{
    const VkDeviceSize size = GetSize();
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    const size_t suballoc1stCount = suballocations1st.size();
    const size_t suballoc2ndCount = suballocations2nd.size();

    inoutStats.statistics.blockCount++;
    inoutStats.statistics.blockBytes += size;

    VkDeviceSize lastOffset = 0;

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const VkDeviceSize freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAllocIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                    VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                VmaAddDetailedStatisticsAllocation(inoutStats, suballoc.size);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    const VkDeviceSize unusedRangeSize = freeSpace2ndTo1stEnd - lastOffset;
                    VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    size_t nextAlloc1stIndex = m_1stNullItemsBeginCount;
    const VkDeviceSize freeSpace1stTo2ndEnd =
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ? suballocations2nd.back().offset : size;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].userData == VMA_NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const VmaSuballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            VmaAddDetailedStatisticsAllocation(inoutStats, suballoc.size);

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            // There is free space from lastOffset to freeSpace1stTo2ndEnd.
            if (lastOffset < freeSpace1stTo2ndEnd)
            {
                const VkDeviceSize unusedRangeSize = freeSpace1stTo2ndEnd - lastOffset;
                VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAllocIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                    VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                VmaAddDetailedStatisticsAllocation(inoutStats, suballoc.size);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                // There is free space from lastOffset to size.
                if (lastOffset < size)
                {
                    const VkDeviceSize unusedRangeSize = size - lastOffset;
                    VmaAddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }
}

void VmaBlockMetadata_Linear::AddStatistics(VmaStatistics& inoutStats) const
{
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    const VkDeviceSize size = GetSize();
    const size_t suballoc1stCount = suballocations1st.size();
    const size_t suballoc2ndCount = suballocations2nd.size();

    inoutStats.blockCount++;
    inoutStats.blockBytes += size;
    inoutStats.allocationBytes += size - m_SumFreeSize;

    VkDeviceSize lastOffset = 0;

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const VkDeviceSize freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = m_1stNullItemsBeginCount;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++inoutStats.allocationCount;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                    const VkDeviceSize unusedRangeSize = freeSpace2ndTo1stEnd - lastOffset;
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    size_t nextAlloc1stIndex = m_1stNullItemsBeginCount;
    const VkDeviceSize freeSpace1stTo2ndEnd =
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ? suballocations2nd.back().offset : size;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].userData == VMA_NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const VmaSuballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            ++inoutStats.allocationCount;

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            if (lastOffset < freeSpace1stTo2ndEnd)
            {
                // There is free space from lastOffset to freeSpace1stTo2ndEnd.
                const VkDeviceSize unusedRangeSize = freeSpace1stTo2ndEnd - lastOffset;
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++inoutStats.allocationCount;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < size)
                {
                    // There is free space from lastOffset to size.
                    const VkDeviceSize unusedRangeSize = size - lastOffset;
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata_Linear::PrintDetailedMap(class VmaJsonWriter& json) const
{
    const VkDeviceSize size = GetSize();
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    const size_t suballoc1stCount = suballocations1st.size();
    const size_t suballoc2ndCount = suballocations2nd.size();

    // FIRST PASS

    size_t unusedRangeCount = 0;
    VkDeviceSize usedBytes = 0;

    VkDeviceSize lastOffset = 0;

    size_t alloc2ndCount = 0;
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const VkDeviceSize freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    ++unusedRangeCount;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++alloc2ndCount;
                usedBytes += suballoc.size;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                    ++unusedRangeCount;
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    size_t nextAlloc1stIndex = m_1stNullItemsBeginCount;
    size_t alloc1stCount = 0;
    const VkDeviceSize freeSpace1stTo2ndEnd =
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ? suballocations2nd.back().offset : size;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].userData == VMA_NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const VmaSuballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                ++unusedRangeCount;
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            ++alloc1stCount;
            usedBytes += suballoc.size;

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            if (lastOffset < size)
            {
                // There is free space from lastOffset to freeSpace1stTo2ndEnd.
                ++unusedRangeCount;
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    ++unusedRangeCount;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++alloc2ndCount;
                usedBytes += suballoc.size;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < size)
                {
                    // There is free space from lastOffset to size.
                    ++unusedRangeCount;
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }

    const VkDeviceSize unusedBytes = size - usedBytes;
    PrintDetailedMap_Begin(json, unusedBytes, alloc1stCount + alloc2ndCount, unusedRangeCount);

    // SECOND PASS
    lastOffset = 0;

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const VkDeviceSize freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.userData);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                    const VkDeviceSize unusedRangeSize = freeSpace2ndTo1stEnd - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    nextAlloc1stIndex = m_1stNullItemsBeginCount;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].userData == VMA_NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const VmaSuballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.userData);

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            if (lastOffset < freeSpace1stTo2ndEnd)
            {
                // There is free space from lastOffset to freeSpace1stTo2ndEnd.
                const VkDeviceSize unusedRangeSize = freeSpace1stTo2ndEnd - lastOffset;
                PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].userData == VMA_NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const VmaSuballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const VkDeviceSize unusedRangeSize = suballoc.offset - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.userData);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < size)
                {
                    // There is free space from lastOffset to size.
                    const VkDeviceSize unusedRangeSize = size - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }

    PrintDetailedMap_End(json);
}
#endif // VMA_STATS_STRING_ENABLED

bool VmaBlockMetadata_Linear::CreateAllocationRequest(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    bool upperAddress,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    VMA_ASSERT(allocSize > 0);
    VMA_ASSERT(allocType != VMA_SUBALLOCATION_TYPE_FREE);
    VMA_ASSERT(pAllocationRequest != VMA_NULL);
    VMA_HEAVY_ASSERT(Validate());
    pAllocationRequest->size = allocSize;
    return upperAddress ?
        CreateAllocationRequest_UpperAddress(
            allocSize, allocAlignment, allocType, strategy, pAllocationRequest) :
        CreateAllocationRequest_LowerAddress(
            allocSize, allocAlignment, allocType, strategy, pAllocationRequest);
}

VkResult VmaBlockMetadata_Linear::CheckCorruption(const void* pBlockData)
{
    VMA_ASSERT(!IsVirtual());
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    for (size_t i = m_1stNullItemsBeginCount, count = suballocations1st.size(); i < count; ++i)
    {
        const VmaSuballocation& suballoc = suballocations1st[i];
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
        {
            if (!VmaValidateMagicValue(pBlockData, suballoc.offset + suballoc.size))
            {
                VMA_ASSERT(0 && "MEMORY CORRUPTION DETECTED AFTER VALIDATED ALLOCATION!");
                return VK_ERROR_UNKNOWN_COPY;
            }
        }
    }

    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    for (size_t i = 0, count = suballocations2nd.size(); i < count; ++i)
    {
        const VmaSuballocation& suballoc = suballocations2nd[i];
        if (suballoc.type != VMA_SUBALLOCATION_TYPE_FREE)
        {
            if (!VmaValidateMagicValue(pBlockData, suballoc.offset + suballoc.size))
            {
                VMA_ASSERT(0 && "MEMORY CORRUPTION DETECTED AFTER VALIDATED ALLOCATION!");
                return VK_ERROR_UNKNOWN_COPY;
            }
        }
    }

    return VK_SUCCESS;
}

void VmaBlockMetadata_Linear::Alloc(
    const VmaAllocationRequest& request,
    VmaSuballocationType type,
    void* userData)
{
    const VkDeviceSize offset = (VkDeviceSize)request.allocHandle - 1;
    const VmaSuballocation newSuballoc = { offset, request.size, userData, type };

    switch (request.type)
    {
    case VmaAllocationRequestType::UpperAddress:
    {
        VMA_ASSERT(m_2ndVectorMode != SECOND_VECTOR_RING_BUFFER &&
            "CRITICAL ERROR: Trying to use linear allocator as double stack while it was already used as ring buffer.");
        SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
        suballocations2nd.push_back(newSuballoc);
        m_2ndVectorMode = SECOND_VECTOR_DOUBLE_STACK;
    }
    break;
    case VmaAllocationRequestType::EndOf1st:
    {
        SuballocationVectorType& suballocations1st = AccessSuballocations1st();

        VMA_ASSERT(suballocations1st.empty() ||
            offset >= suballocations1st.back().offset + suballocations1st.back().size);
        // Check if it fits before the end of the block.
        VMA_ASSERT(offset + request.size <= GetSize());

        suballocations1st.push_back(newSuballoc);
    }
    break;
    case VmaAllocationRequestType::EndOf2nd:
    {
        SuballocationVectorType& suballocations1st = AccessSuballocations1st();
        // New allocation at the end of 2-part ring buffer, so before first allocation from 1st vector.
        VMA_ASSERT(!suballocations1st.empty() &&
            offset + request.size <= suballocations1st[m_1stNullItemsBeginCount].offset);
        SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

        switch (m_2ndVectorMode)
        {
        case SECOND_VECTOR_EMPTY:
            // First allocation from second part ring buffer.
            VMA_ASSERT(suballocations2nd.empty());
            m_2ndVectorMode = SECOND_VECTOR_RING_BUFFER;
            break;
        case SECOND_VECTOR_RING_BUFFER:
            // 2-part ring buffer is already started.
            VMA_ASSERT(!suballocations2nd.empty());
            break;
        case SECOND_VECTOR_DOUBLE_STACK:
            VMA_ASSERT(0 && "CRITICAL ERROR: Trying to use linear allocator as ring buffer while it was already used as double stack.");
            break;
        default:
            VMA_ASSERT(0);
        }

        suballocations2nd.push_back(newSuballoc);
    }
    break;
    default:
        VMA_ASSERT(0 && "CRITICAL INTERNAL ERROR.");
    }

    m_SumFreeSize -= newSuballoc.size;
}

void VmaBlockMetadata_Linear::Free(VmaAllocHandle allocHandle)
{
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    VkDeviceSize offset = (VkDeviceSize)allocHandle - 1;

    if (!suballocations1st.empty())
    {
        // First allocation: Mark it as next empty at the beginning.
        VmaSuballocation& firstSuballoc = suballocations1st[m_1stNullItemsBeginCount];
        if (firstSuballoc.offset == offset)
        {
            firstSuballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
            firstSuballoc.userData = VMA_NULL;
            m_SumFreeSize += firstSuballoc.size;
            ++m_1stNullItemsBeginCount;
            CleanupAfterFree();
            return;
        }
    }

    // Last allocation in 2-part ring buffer or top of upper stack (same logic).
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ||
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        VmaSuballocation& lastSuballoc = suballocations2nd.back();
        if (lastSuballoc.offset == offset)
        {
            m_SumFreeSize += lastSuballoc.size;
            suballocations2nd.pop_back();
            CleanupAfterFree();
            return;
        }
    }
    // Last allocation in 1st vector.
    else if (m_2ndVectorMode == SECOND_VECTOR_EMPTY)
    {
        VmaSuballocation& lastSuballoc = suballocations1st.back();
        if (lastSuballoc.offset == offset)
        {
            m_SumFreeSize += lastSuballoc.size;
            suballocations1st.pop_back();
            CleanupAfterFree();
            return;
        }
    }

    VmaSuballocation refSuballoc;
    refSuballoc.offset = offset;
    // Rest of members stays uninitialized intentionally for better performance.

    // Item from the middle of 1st vector.
    {
        const SuballocationVectorType::iterator it = VmaBinaryFindSorted(
            suballocations1st.begin() + m_1stNullItemsBeginCount,
            suballocations1st.end(),
            refSuballoc,
            VmaSuballocationOffsetLess());
        if (it != suballocations1st.end())
        {
            it->type = VMA_SUBALLOCATION_TYPE_FREE;
            it->userData = VMA_NULL;
            ++m_1stNullItemsMiddleCount;
            m_SumFreeSize += it->size;
            CleanupAfterFree();
            return;
        }
    }

    if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
    {
        // Item from the middle of 2nd vector.
        const SuballocationVectorType::iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
            VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetLess()) :
            VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetGreater());
        if (it != suballocations2nd.end())
        {
            it->type = VMA_SUBALLOCATION_TYPE_FREE;
            it->userData = VMA_NULL;
            ++m_2ndNullItemsCount;
            m_SumFreeSize += it->size;
            CleanupAfterFree();
            return;
        }
    }

    VMA_ASSERT(0 && "Allocation to free not found in linear allocator!");
}

void VmaBlockMetadata_Linear::GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo)
{
    outInfo.offset = (VkDeviceSize)allocHandle - 1;
    VmaSuballocation& suballoc = FindSuballocation(outInfo.offset);
    outInfo.size = suballoc.size;
    outInfo.pUserData = suballoc.userData;
}

void* VmaBlockMetadata_Linear::GetAllocationUserData(VmaAllocHandle allocHandle) const
{
    return FindSuballocation((VkDeviceSize)allocHandle - 1).userData;
}

VmaAllocHandle VmaBlockMetadata_Linear::GetAllocationListBegin() const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    VMA_ASSERT(0);
    return VK_NULL_HANDLE;
}

VmaAllocHandle VmaBlockMetadata_Linear::GetNextAllocation(VmaAllocHandle prevAlloc) const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    VMA_ASSERT(0);
    return VK_NULL_HANDLE;
}

VkDeviceSize VmaBlockMetadata_Linear::GetNextFreeRegionSize(VmaAllocHandle alloc) const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    VMA_ASSERT(0);
    return 0;
}

void VmaBlockMetadata_Linear::Clear()
{
    m_SumFreeSize = GetSize();
    m_Suballocations0.clear();
    m_Suballocations1.clear();
    // Leaving m_1stVectorIndex unchanged - it doesn't matter.
    m_2ndVectorMode = SECOND_VECTOR_EMPTY;
    m_1stNullItemsBeginCount = 0;
    m_1stNullItemsMiddleCount = 0;
    m_2ndNullItemsCount = 0;
}

void VmaBlockMetadata_Linear::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
    VmaSuballocation& suballoc = FindSuballocation((VkDeviceSize)allocHandle - 1);
    suballoc.userData = userData;
}

void VmaBlockMetadata_Linear::DebugLogAllAllocations() const
{
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    for (auto it = suballocations1st.begin() + m_1stNullItemsBeginCount; it != suballocations1st.end(); ++it)
        if (it->type != VMA_SUBALLOCATION_TYPE_FREE)
            DebugLogAllocation(it->offset, it->size, it->userData);

    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    for (auto it = suballocations2nd.begin(); it != suballocations2nd.end(); ++it)
        if (it->type != VMA_SUBALLOCATION_TYPE_FREE)
            DebugLogAllocation(it->offset, it->size, it->userData);
}

VmaSuballocation& VmaBlockMetadata_Linear::FindSuballocation(VkDeviceSize offset) const
{
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    VmaSuballocation refSuballoc;
    refSuballoc.offset = offset;
    // Rest of members stays uninitialized intentionally for better performance.

    // Item from the 1st vector.
    {
        SuballocationVectorType::const_iterator it = VmaBinaryFindSorted(
            suballocations1st.begin() + m_1stNullItemsBeginCount,
            suballocations1st.end(),
            refSuballoc,
            VmaSuballocationOffsetLess());
        if (it != suballocations1st.end())
        {
            return const_cast<VmaSuballocation&>(*it);
        }
    }

    if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
    {
        // Rest of members stays uninitialized intentionally for better performance.
        SuballocationVectorType::const_iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
            VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetLess()) :
            VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetGreater());
        if (it != suballocations2nd.end())
        {
            return const_cast<VmaSuballocation&>(*it);
        }
    }

    VMA_ASSERT(0 && "Allocation not found in linear allocator!");
    return const_cast<VmaSuballocation&>(suballocations1st.back()); // Should never occur.
}

bool VmaBlockMetadata_Linear::ShouldCompact1st() const
{
    const size_t nullItemCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;
    const size_t suballocCount = AccessSuballocations1st().size();
    return suballocCount > 32 && nullItemCount * 2 >= (suballocCount - nullItemCount) * 3;
}

void VmaBlockMetadata_Linear::CleanupAfterFree()
{
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (IsEmpty())
    {
        suballocations1st.clear();
        suballocations2nd.clear();
        m_1stNullItemsBeginCount = 0;
        m_1stNullItemsMiddleCount = 0;
        m_2ndNullItemsCount = 0;
        m_2ndVectorMode = SECOND_VECTOR_EMPTY;
    }
    else
    {
        const size_t suballoc1stCount = suballocations1st.size();
        const size_t nullItem1stCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;
        VMA_ASSERT(nullItem1stCount <= suballoc1stCount);

        // Find more null items at the beginning of 1st vector.
        while (m_1stNullItemsBeginCount < suballoc1stCount &&
            suballocations1st[m_1stNullItemsBeginCount].type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            ++m_1stNullItemsBeginCount;
            --m_1stNullItemsMiddleCount;
        }

        // Find more null items at the end of 1st vector.
        while (m_1stNullItemsMiddleCount > 0 &&
            suballocations1st.back().type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            --m_1stNullItemsMiddleCount;
            suballocations1st.pop_back();
        }

        // Find more null items at the end of 2nd vector.
        while (m_2ndNullItemsCount > 0 &&
            suballocations2nd.back().type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            --m_2ndNullItemsCount;
            suballocations2nd.pop_back();
        }

        // Find more null items at the beginning of 2nd vector.
        while (m_2ndNullItemsCount > 0 &&
            suballocations2nd[0].type == VMA_SUBALLOCATION_TYPE_FREE)
        {
            --m_2ndNullItemsCount;
            VmaVectorRemove(suballocations2nd, 0);
        }

        if (ShouldCompact1st())
        {
            const size_t nonNullItemCount = suballoc1stCount - nullItem1stCount;
            size_t srcIndex = m_1stNullItemsBeginCount;
            for (size_t dstIndex = 0; dstIndex < nonNullItemCount; ++dstIndex)
            {
                while (suballocations1st[srcIndex].type == VMA_SUBALLOCATION_TYPE_FREE)
                {
                    ++srcIndex;
                }
                if (dstIndex != srcIndex)
                {
                    suballocations1st[dstIndex] = suballocations1st[srcIndex];
                }
                ++srcIndex;
            }
            suballocations1st.resize(nonNullItemCount);
            m_1stNullItemsBeginCount = 0;
            m_1stNullItemsMiddleCount = 0;
        }

        // 2nd vector became empty.
        if (suballocations2nd.empty())
        {
            m_2ndVectorMode = SECOND_VECTOR_EMPTY;
        }

        // 1st vector became empty.
        if (suballocations1st.size() - m_1stNullItemsBeginCount == 0)
        {
            suballocations1st.clear();
            m_1stNullItemsBeginCount = 0;

            if (!suballocations2nd.empty() && m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
            {
                // Swap 1st with 2nd. Now 2nd is empty.
                m_2ndVectorMode = SECOND_VECTOR_EMPTY;
                m_1stNullItemsMiddleCount = m_2ndNullItemsCount;
                while (m_1stNullItemsBeginCount < suballocations2nd.size() &&
                    suballocations2nd[m_1stNullItemsBeginCount].type == VMA_SUBALLOCATION_TYPE_FREE)
                {
                    ++m_1stNullItemsBeginCount;
                    --m_1stNullItemsMiddleCount;
                }
                m_2ndNullItemsCount = 0;
                m_1stVectorIndex ^= 1;
            }
        }
    }

    VMA_HEAVY_ASSERT(Validate());
}

bool VmaBlockMetadata_Linear::CreateAllocationRequest_LowerAddress(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    const VkDeviceSize blockSize = GetSize();
    const VkDeviceSize debugMargin = GetDebugMargin();
    const VkDeviceSize bufferImageGranularity = GetBufferImageGranularity();
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        // Try to allocate at the end of 1st vector.

        VkDeviceSize resultBaseOffset = 0;
        if (!suballocations1st.empty())
        {
            const VmaSuballocation& lastSuballoc = suballocations1st.back();
            resultBaseOffset = lastSuballoc.offset + lastSuballoc.size + debugMargin;
        }

        // Start from offset equal to beginning of free space.
        VkDeviceSize resultOffset = resultBaseOffset;

        // Apply alignment.
        resultOffset = VmaAlignUp(resultOffset, allocAlignment);

        // Check previous suballocations for BufferImageGranularity conflicts.
        // Make bigger alignment if necessary.
        if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations1st.empty())
        {
            bool bufferImageGranularityConflict = false;
            for (size_t prevSuballocIndex = suballocations1st.size(); prevSuballocIndex--; )
            {
                const VmaSuballocation& prevSuballoc = suballocations1st[prevSuballocIndex];
                if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
                {
                    if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
                    {
                        bufferImageGranularityConflict = true;
                        break;
                    }
                }
                else
                    // Already on previous page.
                    break;
            }
            if (bufferImageGranularityConflict)
            {
                resultOffset = VmaAlignUp(resultOffset, bufferImageGranularity);
            }
        }

        const VkDeviceSize freeSpaceEnd = m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ?
            suballocations2nd.back().offset : blockSize;

        // There is enough free space at the end after alignment.
        if (resultOffset + allocSize + debugMargin <= freeSpaceEnd)
        {
            // Check next suballocations for BufferImageGranularity conflicts.
            // If conflict exists, allocation cannot be made here.
            if ((allocSize % bufferImageGranularity || resultOffset % bufferImageGranularity) && m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
            {
                for (size_t nextSuballocIndex = suballocations2nd.size(); nextSuballocIndex--; )
                {
                    const VmaSuballocation& nextSuballoc = suballocations2nd[nextSuballocIndex];
                    if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
                    {
                        if (VmaIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        // Already on previous page.
                        break;
                    }
                }
            }

            // All tests passed: Success.
            pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
            // pAllocationRequest->item, customData unused.
            pAllocationRequest->type = VmaAllocationRequestType::EndOf1st;
            return true;
        }
    }

    // Wrap-around to end of 2nd vector. Try to allocate there, watching for the
    // beginning of 1st vector as the end of free space.
    if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        VMA_ASSERT(!suballocations1st.empty());

        VkDeviceSize resultBaseOffset = 0;
        if (!suballocations2nd.empty())
        {
            const VmaSuballocation& lastSuballoc = suballocations2nd.back();
            resultBaseOffset = lastSuballoc.offset + lastSuballoc.size + debugMargin;
        }

        // Start from offset equal to beginning of free space.
        VkDeviceSize resultOffset = resultBaseOffset;

        // Apply alignment.
        resultOffset = VmaAlignUp(resultOffset, allocAlignment);

        // Check previous suballocations for BufferImageGranularity conflicts.
        // Make bigger alignment if necessary.
        if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations2nd.empty())
        {
            bool bufferImageGranularityConflict = false;
            for (size_t prevSuballocIndex = suballocations2nd.size(); prevSuballocIndex--; )
            {
                const VmaSuballocation& prevSuballoc = suballocations2nd[prevSuballocIndex];
                if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
                {
                    if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
                    {
                        bufferImageGranularityConflict = true;
                        break;
                    }
                }
                else
                    // Already on previous page.
                    break;
            }
            if (bufferImageGranularityConflict)
            {
                resultOffset = VmaAlignUp(resultOffset, bufferImageGranularity);
            }
        }

        size_t index1st = m_1stNullItemsBeginCount;

        // There is enough free space at the end after alignment.
        if ((index1st == suballocations1st.size() && resultOffset + allocSize + debugMargin <= blockSize) ||
            (index1st < suballocations1st.size() && resultOffset + allocSize + debugMargin <= suballocations1st[index1st].offset))
        {
            // Check next suballocations for BufferImageGranularity conflicts.
            // If conflict exists, allocation cannot be made here.
            if (allocSize % bufferImageGranularity || resultOffset % bufferImageGranularity)
            {
                for (size_t nextSuballocIndex = index1st;
                    nextSuballocIndex < suballocations1st.size();
                    nextSuballocIndex++)
                {
                    const VmaSuballocation& nextSuballoc = suballocations1st[nextSuballocIndex];
                    if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
                    {
                        if (VmaIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        // Already on next page.
                        break;
                    }
                }
            }

            // All tests passed: Success.
            pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
            pAllocationRequest->type = VmaAllocationRequestType::EndOf2nd;
            // pAllocationRequest->item, customData unused.
            return true;
        }
    }

    return false;
}

bool VmaBlockMetadata_Linear::CreateAllocationRequest_UpperAddress(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    const VkDeviceSize blockSize = GetSize();
    const VkDeviceSize bufferImageGranularity = GetBufferImageGranularity();
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        VMA_ASSERT(0 && "Trying to use pool with linear algorithm as double stack, while it is already being used as ring buffer.");
        return false;
    }

    // Try to allocate before 2nd.back(), or end of block if 2nd.empty().
    if (allocSize > blockSize)
    {
        return false;
    }
    VkDeviceSize resultBaseOffset = blockSize - allocSize;
    if (!suballocations2nd.empty())
    {
        const VmaSuballocation& lastSuballoc = suballocations2nd.back();
        resultBaseOffset = lastSuballoc.offset - allocSize;
        if (allocSize > lastSuballoc.offset)
        {
            return false;
        }
    }

    // Start from offset equal to end of free space.
    VkDeviceSize resultOffset = resultBaseOffset;

    const VkDeviceSize debugMargin = GetDebugMargin();

    // Apply debugMargin at the end.
    if (debugMargin > 0)
    {
        if (resultOffset < debugMargin)
        {
            return false;
        }
        resultOffset -= debugMargin;
    }

    // Apply alignment.
    resultOffset = VmaAlignDown(resultOffset, allocAlignment);

    // Check next suballocations from 2nd for BufferImageGranularity conflicts.
    // Make bigger alignment if necessary.
    if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations2nd.empty())
    {
        bool bufferImageGranularityConflict = false;
        for (size_t nextSuballocIndex = suballocations2nd.size(); nextSuballocIndex--; )
        {
            const VmaSuballocation& nextSuballoc = suballocations2nd[nextSuballocIndex];
            if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
            {
                if (VmaIsBufferImageGranularityConflict(nextSuballoc.type, allocType))
                {
                    bufferImageGranularityConflict = true;
                    break;
                }
            }
            else
                // Already on previous page.
                break;
        }
        if (bufferImageGranularityConflict)
        {
            resultOffset = VmaAlignDown(resultOffset, bufferImageGranularity);
        }
    }

    // There is enough free space.
    const VkDeviceSize endOf1st = !suballocations1st.empty() ?
        suballocations1st.back().offset + suballocations1st.back().size :
        0;
    if (endOf1st + debugMargin <= resultOffset)
    {
        // Check previous suballocations for BufferImageGranularity conflicts.
        // If conflict exists, allocation cannot be made here.
        if (bufferImageGranularity > 1)
        {
            for (size_t prevSuballocIndex = suballocations1st.size(); prevSuballocIndex--; )
            {
                const VmaSuballocation& prevSuballoc = suballocations1st[prevSuballocIndex];
                if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
                {
                    if (VmaIsBufferImageGranularityConflict(allocType, prevSuballoc.type))
                    {
                        return false;
                    }
                }
                else
                {
                    // Already on next page.
                    break;
                }
            }
        }

        // All tests passed: Success.
        pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
        // pAllocationRequest->item unused.
        pAllocationRequest->type = VmaAllocationRequestType::UpperAddress;
        return true;
    }

    return false;
}
#endif // _VMA_BLOCK_METADATA_LINEAR_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_LINEAR

#if 0
#ifndef _VMA_BLOCK_METADATA_BUDDY
/*
- GetSize() is the original size of allocated memory block.
- m_UsableSize is this size aligned down to a power of two.
  All allocations and calculations happen relative to m_UsableSize.
- GetUnusableSize() is the difference between them.
  It is reported as separate, unused range, not available for allocations.

Node at level 0 has size = m_UsableSize.
Each next level contains nodes with size 2 times smaller than current level.
m_LevelCount is the maximum number of levels to use in the current object.
*/
class VmaBlockMetadata_Buddy : public VmaBlockMetadata
{
    VMA_CLASS_NO_COPY(VmaBlockMetadata_Buddy)
public:
    VmaBlockMetadata_Buddy(const VkAllocationCallbacks* pAllocationCallbacks,
        VkDeviceSize bufferImageGranularity, bool isVirtual);
    virtual ~VmaBlockMetadata_Buddy();

    size_t GetAllocationCount() const override { return m_AllocationCount; }
    VkDeviceSize GetSumFreeSize() const override { return m_SumFreeSize + GetUnusableSize(); }
    bool IsEmpty() const override { return m_Root->type == Node::TYPE_FREE; }
    VkResult CheckCorruption(const void* pBlockData) override { return VK_ERROR_FEATURE_NOT_PRESENT; }
    VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return (VkDeviceSize)allocHandle - 1; };
    void DebugLogAllAllocations() const override { DebugLogAllAllocationNode(m_Root, 0); }

    void Init(VkDeviceSize size) override;
    bool Validate() const override;

    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const override;
    void AddStatistics(VmaStatistics& inoutStats) const override;

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json, uint32_t mapRefCount) const override;
#endif

    bool CreateAllocationRequest(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        bool upperAddress,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest) override;

    void Alloc(
        const VmaAllocationRequest& request,
        VmaSuballocationType type,
        void* userData) override;

    void Free(VmaAllocHandle allocHandle) override;
    void GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) override;
    void* GetAllocationUserData(VmaAllocHandle allocHandle) const override;
    VmaAllocHandle GetAllocationListBegin() const override;
    VmaAllocHandle GetNextAllocation(VmaAllocHandle prevAlloc) const override;
    void Clear() override;
    void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;

private:
    static const size_t MAX_LEVELS = 48;

    struct ValidationContext
    {
        size_t calculatedAllocationCount = 0;
        size_t calculatedFreeCount = 0;
        VkDeviceSize calculatedSumFreeSize = 0;
    };
    struct Node
    {
        VkDeviceSize offset;
        enum TYPE
        {
            TYPE_FREE,
            TYPE_ALLOCATION,
            TYPE_SPLIT,
            TYPE_COUNT
        } type;
        Node* parent;
        Node* buddy;

        union
        {
            struct
            {
                Node* prev;
                Node* next;
            } free;
            struct
            {
                void* userData;
            } allocation;
            struct
            {
                Node* leftChild;
            } split;
        };
    };

    // Size of the memory block aligned down to a power of two.
    VkDeviceSize m_UsableSize;
    uint32_t m_LevelCount;
    VmaPoolAllocator<Node> m_NodeAllocator;
    Node* m_Root;
    struct
    {
        Node* front;
        Node* back;
    } m_FreeList[MAX_LEVELS];

    // Number of nodes in the tree with type == TYPE_ALLOCATION.
    size_t m_AllocationCount;
    // Number of nodes in the tree with type == TYPE_FREE.
    size_t m_FreeCount;
    // Doesn't include space wasted due to internal fragmentation - allocation sizes are just aligned up to node sizes.
    // Doesn't include unusable size.
    VkDeviceSize m_SumFreeSize;

    VkDeviceSize GetUnusableSize() const { return GetSize() - m_UsableSize; }
    VkDeviceSize LevelToNodeSize(uint32_t level) const { return m_UsableSize >> level; }

    VkDeviceSize AlignAllocationSize(VkDeviceSize size) const
    {
        if (!IsVirtual())
        {
            size = VmaAlignUp(size, (VkDeviceSize)16);
        }
        return VmaNextPow2(size);
    }
    Node* FindAllocationNode(VkDeviceSize offset, uint32_t& outLevel) const;
    void DeleteNodeChildren(Node* node);
    bool ValidateNode(ValidationContext& ctx, const Node* parent, const Node* curr, uint32_t level, VkDeviceSize levelNodeSize) const;
    uint32_t AllocSizeToLevel(VkDeviceSize allocSize) const;
    void AddNodeToDetailedStatistics(VmaDetailedStatistics& inoutStats, const Node* node, VkDeviceSize levelNodeSize) const;
    // Adds node to the front of FreeList at given level.
    // node->type must be FREE.
    // node->free.prev, next can be undefined.
    void AddToFreeListFront(uint32_t level, Node* node);
    // Removes node from FreeList at given level.
    // node->type must be FREE.
    // node->free.prev, next stay untouched.
    void RemoveFromFreeList(uint32_t level, Node* node);
    void DebugLogAllAllocationNode(Node* node, uint32_t level) const;

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMapNode(class VmaJsonWriter& json, const Node* node, VkDeviceSize levelNodeSize) const;
#endif
};

#ifndef _VMA_BLOCK_METADATA_BUDDY_FUNCTIONS
VmaBlockMetadata_Buddy::VmaBlockMetadata_Buddy(const VkAllocationCallbacks* pAllocationCallbacks,
    VkDeviceSize bufferImageGranularity, bool isVirtual)
    : VmaBlockMetadata(pAllocationCallbacks, bufferImageGranularity, isVirtual),
    m_NodeAllocator(pAllocationCallbacks, 32), // firstBlockCapacity
    m_Root(VMA_NULL),
    m_AllocationCount(0),
    m_FreeCount(1),
    m_SumFreeSize(0)
{
    memset(m_FreeList, 0, sizeof(m_FreeList));
}

VmaBlockMetadata_Buddy::~VmaBlockMetadata_Buddy()
{
    DeleteNodeChildren(m_Root);
    m_NodeAllocator.Free(m_Root);
}

void VmaBlockMetadata_Buddy::Init(VkDeviceSize size)
{
    VmaBlockMetadata::Init(size);

    m_UsableSize = VmaPrevPow2(size);
    m_SumFreeSize = m_UsableSize;

    // Calculate m_LevelCount.
    const VkDeviceSize minNodeSize = IsVirtual() ? 1 : 16;
    m_LevelCount = 1;
    while (m_LevelCount < MAX_LEVELS &&
        LevelToNodeSize(m_LevelCount) >= minNodeSize)
    {
        ++m_LevelCount;
    }

    Node* rootNode = m_NodeAllocator.Alloc();
    rootNode->offset = 0;
    rootNode->type = Node::TYPE_FREE;
    rootNode->parent = VMA_NULL;
    rootNode->buddy = VMA_NULL;

    m_Root = rootNode;
    AddToFreeListFront(0, rootNode);
}

bool VmaBlockMetadata_Buddy::Validate() const
{
    // Validate tree.
    ValidationContext ctx;
    if (!ValidateNode(ctx, VMA_NULL, m_Root, 0, LevelToNodeSize(0)))
    {
        VMA_VALIDATE(false && "ValidateNode failed.");
    }
    VMA_VALIDATE(m_AllocationCount == ctx.calculatedAllocationCount);
    VMA_VALIDATE(m_SumFreeSize == ctx.calculatedSumFreeSize);

    // Validate free node lists.
    for (uint32_t level = 0; level < m_LevelCount; ++level)
    {
        VMA_VALIDATE(m_FreeList[level].front == VMA_NULL ||
            m_FreeList[level].front->free.prev == VMA_NULL);

        for (Node* node = m_FreeList[level].front;
            node != VMA_NULL;
            node = node->free.next)
        {
            VMA_VALIDATE(node->type == Node::TYPE_FREE);

            if (node->free.next == VMA_NULL)
            {
                VMA_VALIDATE(m_FreeList[level].back == node);
            }
            else
            {
                VMA_VALIDATE(node->free.next->free.prev == node);
            }
        }
    }

    // Validate that free lists ar higher levels are empty.
    for (uint32_t level = m_LevelCount; level < MAX_LEVELS; ++level)
    {
        VMA_VALIDATE(m_FreeList[level].front == VMA_NULL && m_FreeList[level].back == VMA_NULL);
    }

    return true;
}

void VmaBlockMetadata_Buddy::AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const
{
    inoutStats.statistics.blockCount++;
    inoutStats.statistics.blockBytes += GetSize();

    AddNodeToDetailedStatistics(inoutStats, m_Root, LevelToNodeSize(0));

    const VkDeviceSize unusableSize = GetUnusableSize();
    if (unusableSize > 0)
        VmaAddDetailedStatisticsUnusedRange(inoutStats, unusableSize);
}

void VmaBlockMetadata_Buddy::AddStatistics(VmaStatistics& inoutStats) const
{
    inoutStats.blockCount++;
    inoutStats.allocationCount += (uint32_t)m_AllocationCount;
    inoutStats.blockBytes += GetSize();
    inoutStats.allocationBytes += GetSize() - m_SumFreeSize;
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata_Buddy::PrintDetailedMap(class VmaJsonWriter& json, uint32_t mapRefCount) const
{
    VmaDetailedStatistics stats;
    VmaClearDetailedStatistics(stats);
    AddDetailedStatistics(stats);

    PrintDetailedMap_Begin(
        json,
        stats.statistics.blockBytes - stats.statistics.allocationBytes,
        stats.statistics.allocationCount,
        stats.unusedRangeCount,
        mapRefCount);

    PrintDetailedMapNode(json, m_Root, LevelToNodeSize(0));

    const VkDeviceSize unusableSize = GetUnusableSize();
    if (unusableSize > 0)
    {
        PrintDetailedMap_UnusedRange(json,
            m_UsableSize, // offset
            unusableSize); // size
    }

    PrintDetailedMap_End(json);
}
#endif // VMA_STATS_STRING_ENABLED

bool VmaBlockMetadata_Buddy::CreateAllocationRequest(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    bool upperAddress,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    VMA_ASSERT(!upperAddress && "VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT can be used only with linear algorithm.");

    allocSize = AlignAllocationSize(allocSize);

    // Simple way to respect bufferImageGranularity. May be optimized some day.
    // Whenever it might be an OPTIMAL image...
    if (allocType == VMA_SUBALLOCATION_TYPE_UNKNOWN ||
        allocType == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
        allocType == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL)
    {
        allocAlignment = VMA_MAX(allocAlignment, GetBufferImageGranularity());
        allocSize = VmaAlignUp(allocSize, GetBufferImageGranularity());
    }

    if (allocSize > m_UsableSize)
    {
        return false;
    }

    const uint32_t targetLevel = AllocSizeToLevel(allocSize);
    for (uint32_t level = targetLevel; level--; )
    {
        for (Node* freeNode = m_FreeList[level].front;
            freeNode != VMA_NULL;
            freeNode = freeNode->free.next)
        {
            if (freeNode->offset % allocAlignment == 0)
            {
                pAllocationRequest->type = VmaAllocationRequestType::Normal;
                pAllocationRequest->allocHandle = (VmaAllocHandle)(freeNode->offset + 1);
                pAllocationRequest->size = allocSize;
                pAllocationRequest->customData = (void*)(uintptr_t)level;
                return true;
            }
        }
    }

    return false;
}

void VmaBlockMetadata_Buddy::Alloc(
    const VmaAllocationRequest& request,
    VmaSuballocationType type,
    void* userData)
{
    VMA_ASSERT(request.type == VmaAllocationRequestType::Normal);

    const uint32_t targetLevel = AllocSizeToLevel(request.size);
    uint32_t currLevel = (uint32_t)(uintptr_t)request.customData;

    Node* currNode = m_FreeList[currLevel].front;
    VMA_ASSERT(currNode != VMA_NULL && currNode->type == Node::TYPE_FREE);
    const VkDeviceSize offset = (VkDeviceSize)request.allocHandle - 1;
    while (currNode->offset != offset)
    {
        currNode = currNode->free.next;
        VMA_ASSERT(currNode != VMA_NULL && currNode->type == Node::TYPE_FREE);
    }

    // Go down, splitting free nodes.
    while (currLevel < targetLevel)
    {
        // currNode is already first free node at currLevel.
        // Remove it from list of free nodes at this currLevel.
        RemoveFromFreeList(currLevel, currNode);

        const uint32_t childrenLevel = currLevel + 1;

        // Create two free sub-nodes.
        Node* leftChild = m_NodeAllocator.Alloc();
        Node* rightChild = m_NodeAllocator.Alloc();

        leftChild->offset = currNode->offset;
        leftChild->type = Node::TYPE_FREE;
        leftChild->parent = currNode;
        leftChild->buddy = rightChild;

        rightChild->offset = currNode->offset + LevelToNodeSize(childrenLevel);
        rightChild->type = Node::TYPE_FREE;
        rightChild->parent = currNode;
        rightChild->buddy = leftChild;

        // Convert current currNode to split type.
        currNode->type = Node::TYPE_SPLIT;
        currNode->split.leftChild = leftChild;

        // Add child nodes to free list. Order is important!
        AddToFreeListFront(childrenLevel, rightChild);
        AddToFreeListFront(childrenLevel, leftChild);

        ++m_FreeCount;
        ++currLevel;
        currNode = m_FreeList[currLevel].front;

        /*
        We can be sure that currNode, as left child of node previously split,
        also fulfills the alignment requirement.
        */
    }

    // Remove from free list.
    VMA_ASSERT(currLevel == targetLevel &&
        currNode != VMA_NULL &&
        currNode->type == Node::TYPE_FREE);
    RemoveFromFreeList(currLevel, currNode);

    // Convert to allocation node.
    currNode->type = Node::TYPE_ALLOCATION;
    currNode->allocation.userData = userData;

    ++m_AllocationCount;
    --m_FreeCount;
    m_SumFreeSize -= request.size;
}

void VmaBlockMetadata_Buddy::GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo)
{
    uint32_t level = 0;
    outInfo.offset = (VkDeviceSize)allocHandle - 1;
    const Node* const node = FindAllocationNode(outInfo.offset, level);
    outInfo.size = LevelToNodeSize(level);
    outInfo.pUserData = node->allocation.userData;
}

void* VmaBlockMetadata_Buddy::GetAllocationUserData(VmaAllocHandle allocHandle) const
{
    uint32_t level = 0;
    const Node* const node = FindAllocationNode((VkDeviceSize)allocHandle - 1, level);
    return node->allocation.userData;
}

VmaAllocHandle VmaBlockMetadata_Buddy::GetAllocationListBegin() const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    return VK_NULL_HANDLE;
}

VmaAllocHandle VmaBlockMetadata_Buddy::GetNextAllocation(VmaAllocHandle prevAlloc) const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    return VK_NULL_HANDLE;
}

void VmaBlockMetadata_Buddy::DeleteNodeChildren(Node* node)
{
    if (node->type == Node::TYPE_SPLIT)
    {
        DeleteNodeChildren(node->split.leftChild->buddy);
        DeleteNodeChildren(node->split.leftChild);
        const VkAllocationCallbacks* allocationCallbacks = GetAllocationCallbacks();
        m_NodeAllocator.Free(node->split.leftChild->buddy);
        m_NodeAllocator.Free(node->split.leftChild);
    }
}

void VmaBlockMetadata_Buddy::Clear()
{
    DeleteNodeChildren(m_Root);
    m_Root->type = Node::TYPE_FREE;
    m_AllocationCount = 0;
    m_FreeCount = 1;
    m_SumFreeSize = m_UsableSize;
}

void VmaBlockMetadata_Buddy::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
    uint32_t level = 0;
    Node* const node = FindAllocationNode((VkDeviceSize)allocHandle - 1, level);
    node->allocation.userData = userData;
}

VmaBlockMetadata_Buddy::Node* VmaBlockMetadata_Buddy::FindAllocationNode(VkDeviceSize offset, uint32_t& outLevel) const
{
    Node* node = m_Root;
    VkDeviceSize nodeOffset = 0;
    outLevel = 0;
    VkDeviceSize levelNodeSize = LevelToNodeSize(0);
    while (node->type == Node::TYPE_SPLIT)
    {
        const VkDeviceSize nextLevelNodeSize = levelNodeSize >> 1;
        if (offset < nodeOffset + nextLevelNodeSize)
        {
            node = node->split.leftChild;
        }
        else
        {
            node = node->split.leftChild->buddy;
            nodeOffset += nextLevelNodeSize;
        }
        ++outLevel;
        levelNodeSize = nextLevelNodeSize;
    }

    VMA_ASSERT(node != VMA_NULL && node->type == Node::TYPE_ALLOCATION);
    return node;
}

bool VmaBlockMetadata_Buddy::ValidateNode(ValidationContext& ctx, const Node* parent, const Node* curr, uint32_t level, VkDeviceSize levelNodeSize) const
{
    VMA_VALIDATE(level < m_LevelCount);
    VMA_VALIDATE(curr->parent == parent);
    VMA_VALIDATE((curr->buddy == VMA_NULL) == (parent == VMA_NULL));
    VMA_VALIDATE(curr->buddy == VMA_NULL || curr->buddy->buddy == curr);
    switch (curr->type)
    {
    case Node::TYPE_FREE:
        // curr->free.prev, next are validated separately.
        ctx.calculatedSumFreeSize += levelNodeSize;
        ++ctx.calculatedFreeCount;
        break;
    case Node::TYPE_ALLOCATION:
        ++ctx.calculatedAllocationCount;
        if (!IsVirtual())
        {
            VMA_VALIDATE(curr->allocation.userData != VMA_NULL);
        }
        break;
    case Node::TYPE_SPLIT:
    {
        const uint32_t childrenLevel = level + 1;
        const VkDeviceSize childrenLevelNodeSize = levelNodeSize >> 1;
        const Node* const leftChild = curr->split.leftChild;
        VMA_VALIDATE(leftChild != VMA_NULL);
        VMA_VALIDATE(leftChild->offset == curr->offset);
        if (!ValidateNode(ctx, curr, leftChild, childrenLevel, childrenLevelNodeSize))
        {
            VMA_VALIDATE(false && "ValidateNode for left child failed.");
        }
        const Node* const rightChild = leftChild->buddy;
        VMA_VALIDATE(rightChild->offset == curr->offset + childrenLevelNodeSize);
        if (!ValidateNode(ctx, curr, rightChild, childrenLevel, childrenLevelNodeSize))
        {
            VMA_VALIDATE(false && "ValidateNode for right child failed.");
        }
    }
    break;
    default:
        return false;
    }

    return true;
}

uint32_t VmaBlockMetadata_Buddy::AllocSizeToLevel(VkDeviceSize allocSize) const
{
    // I know this could be optimized somehow e.g. by using std::log2p1 from C++20.
    uint32_t level = 0;
    VkDeviceSize currLevelNodeSize = m_UsableSize;
    VkDeviceSize nextLevelNodeSize = currLevelNodeSize >> 1;
    while (allocSize <= nextLevelNodeSize && level + 1 < m_LevelCount)
    {
        ++level;
        currLevelNodeSize >>= 1;
        nextLevelNodeSize >>= 1;
    }
    return level;
}

void VmaBlockMetadata_Buddy::Free(VmaAllocHandle allocHandle)
{
    uint32_t level = 0;
    Node* node = FindAllocationNode((VkDeviceSize)allocHandle - 1, level);

    ++m_FreeCount;
    --m_AllocationCount;
    m_SumFreeSize += LevelToNodeSize(level);

    node->type = Node::TYPE_FREE;

    // Join free nodes if possible.
    while (level > 0 && node->buddy->type == Node::TYPE_FREE)
    {
        RemoveFromFreeList(level, node->buddy);
        Node* const parent = node->parent;

        m_NodeAllocator.Free(node->buddy);
        m_NodeAllocator.Free(node);
        parent->type = Node::TYPE_FREE;

        node = parent;
        --level;
        --m_FreeCount;
    }

    AddToFreeListFront(level, node);
}

void VmaBlockMetadata_Buddy::AddNodeToDetailedStatistics(VmaDetailedStatistics& inoutStats, const Node* node, VkDeviceSize levelNodeSize) const
{
    switch (node->type)
    {
    case Node::TYPE_FREE:
        VmaAddDetailedStatisticsUnusedRange(inoutStats, levelNodeSize);
        break;
    case Node::TYPE_ALLOCATION:
        VmaAddDetailedStatisticsAllocation(inoutStats, levelNodeSize);
        break;
    case Node::TYPE_SPLIT:
    {
        const VkDeviceSize childrenNodeSize = levelNodeSize / 2;
        const Node* const leftChild = node->split.leftChild;
        AddNodeToDetailedStatistics(inoutStats, leftChild, childrenNodeSize);
        const Node* const rightChild = leftChild->buddy;
        AddNodeToDetailedStatistics(inoutStats, rightChild, childrenNodeSize);
    }
    break;
    default:
        VMA_ASSERT(0);
    }
}

void VmaBlockMetadata_Buddy::AddToFreeListFront(uint32_t level, Node* node)
{
    VMA_ASSERT(node->type == Node::TYPE_FREE);

    // List is empty.
    Node* const frontNode = m_FreeList[level].front;
    if (frontNode == VMA_NULL)
    {
        VMA_ASSERT(m_FreeList[level].back == VMA_NULL);
        node->free.prev = node->free.next = VMA_NULL;
        m_FreeList[level].front = m_FreeList[level].back = node;
    }
    else
    {
        VMA_ASSERT(frontNode->free.prev == VMA_NULL);
        node->free.prev = VMA_NULL;
        node->free.next = frontNode;
        frontNode->free.prev = node;
        m_FreeList[level].front = node;
    }
}

void VmaBlockMetadata_Buddy::RemoveFromFreeList(uint32_t level, Node* node)
{
    VMA_ASSERT(m_FreeList[level].front != VMA_NULL);

    // It is at the front.
    if (node->free.prev == VMA_NULL)
    {
        VMA_ASSERT(m_FreeList[level].front == node);
        m_FreeList[level].front = node->free.next;
    }
    else
    {
        Node* const prevFreeNode = node->free.prev;
        VMA_ASSERT(prevFreeNode->free.next == node);
        prevFreeNode->free.next = node->free.next;
    }

    // It is at the back.
    if (node->free.next == VMA_NULL)
    {
        VMA_ASSERT(m_FreeList[level].back == node);
        m_FreeList[level].back = node->free.prev;
    }
    else
    {
        Node* const nextFreeNode = node->free.next;
        VMA_ASSERT(nextFreeNode->free.prev == node);
        nextFreeNode->free.prev = node->free.prev;
    }
}

void VmaBlockMetadata_Buddy::DebugLogAllAllocationNode(Node* node, uint32_t level) const
{
    switch (node->type)
    {
    case Node::TYPE_FREE:
        break;
    case Node::TYPE_ALLOCATION:
        DebugLogAllocation(node->offset, LevelToNodeSize(level), node->allocation.userData);
        break;
    case Node::TYPE_SPLIT:
    {
        ++level;
        DebugLogAllAllocationNode(node->split.leftChild, level);
        DebugLogAllAllocationNode(node->split.leftChild->buddy, level);
    }
    break;
    default:
        VMA_ASSERT(0);
    }
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata_Buddy::PrintDetailedMapNode(class VmaJsonWriter& json, const Node* node, VkDeviceSize levelNodeSize) const
{
    switch (node->type)
    {
    case Node::TYPE_FREE:
        PrintDetailedMap_UnusedRange(json, node->offset, levelNodeSize);
        break;
    case Node::TYPE_ALLOCATION:
        PrintDetailedMap_Allocation(json, node->offset, levelNodeSize, node->allocation.userData);
        break;
    case Node::TYPE_SPLIT:
    {
        const VkDeviceSize childrenNodeSize = levelNodeSize / 2;
        const Node* const leftChild = node->split.leftChild;
        PrintDetailedMapNode(json, leftChild, childrenNodeSize);
        const Node* const rightChild = leftChild->buddy;
        PrintDetailedMapNode(json, rightChild, childrenNodeSize);
    }
    break;
    default:
        VMA_ASSERT(0);
    }
}
#endif // VMA_STATS_STRING_ENABLED
#endif // _VMA_BLOCK_METADATA_BUDDY_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_BUDDY
#endif // #if 0

#ifndef _VMA_BLOCK_METADATA_TLSF
// To not search current larger region if first allocation won't succeed and skip to smaller range
// use with VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT as strategy in CreateAllocationRequest().
// When fragmentation and reusal of previous blocks doesn't matter then use with
// VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT for fastest alloc time possible.
class VmaBlockMetadata_TLSF : public VmaBlockMetadata
{
    VMA_CLASS_NO_COPY(VmaBlockMetadata_TLSF)
public:
    VmaBlockMetadata_TLSF(const VkAllocationCallbacks* pAllocationCallbacks,
        VkDeviceSize bufferImageGranularity, bool isVirtual);
    virtual ~VmaBlockMetadata_TLSF();

    size_t GetAllocationCount() const override { return m_AllocCount; }
    size_t GetFreeRegionsCount() const override { return m_BlocksFreeCount + 1; }
    VkDeviceSize GetSumFreeSize() const override { return m_BlocksFreeSize + m_NullBlock->size; }
    bool IsEmpty() const override { return m_NullBlock->offset == 0; }
    VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return ((Block*)allocHandle)->offset; };

    void Init(VkDeviceSize size) override;
    bool Validate() const override;

    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const override;
    void AddStatistics(VmaStatistics& inoutStats) const override;

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json) const override;
#endif

    bool CreateAllocationRequest(
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        bool upperAddress,
        VmaSuballocationType allocType,
        uint32_t strategy,
        VmaAllocationRequest* pAllocationRequest) override;

    VkResult CheckCorruption(const void* pBlockData) override;
    void Alloc(
        const VmaAllocationRequest& request,
        VmaSuballocationType type,
        void* userData) override;

    void Free(VmaAllocHandle allocHandle) override;
    void GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) override;
    void* GetAllocationUserData(VmaAllocHandle allocHandle) const override;
    VmaAllocHandle GetAllocationListBegin() const override;
    VmaAllocHandle GetNextAllocation(VmaAllocHandle prevAlloc) const override;
    VkDeviceSize GetNextFreeRegionSize(VmaAllocHandle alloc) const override;
    void Clear() override;
    void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;
    void DebugLogAllAllocations() const override;

private:
    // According to original paper it should be preferable 4 or 5:
    // M. Masmano, I. Ripoll, A. Crespo, and J. Real "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"
    // http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
    static const uint8_t SECOND_LEVEL_INDEX = 5;
    static const uint16_t SMALL_BUFFER_SIZE = 256;
    static const uint32_t INITIAL_BLOCK_ALLOC_COUNT = 16;
    static const uint8_t MEMORY_CLASS_SHIFT = 7;
    static const uint8_t MAX_MEMORY_CLASSES = 65 - MEMORY_CLASS_SHIFT;

    class Block
    {
    public:
        VkDeviceSize offset;
        VkDeviceSize size;
        Block* prevPhysical;
        Block* nextPhysical;

        void MarkFree() { prevFree = VMA_NULL; }
        void MarkTaken() { prevFree = this; }
        bool IsFree() const { return prevFree != this; }
        void*& UserData() { VMA_HEAVY_ASSERT(!IsFree()); return userData; }
        Block*& PrevFree() { return prevFree; }
        Block*& NextFree() { VMA_HEAVY_ASSERT(IsFree()); return nextFree; }

    private:
        Block* prevFree; // Address of the same block here indicates that block is taken
        union
        {
            Block* nextFree;
            void* userData;
        };
    };

    size_t m_AllocCount;
    // Total number of free blocks besides null block
    size_t m_BlocksFreeCount;
    // Total size of free blocks excluding null block
    VkDeviceSize m_BlocksFreeSize;
    uint32_t m_IsFreeBitmap;
    uint8_t m_MemoryClasses;
    uint32_t m_InnerIsFreeBitmap[MAX_MEMORY_CLASSES];
    uint32_t m_ListsCount;
    /*
    * 0: 0-3 lists for small buffers
    * 1+: 0-(2^SLI-1) lists for normal buffers
    */
    Block** m_FreeList;
    VmaPoolAllocator<Block> m_BlockAllocator;
    Block* m_NullBlock;
    VmaBlockBufferImageGranularity m_GranularityHandler;

    uint8_t SizeToMemoryClass(VkDeviceSize size) const;
    uint16_t SizeToSecondIndex(VkDeviceSize size, uint8_t memoryClass) const;
    uint32_t GetListIndex(uint8_t memoryClass, uint16_t secondIndex) const;
    uint32_t GetListIndex(VkDeviceSize size) const;

    void RemoveFreeBlock(Block* block);
    void InsertFreeBlock(Block* block);
    void MergeBlock(Block* block, Block* prev);

    Block* FindFreeBlock(VkDeviceSize size, uint32_t& listIndex) const;
    bool CheckBlock(
        Block& block,
        uint32_t listIndex,
        VkDeviceSize allocSize,
        VkDeviceSize allocAlignment,
        VmaSuballocationType allocType,
        VmaAllocationRequest* pAllocationRequest);
};

#ifndef _VMA_BLOCK_METADATA_TLSF_FUNCTIONS
VmaBlockMetadata_TLSF::VmaBlockMetadata_TLSF(const VkAllocationCallbacks* pAllocationCallbacks,
    VkDeviceSize bufferImageGranularity, bool isVirtual)
    : VmaBlockMetadata(pAllocationCallbacks, bufferImageGranularity, isVirtual),
    m_AllocCount(0),
    m_BlocksFreeCount(0),
    m_BlocksFreeSize(0),
    m_IsFreeBitmap(0),
    m_MemoryClasses(0),
    m_ListsCount(0),
    m_FreeList(VMA_NULL),
    m_BlockAllocator(pAllocationCallbacks, INITIAL_BLOCK_ALLOC_COUNT),
    m_NullBlock(VMA_NULL),
    m_GranularityHandler(bufferImageGranularity) {}

VmaBlockMetadata_TLSF::~VmaBlockMetadata_TLSF()
{
    if (m_FreeList)
        vma_delete_array(GetAllocationCallbacks(), m_FreeList, m_ListsCount);
    m_GranularityHandler.Destroy(GetAllocationCallbacks());
}

void VmaBlockMetadata_TLSF::Init(VkDeviceSize size)
{
    VmaBlockMetadata::Init(size);

    if (!IsVirtual())
        m_GranularityHandler.Init(GetAllocationCallbacks(), size);

    m_NullBlock = m_BlockAllocator.Alloc();
    m_NullBlock->size = size;
    m_NullBlock->offset = 0;
    m_NullBlock->prevPhysical = VMA_NULL;
    m_NullBlock->nextPhysical = VMA_NULL;
    m_NullBlock->MarkFree();
    m_NullBlock->NextFree() = VMA_NULL;
    m_NullBlock->PrevFree() = VMA_NULL;
    uint8_t memoryClass = SizeToMemoryClass(size);
    uint16_t sli = SizeToSecondIndex(size, memoryClass);
    m_ListsCount = (memoryClass == 0 ? 0 : (memoryClass - 1) * (1UL << SECOND_LEVEL_INDEX) + sli) + 1;
    if (IsVirtual())
        m_ListsCount += 1UL << SECOND_LEVEL_INDEX;
    else
        m_ListsCount += 4;

    m_MemoryClasses = memoryClass + 2;
    memset(m_InnerIsFreeBitmap, 0, MAX_MEMORY_CLASSES * sizeof(uint32_t));

    m_FreeList = vma_new_array(GetAllocationCallbacks(), Block*, m_ListsCount);
    memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
}

bool VmaBlockMetadata_TLSF::Validate() const
{
    VMA_VALIDATE(GetSumFreeSize() <= GetSize());

    VkDeviceSize calculatedSize = m_NullBlock->size;
    VkDeviceSize calculatedFreeSize = m_NullBlock->size;
    size_t allocCount = 0;
    size_t freeCount = 0;

    // Check integrity of free lists
    for (uint32_t list = 0; list < m_ListsCount; ++list)
    {
        Block* block = m_FreeList[list];
        if (block != VMA_NULL)
        {
            VMA_VALIDATE(block->IsFree());
            VMA_VALIDATE(block->PrevFree() == VMA_NULL);
            while (block->NextFree())
            {
                VMA_VALIDATE(block->NextFree()->IsFree());
                VMA_VALIDATE(block->NextFree()->PrevFree() == block);
                block = block->NextFree();
            }
        }
    }

    VkDeviceSize nextOffset = m_NullBlock->offset;
    auto validateCtx = m_GranularityHandler.StartValidation(GetAllocationCallbacks(), IsVirtual());

    VMA_VALIDATE(m_NullBlock->nextPhysical == VMA_NULL);
    if (m_NullBlock->prevPhysical)
    {
        VMA_VALIDATE(m_NullBlock->prevPhysical->nextPhysical == m_NullBlock);
    }
    // Check all blocks
    for (Block* prev = m_NullBlock->prevPhysical; prev != VMA_NULL; prev = prev->prevPhysical)
    {
        VMA_VALIDATE(prev->offset + prev->size == nextOffset);
        nextOffset = prev->offset;
        calculatedSize += prev->size;

        uint32_t listIndex = GetListIndex(prev->size);
        if (prev->IsFree())
        {
            ++freeCount;
            // Check if free block belongs to free list
            Block* freeBlock = m_FreeList[listIndex];
            VMA_VALIDATE(freeBlock != VMA_NULL);

            bool found = false;
            do
            {
                if (freeBlock == prev)
                    found = true;

                freeBlock = freeBlock->NextFree();
            } while (!found && freeBlock != VMA_NULL);

            VMA_VALIDATE(found);
            calculatedFreeSize += prev->size;
        }
        else
        {
            ++allocCount;
            // Check if taken block is not on a free list
            Block* freeBlock = m_FreeList[listIndex];
            while (freeBlock)
            {
                VMA_VALIDATE(freeBlock != prev);
                freeBlock = freeBlock->NextFree();
            }

            if (!IsVirtual())
            {
                VMA_VALIDATE(m_GranularityHandler.Validate(validateCtx, prev->offset, prev->size));
            }
        }

        if (prev->prevPhysical)
        {
            VMA_VALIDATE(prev->prevPhysical->nextPhysical == prev);
        }
    }

    if (!IsVirtual())
    {
        VMA_VALIDATE(m_GranularityHandler.FinishValidation(validateCtx));
    }

    VMA_VALIDATE(nextOffset == 0);
    VMA_VALIDATE(calculatedSize == GetSize());
    VMA_VALIDATE(calculatedFreeSize == GetSumFreeSize());
    VMA_VALIDATE(allocCount == m_AllocCount);
    VMA_VALIDATE(freeCount == m_BlocksFreeCount);

    return true;
}

void VmaBlockMetadata_TLSF::AddDetailedStatistics(VmaDetailedStatistics& inoutStats) const
{
    inoutStats.statistics.blockCount++;
    inoutStats.statistics.blockBytes += GetSize();
    if (m_NullBlock->size > 0)
        VmaAddDetailedStatisticsUnusedRange(inoutStats, m_NullBlock->size);

    for (Block* block = m_NullBlock->prevPhysical; block != VMA_NULL; block = block->prevPhysical)
    {
        if (block->IsFree())
            VmaAddDetailedStatisticsUnusedRange(inoutStats, block->size);
        else
            VmaAddDetailedStatisticsAllocation(inoutStats, block->size);
    }
}

void VmaBlockMetadata_TLSF::AddStatistics(VmaStatistics& inoutStats) const
{
    inoutStats.blockCount++;
    inoutStats.allocationCount += (uint32_t)m_AllocCount;
    inoutStats.blockBytes += GetSize();
    inoutStats.allocationBytes += GetSize() - GetSumFreeSize();
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockMetadata_TLSF::PrintDetailedMap(class VmaJsonWriter& json) const
{
    size_t blockCount = m_AllocCount + m_BlocksFreeCount;
    VmaStlAllocator<Block*> allocator(GetAllocationCallbacks());
    VmaVector<Block*, VmaStlAllocator<Block*>> blockList(blockCount, allocator);

    size_t i = blockCount;
    for (Block* block = m_NullBlock->prevPhysical; block != VMA_NULL; block = block->prevPhysical)
    {
        blockList[--i] = block;
    }
    VMA_ASSERT(i == 0);

    VmaDetailedStatistics stats;
    VmaClearDetailedStatistics(stats);
    AddDetailedStatistics(stats);

    PrintDetailedMap_Begin(json,
        stats.statistics.blockBytes - stats.statistics.allocationBytes,
        stats.statistics.allocationCount,
        stats.unusedRangeCount);

    for (; i < blockCount; ++i)
    {
        Block* block = blockList[i];
        if (block->IsFree())
            PrintDetailedMap_UnusedRange(json, block->offset, block->size);
        else
            PrintDetailedMap_Allocation(json, block->offset, block->size, block->UserData());
    }
    if (m_NullBlock->size > 0)
        PrintDetailedMap_UnusedRange(json, m_NullBlock->offset, m_NullBlock->size);

    PrintDetailedMap_End(json);
}
#endif

bool VmaBlockMetadata_TLSF::CreateAllocationRequest(
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    bool upperAddress,
    VmaSuballocationType allocType,
    uint32_t strategy,
    VmaAllocationRequest* pAllocationRequest)
{
    VMA_ASSERT(allocSize > 0 && "Cannot allocate empty block!");
    VMA_ASSERT(!upperAddress && "VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT can be used only with linear algorithm.");

    // For small granularity round up
    if (!IsVirtual())
        m_GranularityHandler.RoundupAllocRequest(allocType, allocSize, allocAlignment);

    allocSize += GetDebugMargin();
    // Quick check for too small pool
    if (allocSize > GetSumFreeSize())
        return false;

    // If no free blocks in pool then check only null block
    if (m_BlocksFreeCount == 0)
        return CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest);

    // Round up to the next block
    VkDeviceSize sizeForNextList = allocSize;
    VkDeviceSize smallSizeStep = SMALL_BUFFER_SIZE / (IsVirtual() ? 1 << SECOND_LEVEL_INDEX : 4);
    if (allocSize > SMALL_BUFFER_SIZE)
    {
        sizeForNextList += (1ULL << (VMA_BITSCAN_MSB(allocSize) - SECOND_LEVEL_INDEX));
    }
    else if (allocSize > SMALL_BUFFER_SIZE - smallSizeStep)
        sizeForNextList = SMALL_BUFFER_SIZE + 1;
    else
        sizeForNextList += smallSizeStep;

    uint32_t nextListIndex = 0;
    uint32_t prevListIndex = 0;
    Block* nextListBlock = VMA_NULL;
    Block* prevListBlock = VMA_NULL;

    // Check blocks according to strategies
    if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT)
    {
        // Quick check for larger block first
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        if (nextListBlock != VMA_NULL && CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
            return true;

        // If not fitted then null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
            return true;

        // Null block failed, search larger bucket
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }

        // Failed again, check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }
    }
    else if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT)
    {
        // Check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
            return true;

        // Check larger bucket
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }
    }
    else if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT )
    {
        // Perform search from the start
        VmaStlAllocator<Block*> allocator(GetAllocationCallbacks());
        VmaVector<Block*, VmaStlAllocator<Block*>> blockList(m_BlocksFreeCount, allocator);

        size_t i = m_BlocksFreeCount;
        for (Block* block = m_NullBlock->prevPhysical; block != VMA_NULL; block = block->prevPhysical)
        {
            if (block->IsFree() && block->size >= allocSize)
                blockList[--i] = block;
        }

        for (; i < m_BlocksFreeCount; ++i)
        {
            Block& block = *blockList[i];
            if (CheckBlock(block, GetListIndex(block.size), allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
            return true;

        // Whole range searched, no more memory
        return false;
    }
    else
    {
        // Check larger bucket
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
            return true;

        // Check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }
    }

    // Worst case, full search has to be done
    while (++nextListIndex < m_ListsCount)
    {
        nextListBlock = m_FreeList[nextListIndex];
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }
    }

    // No more memory sadly
    return false;
}

VkResult VmaBlockMetadata_TLSF::CheckCorruption(const void* pBlockData)
{
    for (Block* block = m_NullBlock->prevPhysical; block != VMA_NULL; block = block->prevPhysical)
    {
        if (!block->IsFree())
        {
            if (!VmaValidateMagicValue(pBlockData, block->offset + block->size))
            {
                VMA_ASSERT(0 && "MEMORY CORRUPTION DETECTED AFTER VALIDATED ALLOCATION!");
                return VK_ERROR_UNKNOWN_COPY;
            }
        }
    }

    return VK_SUCCESS;
}

void VmaBlockMetadata_TLSF::Alloc(
    const VmaAllocationRequest& request,
    VmaSuballocationType type,
    void* userData)
{
    VMA_ASSERT(request.type == VmaAllocationRequestType::TLSF);

    // Get block and pop it from the free list
    Block* currentBlock = (Block*)request.allocHandle;
    VkDeviceSize offset = request.algorithmData;
    VMA_ASSERT(currentBlock != VMA_NULL);
    VMA_ASSERT(currentBlock->offset <= offset);

    if (currentBlock != m_NullBlock)
        RemoveFreeBlock(currentBlock);

    VkDeviceSize debugMargin = GetDebugMargin();
    VkDeviceSize misssingAlignment = offset - currentBlock->offset;

    // Append missing alignment to prev block or create new one
    if (misssingAlignment)
    {
        Block* prevBlock = currentBlock->prevPhysical;
        VMA_ASSERT(prevBlock != VMA_NULL && "There should be no missing alignment at offset 0!");

        if (prevBlock->IsFree() && prevBlock->size != debugMargin)
        {
            uint32_t oldList = GetListIndex(prevBlock->size);
            prevBlock->size += misssingAlignment;
            // Check if new size crosses list bucket
            if (oldList != GetListIndex(prevBlock->size))
            {
                prevBlock->size -= misssingAlignment;
                RemoveFreeBlock(prevBlock);
                prevBlock->size += misssingAlignment;
                InsertFreeBlock(prevBlock);
            }
            else
                m_BlocksFreeSize += misssingAlignment;
        }
        else
        {
            Block* newBlock = m_BlockAllocator.Alloc();
            currentBlock->prevPhysical = newBlock;
            prevBlock->nextPhysical = newBlock;
            newBlock->prevPhysical = prevBlock;
            newBlock->nextPhysical = currentBlock;
            newBlock->size = misssingAlignment;
            newBlock->offset = currentBlock->offset;
            newBlock->MarkTaken();

            InsertFreeBlock(newBlock);
        }

        currentBlock->size -= misssingAlignment;
        currentBlock->offset += misssingAlignment;
    }

    VkDeviceSize size = request.size + debugMargin;
    if (currentBlock->size == size)
    {
        if (currentBlock == m_NullBlock)
        {
            // Setup new null block
            m_NullBlock = m_BlockAllocator.Alloc();
            m_NullBlock->size = 0;
            m_NullBlock->offset = currentBlock->offset + size;
            m_NullBlock->prevPhysical = currentBlock;
            m_NullBlock->nextPhysical = VMA_NULL;
            m_NullBlock->MarkFree();
            m_NullBlock->PrevFree() = VMA_NULL;
            m_NullBlock->NextFree() = VMA_NULL;
            currentBlock->nextPhysical = m_NullBlock;
            currentBlock->MarkTaken();
        }
    }
    else
    {
        VMA_ASSERT(currentBlock->size > size && "Proper block already found, shouldn't find smaller one!");

        // Create new free block
        Block* newBlock = m_BlockAllocator.Alloc();
        newBlock->size = currentBlock->size - size;
        newBlock->offset = currentBlock->offset + size;
        newBlock->prevPhysical = currentBlock;
        newBlock->nextPhysical = currentBlock->nextPhysical;
        currentBlock->nextPhysical = newBlock;
        currentBlock->size = size;

        if (currentBlock == m_NullBlock)
        {
            m_NullBlock = newBlock;
            m_NullBlock->MarkFree();
            m_NullBlock->NextFree() = VMA_NULL;
            m_NullBlock->PrevFree() = VMA_NULL;
            currentBlock->MarkTaken();
        }
        else
        {
            newBlock->nextPhysical->prevPhysical = newBlock;
            newBlock->MarkTaken();
            InsertFreeBlock(newBlock);
        }
    }
    currentBlock->UserData() = userData;

    if (debugMargin > 0)
    {
        currentBlock->size -= debugMargin;
        Block* newBlock = m_BlockAllocator.Alloc();
        newBlock->size = debugMargin;
        newBlock->offset = currentBlock->offset + currentBlock->size;
        newBlock->prevPhysical = currentBlock;
        newBlock->nextPhysical = currentBlock->nextPhysical;
        newBlock->MarkTaken();
        currentBlock->nextPhysical->prevPhysical = newBlock;
        currentBlock->nextPhysical = newBlock;
        InsertFreeBlock(newBlock);
    }

    if (!IsVirtual())
        m_GranularityHandler.AllocPages((uint8_t)(uintptr_t)request.customData,
            currentBlock->offset, currentBlock->size);
    ++m_AllocCount;
}

void VmaBlockMetadata_TLSF::Free(VmaAllocHandle allocHandle)
{
    Block* block = (Block*)allocHandle;
    Block* next = block->nextPhysical;
    VMA_ASSERT(!block->IsFree() && "Block is already free!");

    if (!IsVirtual())
        m_GranularityHandler.FreePages(block->offset, block->size);
    --m_AllocCount;

    VkDeviceSize debugMargin = GetDebugMargin();
    if (debugMargin > 0)
    {
        RemoveFreeBlock(next);
        MergeBlock(next, block);
        block = next;
        next = next->nextPhysical;
    }

    // Try merging
    Block* prev = block->prevPhysical;
    if (prev != VMA_NULL && prev->IsFree() && prev->size != debugMargin)
    {
        RemoveFreeBlock(prev);
        MergeBlock(block, prev);
    }

    if (!next->IsFree())
        InsertFreeBlock(block);
    else if (next == m_NullBlock)
        MergeBlock(m_NullBlock, block);
    else
    {
        RemoveFreeBlock(next);
        MergeBlock(next, block);
        InsertFreeBlock(next);
    }
}

void VmaBlockMetadata_TLSF::GetAllocationInfo(VmaAllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo)
{
    Block* block = (Block*)allocHandle;
    VMA_ASSERT(!block->IsFree() && "Cannot get allocation info for free block!");
    outInfo.offset = block->offset;
    outInfo.size = block->size;
    outInfo.pUserData = block->UserData();
}

void* VmaBlockMetadata_TLSF::GetAllocationUserData(VmaAllocHandle allocHandle) const
{
    Block* block = (Block*)allocHandle;
    VMA_ASSERT(!block->IsFree() && "Cannot get user data for free block!");
    return block->UserData();
}

VmaAllocHandle VmaBlockMetadata_TLSF::GetAllocationListBegin() const
{
    if (m_AllocCount == 0)
        return VK_NULL_HANDLE;

    for (Block* block = m_NullBlock->prevPhysical; block; block = block->prevPhysical)
    {
        if (!block->IsFree())
            return (VmaAllocHandle)block;
    }
    VMA_ASSERT(false && "If m_AllocCount > 0 then should find any allocation!");
    return VK_NULL_HANDLE;
}

VmaAllocHandle VmaBlockMetadata_TLSF::GetNextAllocation(VmaAllocHandle prevAlloc) const
{
    Block* startBlock = (Block*)prevAlloc;
    VMA_ASSERT(!startBlock->IsFree() && "Incorrect block!");

    for (Block* block = startBlock->prevPhysical; block; block = block->prevPhysical)
    {
        if (!block->IsFree())
            return (VmaAllocHandle)block;
    }
    return VK_NULL_HANDLE;
}

VkDeviceSize VmaBlockMetadata_TLSF::GetNextFreeRegionSize(VmaAllocHandle alloc) const
{
    Block* block = (Block*)alloc;
    VMA_ASSERT(!block->IsFree() && "Incorrect block!");

    if (block->prevPhysical)
        return block->prevPhysical->IsFree() ? block->prevPhysical->size : 0;
    return 0;
}

void VmaBlockMetadata_TLSF::Clear()
{
    m_AllocCount = 0;
    m_BlocksFreeCount = 0;
    m_BlocksFreeSize = 0;
    m_IsFreeBitmap = 0;
    m_NullBlock->offset = 0;
    m_NullBlock->size = GetSize();
    Block* block = m_NullBlock->prevPhysical;
    m_NullBlock->prevPhysical = VMA_NULL;
    while (block)
    {
        Block* prev = block->prevPhysical;
        m_BlockAllocator.Free(block);
        block = prev;
    }
    memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
    memset(m_InnerIsFreeBitmap, 0, m_MemoryClasses * sizeof(uint32_t));
    m_GranularityHandler.Clear();
}

void VmaBlockMetadata_TLSF::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
    Block* block = (Block*)allocHandle;
    VMA_ASSERT(!block->IsFree() && "Trying to set user data for not allocated block!");
    block->UserData() = userData;
}

void VmaBlockMetadata_TLSF::DebugLogAllAllocations() const
{
    for (Block* block = m_NullBlock->prevPhysical; block != VMA_NULL; block = block->prevPhysical)
        if (!block->IsFree())
            DebugLogAllocation(block->offset, block->size, block->UserData());
}

uint8_t VmaBlockMetadata_TLSF::SizeToMemoryClass(VkDeviceSize size) const
{
    if (size > SMALL_BUFFER_SIZE)
        return VMA_BITSCAN_MSB(size) - MEMORY_CLASS_SHIFT;
    return 0;
}

uint16_t VmaBlockMetadata_TLSF::SizeToSecondIndex(VkDeviceSize size, uint8_t memoryClass) const
{
    if (memoryClass == 0)
    {
        if (IsVirtual())
            return static_cast<uint16_t>((size - 1) / 8);
        else
            return static_cast<uint16_t>((size - 1) / 64);
    }
    return static_cast<uint16_t>((size >> (memoryClass + MEMORY_CLASS_SHIFT - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
}

uint32_t VmaBlockMetadata_TLSF::GetListIndex(uint8_t memoryClass, uint16_t secondIndex) const
{
    if (memoryClass == 0)
        return secondIndex;

    const uint32_t index = static_cast<uint32_t>(memoryClass - 1) * (1 << SECOND_LEVEL_INDEX) + secondIndex;
    if (IsVirtual())
        return index + (1 << SECOND_LEVEL_INDEX);
    else
        return index + 4;
}

uint32_t VmaBlockMetadata_TLSF::GetListIndex(VkDeviceSize size) const
{
    uint8_t memoryClass = SizeToMemoryClass(size);
    return GetListIndex(memoryClass, SizeToSecondIndex(size, memoryClass));
}

void VmaBlockMetadata_TLSF::RemoveFreeBlock(Block* block)
{
    VMA_ASSERT(block != m_NullBlock);
    VMA_ASSERT(block->IsFree());

    if (block->NextFree() != VMA_NULL)
        block->NextFree()->PrevFree() = block->PrevFree();
    if (block->PrevFree() != VMA_NULL)
        block->PrevFree()->NextFree() = block->NextFree();
    else
    {
        uint8_t memClass = SizeToMemoryClass(block->size);
        uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
        uint32_t index = GetListIndex(memClass, secondIndex);
        VMA_ASSERT(m_FreeList[index] == block);
        m_FreeList[index] = block->NextFree();
        if (block->NextFree() == VMA_NULL)
        {
            m_InnerIsFreeBitmap[memClass] &= ~(1U << secondIndex);
            if (m_InnerIsFreeBitmap[memClass] == 0)
                m_IsFreeBitmap &= ~(1UL << memClass);
        }
    }
    block->MarkTaken();
    block->UserData() = VMA_NULL;
    --m_BlocksFreeCount;
    m_BlocksFreeSize -= block->size;
}

void VmaBlockMetadata_TLSF::InsertFreeBlock(Block* block)
{
    VMA_ASSERT(block != m_NullBlock);
    VMA_ASSERT(!block->IsFree() && "Cannot insert block twice!");

    uint8_t memClass = SizeToMemoryClass(block->size);
    uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
    uint32_t index = GetListIndex(memClass, secondIndex);
    VMA_ASSERT(index < m_ListsCount);
    block->PrevFree() = VMA_NULL;
    block->NextFree() = m_FreeList[index];
    m_FreeList[index] = block;
    if (block->NextFree() != VMA_NULL)
        block->NextFree()->PrevFree() = block;
    else
    {
        m_InnerIsFreeBitmap[memClass] |= 1U << secondIndex;
        m_IsFreeBitmap |= 1UL << memClass;
    }
    ++m_BlocksFreeCount;
    m_BlocksFreeSize += block->size;
}

void VmaBlockMetadata_TLSF::MergeBlock(Block* block, Block* prev)
{
    VMA_ASSERT(block->prevPhysical == prev && "Cannot merge seperate physical regions!");
    VMA_ASSERT(!prev->IsFree() && "Cannot merge block that belongs to free list!");

    block->offset = prev->offset;
    block->size += prev->size;
    block->prevPhysical = prev->prevPhysical;
    if (block->prevPhysical)
        block->prevPhysical->nextPhysical = block;
    m_BlockAllocator.Free(prev);
}

VmaBlockMetadata_TLSF::Block* VmaBlockMetadata_TLSF::FindFreeBlock(VkDeviceSize size, uint32_t& listIndex) const
{
    uint8_t memoryClass = SizeToMemoryClass(size);
    uint32_t innerFreeMap = m_InnerIsFreeBitmap[memoryClass] & (~0U << SizeToSecondIndex(size, memoryClass));
    if (!innerFreeMap)
    {
        // Check higher levels for avaiable blocks
        uint32_t freeMap = m_IsFreeBitmap & (~0UL << (memoryClass + 1));
        if (!freeMap)
            return VMA_NULL; // No more memory avaible

        // Find lowest free region
        memoryClass = VMA_BITSCAN_LSB(freeMap);
        innerFreeMap = m_InnerIsFreeBitmap[memoryClass];
        VMA_ASSERT(innerFreeMap != 0);
    }
    // Find lowest free subregion
    listIndex = GetListIndex(memoryClass, VMA_BITSCAN_LSB(innerFreeMap));
    VMA_ASSERT(m_FreeList[listIndex]);
    return m_FreeList[listIndex];
}

bool VmaBlockMetadata_TLSF::CheckBlock(
    Block& block,
    uint32_t listIndex,
    VkDeviceSize allocSize,
    VkDeviceSize allocAlignment,
    VmaSuballocationType allocType,
    VmaAllocationRequest* pAllocationRequest)
{
    VMA_ASSERT(block.IsFree() && "Block is already taken!");

    VkDeviceSize alignedOffset = VmaAlignUp(block.offset, allocAlignment);
    if (block.size < allocSize + alignedOffset - block.offset)
        return false;

    // Check for granularity conflicts
    if (!IsVirtual() &&
        m_GranularityHandler.CheckConflictAndAlignUp(alignedOffset, allocSize, block.offset, block.size, allocType))
        return false;

    // Alloc successful
    pAllocationRequest->type = VmaAllocationRequestType::TLSF;
    pAllocationRequest->allocHandle = (VmaAllocHandle)&block;
    pAllocationRequest->size = allocSize - GetDebugMargin();
    pAllocationRequest->customData = (void*)allocType;
    pAllocationRequest->algorithmData = alignedOffset;

    // Place block at the start of list if it's normal block
    if (listIndex != m_ListsCount && block.PrevFree())
    {
        block.PrevFree()->NextFree() = block.NextFree();
        if (block.NextFree())
            block.NextFree()->PrevFree() = block.PrevFree();
        block.PrevFree() = VMA_NULL;
        block.NextFree() = m_FreeList[listIndex];
        m_FreeList[listIndex] = &block;
        if (block.NextFree())
            block.NextFree()->PrevFree() = &block;
    }

    return true;
}
#endif // _VMA_BLOCK_METADATA_TLSF_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_TLSF

#ifndef _VMA_BLOCK_VECTOR
/*
Sequence of VmaDeviceMemoryBlock. Represents memory blocks allocated for a specific
Vulkan memory type.

Synchronized internally with a mutex.
*/
class VmaBlockVector
{
    friend struct VmaDefragmentationContext_T;
    VMA_CLASS_NO_COPY(VmaBlockVector)
public:
    VmaBlockVector(
        VmaAllocator hAllocator,
        VmaPool hParentPool,
        uint32_t memoryTypeIndex,
        VkDeviceSize preferredBlockSize,
        size_t minBlockCount,
        size_t maxBlockCount,
        VkDeviceSize bufferImageGranularity,
        bool explicitBlockSize,
        uint32_t algorithm,
        float priority,
        VkDeviceSize minAllocationAlignment,
        void* pMemoryAllocateNext);
    ~VmaBlockVector();

    VmaAllocator GetAllocator() const { return m_hAllocator; }
    VmaPool GetParentPool() const { return m_hParentPool; }
    bool IsCustomPool() const { return m_hParentPool != VMA_NULL; }
    uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }
    VkDeviceSize GetPreferredBlockSize() const { return m_PreferredBlockSize; }
    VkDeviceSize GetBufferImageGranularity() const { return m_BufferImageGranularity; }
    uint32_t GetAlgorithm() const { return m_Algorithm; }
    bool HasExplicitBlockSize() const { return m_ExplicitBlockSize; }
    float GetPriority() const { return m_Priority; }
    const void* GetAllocationNextPtr() const { return m_pMemoryAllocateNext; }
    // To be used only while the m_Mutex is locked. Used during defragmentation.
    size_t GetBlockCount() const { return m_Blocks.size(); }
    // To be used only while the m_Mutex is locked. Used during defragmentation.
    VmaDeviceMemoryBlock* GetBlock(size_t index) const { return m_Blocks[index]; }
    VMA_RW_MUTEX &GetMutex() { return m_Mutex; }

    VkResult CreateMinBlocks();
    void AddStatistics(VmaStatistics& inoutStats);
    void AddDetailedStatistics(VmaDetailedStatistics& inoutStats);
    bool IsEmpty();
    bool IsCorruptionDetectionEnabled() const;

    VkResult Allocate(
        VkDeviceSize size,
        VkDeviceSize alignment,
        const VmaAllocationCreateInfo& createInfo,
        VmaSuballocationType suballocType,
        size_t allocationCount,
        VmaAllocation* pAllocations);

    void Free(const VmaAllocation hAllocation);

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json);
#endif

    VkResult CheckCorruption();

private:
    const VmaAllocator m_hAllocator;
    const VmaPool m_hParentPool;
    const uint32_t m_MemoryTypeIndex;
    const VkDeviceSize m_PreferredBlockSize;
    const size_t m_MinBlockCount;
    const size_t m_MaxBlockCount;
    const VkDeviceSize m_BufferImageGranularity;
    const bool m_ExplicitBlockSize;
    const uint32_t m_Algorithm;
    const float m_Priority;
    const VkDeviceSize m_MinAllocationAlignment;

    void* const m_pMemoryAllocateNext;
    VMA_RW_MUTEX m_Mutex;
    // Incrementally sorted by sumFreeSize, ascending.
    VmaVector<VmaDeviceMemoryBlock*, VmaStlAllocator<VmaDeviceMemoryBlock*>> m_Blocks;
    uint32_t m_NextBlockId;
    bool m_IncrementalSort = true;

    void SetIncrementalSort(bool val) { m_IncrementalSort = val; }

    VkDeviceSize CalcMaxBlockSize() const;
    // Finds and removes given block from vector.
    void Remove(VmaDeviceMemoryBlock* pBlock);
    // Performs single step in sorting m_Blocks. They may not be fully sorted
    // after this call.
    void IncrementallySortBlocks();
    void SortByFreeSize();

    VkResult AllocatePage(
        VkDeviceSize size,
        VkDeviceSize alignment,
        const VmaAllocationCreateInfo& createInfo,
        VmaSuballocationType suballocType,
        VmaAllocation* pAllocation);

    VkResult AllocateFromBlock(
        VmaDeviceMemoryBlock* pBlock,
        VkDeviceSize size,
        VkDeviceSize alignment,
        VmaAllocationCreateFlags allocFlags,
        void* pUserData,
        VmaSuballocationType suballocType,
        uint32_t strategy,
        VmaAllocation* pAllocation);

    VkResult CommitAllocationRequest(
        VmaAllocationRequest& allocRequest,
        VmaDeviceMemoryBlock* pBlock,
        VkDeviceSize alignment,
        VmaAllocationCreateFlags allocFlags,
        void* pUserData,
        VmaSuballocationType suballocType,
        VmaAllocation* pAllocation);

    VkResult CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex);
    bool HasEmptyBlock();
};
#endif // _VMA_BLOCK_VECTOR

#ifndef _VMA_DEFRAGMENTATION_CONTEXT
struct VmaDefragmentationContext_T
{
    VMA_CLASS_NO_COPY(VmaDefragmentationContext_T)
public:
    VmaDefragmentationContext_T(
        VmaAllocator hAllocator,
        const VmaDefragmentationInfo& info);
    ~VmaDefragmentationContext_T();

    void GetStats(VmaDefragmentationStats& outStats) { outStats = m_GlobalStats; }

    VkResult DefragmentPassBegin(VmaDefragmentationPassMoveInfo& moveInfo);
    VkResult DefragmentPassEnd(VmaDefragmentationPassMoveInfo& moveInfo);

private:
    // Max number of allocations to ignore due to size constraints before ending single pass
    static const uint8_t MAX_ALLOCS_TO_IGNORE = 16;
    enum class CounterStatus { Pass, Ignore, End };

    struct FragmentedBlock
    {
        uint32_t data;
        VmaDeviceMemoryBlock* block;
    };
    struct StateBalanced
    {
        VkDeviceSize avgFreeSize = 0;
        VkDeviceSize avgAllocSize = UINT64_MAX;
    };
    struct StateExtensive
    {
        enum class Operation : uint8_t
        {
            FindFreeBlockBuffer, FindFreeBlockTexture, FindFreeBlockAll,
            MoveBuffers, MoveTextures, MoveAll,
            Cleanup, Done
        };

        Operation operation = Operation::FindFreeBlockTexture;
        size_t firstFreeBlock = SIZE_MAX;
    };
    struct MoveAllocationData
    {
        VkDeviceSize size;
        VkDeviceSize alignment;
        VmaSuballocationType type;
        VmaAllocationCreateFlags flags;
        VmaDefragmentationMove move = {};
    };

    const VkDeviceSize m_MaxPassBytes;
    const uint32_t m_MaxPassAllocations;

    VmaStlAllocator<VmaDefragmentationMove> m_MoveAllocator;
    VmaVector<VmaDefragmentationMove, VmaStlAllocator<VmaDefragmentationMove>> m_Moves;

    uint8_t m_IgnoredAllocs = 0;
    uint32_t m_Algorithm;
    uint32_t m_BlockVectorCount;
    VmaBlockVector* m_PoolBlockVector;
    VmaBlockVector** m_pBlockVectors;
    size_t m_ImmovableBlockCount = 0;
    VmaDefragmentationStats m_GlobalStats = { 0 };
    VmaDefragmentationStats m_PassStats = { 0 };
    void* m_AlgorithmState = VMA_NULL;

    static MoveAllocationData GetMoveData(VmaAllocHandle handle, VmaBlockMetadata* metadata);
    CounterStatus CheckCounters(VkDeviceSize bytes);
    bool IncrementCounters(VkDeviceSize bytes);
    bool ReallocWithinBlock(VmaBlockVector& vector, VmaDeviceMemoryBlock* block);
    bool AllocInOtherBlock(size_t start, size_t end, MoveAllocationData& data, VmaBlockVector& vector);

    bool ComputeDefragmentation(VmaBlockVector& vector, size_t index);
    bool ComputeDefragmentation_Fast(VmaBlockVector& vector);
    bool ComputeDefragmentation_Balanced(VmaBlockVector& vector, size_t index, bool update);
    bool ComputeDefragmentation_Full(VmaBlockVector& vector);
    bool ComputeDefragmentation_Extensive(VmaBlockVector& vector, size_t index);

    void UpdateVectorStatistics(VmaBlockVector& vector, StateBalanced& state);
    bool MoveDataToFreeBlocks(VmaSuballocationType currentType,
        VmaBlockVector& vector, size_t firstFreeBlock,
        bool& texturePresent, bool& bufferPresent, bool& otherPresent);
};
#endif // _VMA_DEFRAGMENTATION_CONTEXT

#ifndef _VMA_POOL_T
struct VmaPool_T
{
    friend struct VmaPoolListItemTraits;
    VMA_CLASS_NO_COPY(VmaPool_T)
public:
    VmaBlockVector m_BlockVector;
    VmaDedicatedAllocationList m_DedicatedAllocations;

    VmaPool_T(
        VmaAllocator hAllocator,
        const VmaPoolCreateInfo& createInfo,
        VkDeviceSize preferredBlockSize);
    ~VmaPool_T();

    uint32_t GetId() const { return m_Id; }
    void SetId(uint32_t id) { VMA_ASSERT(m_Id == 0); m_Id = id; }

    const char* GetName() const { return m_Name; }
    void SetName(const char* pName);

#if VMA_STATS_STRING_ENABLED
    //void PrintDetailedMap(class VmaStringBuilder& sb);
#endif

private:
    uint32_t m_Id;
    char* m_Name;
    VmaPool_T* m_PrevPool = VMA_NULL;
    VmaPool_T* m_NextPool = VMA_NULL;
};

struct VmaPoolListItemTraits
{
    typedef VmaPool_T ItemType;

    static ItemType* GetPrev(const ItemType* item) { return item->m_PrevPool; }
    static ItemType* GetNext(const ItemType* item) { return item->m_NextPool; }
    static ItemType*& AccessPrev(ItemType* item) { return item->m_PrevPool; }
    static ItemType*& AccessNext(ItemType* item) { return item->m_NextPool; }
};
#endif // _VMA_POOL_T

#ifndef _VMA_CURRENT_BUDGET_DATA
struct VmaCurrentBudgetData
{
    VMA_ATOMIC_UINT32 m_BlockCount[VK_MAX_MEMORY_HEAPS];
    VMA_ATOMIC_UINT32 m_AllocationCount[VK_MAX_MEMORY_HEAPS];
    VMA_ATOMIC_UINT64 m_BlockBytes[VK_MAX_MEMORY_HEAPS];
    VMA_ATOMIC_UINT64 m_AllocationBytes[VK_MAX_MEMORY_HEAPS];

#if VMA_MEMORY_BUDGET
    VMA_ATOMIC_UINT32 m_OperationsSinceBudgetFetch;
    VMA_RW_MUTEX m_BudgetMutex;
    uint64_t m_VulkanUsage[VK_MAX_MEMORY_HEAPS];
    uint64_t m_VulkanBudget[VK_MAX_MEMORY_HEAPS];
    uint64_t m_BlockBytesAtBudgetFetch[VK_MAX_MEMORY_HEAPS];
#endif // VMA_MEMORY_BUDGET

    VmaCurrentBudgetData();

    void AddAllocation(uint32_t heapIndex, VkDeviceSize allocationSize);
    void RemoveAllocation(uint32_t heapIndex, VkDeviceSize allocationSize);
};

#ifndef _VMA_CURRENT_BUDGET_DATA_FUNCTIONS
VmaCurrentBudgetData::VmaCurrentBudgetData()
{
    for (uint32_t heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; ++heapIndex)
    {
        m_BlockCount[heapIndex] = 0;
        m_AllocationCount[heapIndex] = 0;
        m_BlockBytes[heapIndex] = 0;
        m_AllocationBytes[heapIndex] = 0;
#if VMA_MEMORY_BUDGET
        m_VulkanUsage[heapIndex] = 0;
        m_VulkanBudget[heapIndex] = 0;
        m_BlockBytesAtBudgetFetch[heapIndex] = 0;
#endif
    }

#if VMA_MEMORY_BUDGET
    m_OperationsSinceBudgetFetch = 0;
#endif
}

void VmaCurrentBudgetData::AddAllocation(uint32_t heapIndex, VkDeviceSize allocationSize)
{
    m_AllocationBytes[heapIndex] += allocationSize;
    ++m_AllocationCount[heapIndex];
#if VMA_MEMORY_BUDGET
    ++m_OperationsSinceBudgetFetch;
#endif
}

void VmaCurrentBudgetData::RemoveAllocation(uint32_t heapIndex, VkDeviceSize allocationSize)
{
    VMA_ASSERT(m_AllocationBytes[heapIndex] >= allocationSize);
    m_AllocationBytes[heapIndex] -= allocationSize;
    VMA_ASSERT(m_AllocationCount[heapIndex] > 0);
    --m_AllocationCount[heapIndex];
#if VMA_MEMORY_BUDGET
    ++m_OperationsSinceBudgetFetch;
#endif
}
#endif // _VMA_CURRENT_BUDGET_DATA_FUNCTIONS
#endif // _VMA_CURRENT_BUDGET_DATA

#ifndef _VMA_ALLOCATION_OBJECT_ALLOCATOR
/*
Thread-safe wrapper over VmaPoolAllocator free list, for allocation of VmaAllocation_T objects.
*/
class VmaAllocationObjectAllocator
{
    VMA_CLASS_NO_COPY(VmaAllocationObjectAllocator)
public:
    VmaAllocationObjectAllocator(const VkAllocationCallbacks* pAllocationCallbacks)
        : m_Allocator(pAllocationCallbacks, 1024) {}

    template<typename... Types> VmaAllocation Allocate(Types&&... args);
    void Free(VmaAllocation hAlloc);

private:
    VMA_MUTEX m_Mutex;
    VmaPoolAllocator<VmaAllocation_T> m_Allocator;
};

template<typename... Types>
VmaAllocation VmaAllocationObjectAllocator::Allocate(Types&&... args)
{
    VmaMutexLock mutexLock(m_Mutex);
    return m_Allocator.Alloc<Types...>(std::forward<Types>(args)...);
}

void VmaAllocationObjectAllocator::Free(VmaAllocation hAlloc)
{
    VmaMutexLock mutexLock(m_Mutex);
    m_Allocator.Free(hAlloc);
}
#endif // _VMA_ALLOCATION_OBJECT_ALLOCATOR

#ifndef _VMA_VIRTUAL_BLOCK_T
struct VmaVirtualBlock_T
{
    VMA_CLASS_NO_COPY(VmaVirtualBlock_T)
public:
    const bool m_AllocationCallbacksSpecified;
    const VkAllocationCallbacks m_AllocationCallbacks;

    VmaVirtualBlock_T(const VmaVirtualBlockCreateInfo& createInfo);
    ~VmaVirtualBlock_T();

    VkResult Init() { return VK_SUCCESS; }
    bool IsEmpty() const { return m_Metadata->IsEmpty(); }
    void Free(VmaVirtualAllocation allocation) { m_Metadata->Free((VmaAllocHandle)allocation); }
    void SetAllocationUserData(VmaVirtualAllocation allocation, void* userData) { m_Metadata->SetAllocationUserData((VmaAllocHandle)allocation, userData); }
    void Clear() { m_Metadata->Clear(); }

    const VkAllocationCallbacks* GetAllocationCallbacks() const;
    void GetAllocationInfo(VmaVirtualAllocation allocation, VmaVirtualAllocationInfo& outInfo);
    VkResult Allocate(const VmaVirtualAllocationCreateInfo& createInfo, VmaVirtualAllocation& outAllocation,
        VkDeviceSize* outOffset);
    void GetStatistics(VmaStatistics& outStats) const;
    void CalculateDetailedStatistics(VmaDetailedStatistics& outStats) const;
#if VMA_STATS_STRING_ENABLED
    void BuildStatsString(bool detailedMap, VmaStringBuilder& sb) const;
#endif

private:
    VmaBlockMetadata* m_Metadata;
};

#ifndef _VMA_VIRTUAL_BLOCK_T_FUNCTIONS
VmaVirtualBlock_T::VmaVirtualBlock_T(const VmaVirtualBlockCreateInfo& createInfo)
    : m_AllocationCallbacksSpecified(createInfo.pAllocationCallbacks != VMA_NULL),
    m_AllocationCallbacks(createInfo.pAllocationCallbacks != VMA_NULL ? *createInfo.pAllocationCallbacks : VmaEmptyAllocationCallbacks)
{
    const uint32_t algorithm = createInfo.flags & VMA_VIRTUAL_BLOCK_CREATE_ALGORITHM_MASK;
    switch (algorithm)
    {
    default:
        VMA_ASSERT(0);
    case 0:
        m_Metadata = vma_new(GetAllocationCallbacks(), VmaBlockMetadata_TLSF)(VK_NULL_HANDLE, 1, true);
        break;
    case VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT:
        m_Metadata = vma_new(GetAllocationCallbacks(), VmaBlockMetadata_Linear)(VK_NULL_HANDLE, 1, true);
        break;
    }

    m_Metadata->Init(createInfo.size);
}

VmaVirtualBlock_T::~VmaVirtualBlock_T()
{
    // Define macro VMA_DEBUG_LOG to receive the list of the unfreed allocations
    if (!m_Metadata->IsEmpty())
        m_Metadata->DebugLogAllAllocations();
    // This is the most important assert in the entire library.
    // Hitting it means you have some memory leak - unreleased virtual allocations.
    VMA_ASSERT(m_Metadata->IsEmpty() && "Some virtual allocations were not freed before destruction of this virtual block!");

    vma_delete(GetAllocationCallbacks(), m_Metadata);
}

const VkAllocationCallbacks* VmaVirtualBlock_T::GetAllocationCallbacks() const
{
    return m_AllocationCallbacksSpecified ? &m_AllocationCallbacks : VMA_NULL;
}

void VmaVirtualBlock_T::GetAllocationInfo(VmaVirtualAllocation allocation, VmaVirtualAllocationInfo& outInfo)
{
    m_Metadata->GetAllocationInfo((VmaAllocHandle)allocation, outInfo);
}

VkResult VmaVirtualBlock_T::Allocate(const VmaVirtualAllocationCreateInfo& createInfo, VmaVirtualAllocation& outAllocation,
    VkDeviceSize* outOffset)
{
    VmaAllocationRequest request = {};
    if (m_Metadata->CreateAllocationRequest(
        createInfo.size, // allocSize
        VMA_MAX(createInfo.alignment, (VkDeviceSize)1), // allocAlignment
        (createInfo.flags & VMA_VIRTUAL_ALLOCATION_CREATE_UPPER_ADDRESS_BIT) != 0, // upperAddress
        VMA_SUBALLOCATION_TYPE_UNKNOWN, // allocType - unimportant
        createInfo.flags & VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MASK, // strategy
        &request))
    {
        m_Metadata->Alloc(request,
            VMA_SUBALLOCATION_TYPE_UNKNOWN, // type - unimportant
            createInfo.pUserData);
        outAllocation = (VmaVirtualAllocation)request.allocHandle;
        if(outOffset)
            *outOffset = m_Metadata->GetAllocationOffset(request.allocHandle);
        return VK_SUCCESS;
    }
    outAllocation = (VmaVirtualAllocation)VK_NULL_HANDLE;
    if (outOffset)
        *outOffset = UINT64_MAX;
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VmaVirtualBlock_T::GetStatistics(VmaStatistics& outStats) const
{
    VmaClearStatistics(outStats);
    m_Metadata->AddStatistics(outStats);
}

void VmaVirtualBlock_T::CalculateDetailedStatistics(VmaDetailedStatistics& outStats) const
{
    VmaClearDetailedStatistics(outStats);
    m_Metadata->AddDetailedStatistics(outStats);
}

#if VMA_STATS_STRING_ENABLED
void VmaVirtualBlock_T::BuildStatsString(bool detailedMap, VmaStringBuilder& sb) const
{
    VmaJsonWriter json(GetAllocationCallbacks(), sb);
    json.BeginObject();

    VmaDetailedStatistics stats;
    CalculateDetailedStatistics(stats);

    json.WriteString("Stats");
    VmaPrintDetailedStatistics(json, stats);

    if (detailedMap)
    {
        json.WriteString("Details");
        json.BeginObject();
        m_Metadata->PrintDetailedMap(json);
        json.EndObject();
    }

    json.EndObject();
}
#endif // VMA_STATS_STRING_ENABLED
#endif // _VMA_VIRTUAL_BLOCK_T_FUNCTIONS
#endif // _VMA_VIRTUAL_BLOCK_T


// Main allocator object.
struct VmaAllocator_T
{
    VMA_CLASS_NO_COPY(VmaAllocator_T)
public:
    bool m_UseMutex;
    uint32_t m_VulkanApiVersion;
    bool m_UseKhrDedicatedAllocation; // Can be set only if m_VulkanApiVersion < VK_MAKE_VERSION(1, 1, 0).
    bool m_UseKhrBindMemory2; // Can be set only if m_VulkanApiVersion < VK_MAKE_VERSION(1, 1, 0).
    bool m_UseExtMemoryBudget;
    bool m_UseAmdDeviceCoherentMemory;
    bool m_UseKhrBufferDeviceAddress;
    bool m_UseExtMemoryPriority;
    VkDevice m_hDevice;
    VkInstance m_hInstance;
    bool m_AllocationCallbacksSpecified;
    VkAllocationCallbacks m_AllocationCallbacks;
    VmaDeviceMemoryCallbacks m_DeviceMemoryCallbacks;
    VmaAllocationObjectAllocator m_AllocationObjectAllocator;

    // Each bit (1 << i) is set if HeapSizeLimit is enabled for that heap, so cannot allocate more than the heap size.
    uint32_t m_HeapSizeLimitMask;

    VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties m_MemProps;

    // Default pools.
    VmaBlockVector* m_pBlockVectors[VK_MAX_MEMORY_TYPES];
    VmaDedicatedAllocationList m_DedicatedAllocations[VK_MAX_MEMORY_TYPES];

    VmaCurrentBudgetData m_Budget;
    VMA_ATOMIC_UINT32 m_DeviceMemoryCount; // Total number of VkDeviceMemory objects.

    VmaAllocator_T(const VmaAllocatorCreateInfo* pCreateInfo);
    VkResult Init(const VmaAllocatorCreateInfo* pCreateInfo);
    ~VmaAllocator_T();

    const VkAllocationCallbacks* GetAllocationCallbacks() const
    {
        return m_AllocationCallbacksSpecified ? &m_AllocationCallbacks : VMA_NULL;
    }
    const VmaVulkanFunctions& GetVulkanFunctions() const
    {
        return m_VulkanFunctions;
    }

    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

    VkDeviceSize GetBufferImageGranularity() const
    {
        return VMA_MAX(
            static_cast<VkDeviceSize>(VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY),
            m_PhysicalDeviceProperties.limits.bufferImageGranularity);
    }

    uint32_t GetMemoryHeapCount() const { return m_MemProps.memoryHeapCount; }
    uint32_t GetMemoryTypeCount() const { return m_MemProps.memoryTypeCount; }

    uint32_t MemoryTypeIndexToHeapIndex(uint32_t memTypeIndex) const
    {
        VMA_ASSERT(memTypeIndex < m_MemProps.memoryTypeCount);
        return m_MemProps.memoryTypes[memTypeIndex].heapIndex;
    }
    // True when specific memory type is HOST_VISIBLE but not HOST_COHERENT.
    bool IsMemoryTypeNonCoherent(uint32_t memTypeIndex) const
    {
        return (m_MemProps.memoryTypes[memTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    // Minimum alignment for all allocations in specific memory type.
    VkDeviceSize GetMemoryTypeMinAlignment(uint32_t memTypeIndex) const
    {
        return IsMemoryTypeNonCoherent(memTypeIndex) ?
            VMA_MAX((VkDeviceSize)VMA_MIN_ALIGNMENT, m_PhysicalDeviceProperties.limits.nonCoherentAtomSize) :
            (VkDeviceSize)VMA_MIN_ALIGNMENT;
    }

    bool IsIntegratedGpu() const
    {
        return m_PhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    }

    uint32_t GetGlobalMemoryTypeBits() const { return m_GlobalMemoryTypeBits; }

    void GetBufferMemoryRequirements(
        VkBuffer hBuffer,
        VkMemoryRequirements& memReq,
        bool& requiresDedicatedAllocation,
        bool& prefersDedicatedAllocation) const;
    void GetImageMemoryRequirements(
        VkImage hImage,
        VkMemoryRequirements& memReq,
        bool& requiresDedicatedAllocation,
        bool& prefersDedicatedAllocation) const;
    VkResult FindMemoryTypeIndex(
        uint32_t memoryTypeBits,
        const VmaAllocationCreateInfo* pAllocationCreateInfo,
        VkFlags bufImgUsage, // VkBufferCreateInfo::usage or VkImageCreateInfo::usage. UINT32_MAX if unknown.
        uint32_t* pMemoryTypeIndex) const;

    // Main allocation function.
    VkResult AllocateMemory(
        const VkMemoryRequirements& vkMemReq,
        bool requiresDedicatedAllocation,
        bool prefersDedicatedAllocation,
        VkBuffer dedicatedBuffer,
        VkImage dedicatedImage,
        VkFlags dedicatedBufferImageUsage, // UINT32_MAX if unknown.
        const VmaAllocationCreateInfo& createInfo,
        VmaSuballocationType suballocType,
        size_t allocationCount,
        VmaAllocation* pAllocations);

    // Main deallocation function.
    void FreeMemory(
        size_t allocationCount,
        const VmaAllocation* pAllocations);

    void CalculateStatistics(VmaTotalStatistics* pStats);

    void GetHeapBudgets(
        VmaBudget* outBudgets, uint32_t firstHeap, uint32_t heapCount);

#if VMA_STATS_STRING_ENABLED
    void PrintDetailedMap(class VmaJsonWriter& json);
#endif

    void GetAllocationInfo(VmaAllocation hAllocation, VmaAllocationInfo* pAllocationInfo);

    VkResult CreatePool(const VmaPoolCreateInfo* pCreateInfo, VmaPool* pPool);
    void DestroyPool(VmaPool pool);
    void GetPoolStatistics(VmaPool pool, VmaStatistics* pPoolStats);
    void CalculatePoolStatistics(VmaPool pool, VmaDetailedStatistics* pPoolStats);

    void SetCurrentFrameIndex(uint32_t frameIndex);
    uint32_t GetCurrentFrameIndex() const { return m_CurrentFrameIndex.load(); }

    VkResult CheckPoolCorruption(VmaPool hPool);
    VkResult CheckCorruption(uint32_t memoryTypeBits);

    // Call to Vulkan function vkAllocateMemory with accompanying bookkeeping.
    VkResult AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory);
    // Call to Vulkan function vkFreeMemory with accompanying bookkeeping.
    void FreeVulkanMemory(uint32_t memoryType, VkDeviceSize size, VkDeviceMemory hMemory);
    // Call to Vulkan function vkBindBufferMemory or vkBindBufferMemory2KHR.
    VkResult BindVulkanBuffer(
        VkDeviceMemory memory,
        VkDeviceSize memoryOffset,
        VkBuffer buffer,
        const void* pNext);
    // Call to Vulkan function vkBindImageMemory or vkBindImageMemory2KHR.
    VkResult BindVulkanImage(
        VkDeviceMemory memory,
        VkDeviceSize memoryOffset,
        VkImage image,
        const void* pNext);

    VkResult Map(VmaAllocation hAllocation, void** ppData);
    void Unmap(VmaAllocation hAllocation);

    VkResult BindBufferMemory(
        VmaAllocation hAllocation,
        VkDeviceSize allocationLocalOffset,
        VkBuffer hBuffer,
        const void* pNext);
    VkResult BindImageMemory(
        VmaAllocation hAllocation,
        VkDeviceSize allocationLocalOffset,
        VkImage hImage,
        const void* pNext);

    VkResult FlushOrInvalidateAllocation(
        VmaAllocation hAllocation,
        VkDeviceSize offset, VkDeviceSize size,
        VMA_CACHE_OPERATION op);
    VkResult FlushOrInvalidateAllocations(
        uint32_t allocationCount,
        const VmaAllocation* allocations,
        const VkDeviceSize* offsets, const VkDeviceSize* sizes,
        VMA_CACHE_OPERATION op);

    void FillAllocation(const VmaAllocation hAllocation, uint8_t pattern);

    /*
    Returns bit mask of memory types that can support defragmentation on GPU as
    they support creation of required buffer for copy operations.
    */
    uint32_t GetGpuDefragmentationMemoryTypeBits();

#if VMA_EXTERNAL_MEMORY
    VkExternalMemoryHandleTypeFlagsKHR GetExternalMemoryHandleTypeFlags(uint32_t memTypeIndex) const
    {
        return m_TypeExternalMemoryHandleTypes[memTypeIndex];
    }
#endif // #if VMA_EXTERNAL_MEMORY

private:
    VkDeviceSize m_PreferredLargeHeapBlockSize;

    VkPhysicalDevice m_PhysicalDevice;
    VMA_ATOMIC_UINT32 m_CurrentFrameIndex;
    VMA_ATOMIC_UINT32 m_GpuDefragmentationMemoryTypeBits; // UINT32_MAX means uninitialized.
#if VMA_EXTERNAL_MEMORY
    VkExternalMemoryHandleTypeFlagsKHR m_TypeExternalMemoryHandleTypes[VK_MAX_MEMORY_TYPES];
#endif // #if VMA_EXTERNAL_MEMORY

    VMA_RW_MUTEX m_PoolsMutex;
    typedef VmaIntrusiveLinkedList<VmaPoolListItemTraits> PoolList;
    // Protected by m_PoolsMutex.
    PoolList m_Pools;
    uint32_t m_NextPoolId;

    VmaVulkanFunctions m_VulkanFunctions;

    // Global bit mask AND-ed with any memoryTypeBits to disallow certain memory types.
    uint32_t m_GlobalMemoryTypeBits;

    void ImportVulkanFunctions(const VmaVulkanFunctions* pVulkanFunctions);

#if VMA_STATIC_VULKAN_FUNCTIONS == 1
    void ImportVulkanFunctions_Static();
#endif

    void ImportVulkanFunctions_Custom(const VmaVulkanFunctions* pVulkanFunctions);

#if VMA_DYNAMIC_VULKAN_FUNCTIONS == 1
    void ImportVulkanFunctions_Dynamic();
#endif

    void ValidateVulkanFunctions();

    VkDeviceSize CalcPreferredBlockSize(uint32_t memTypeIndex);

    VkResult AllocateMemoryOfType(
        VmaPool pool,
        VkDeviceSize size,
        VkDeviceSize alignment,
        bool dedicatedPreferred,
        VkBuffer dedicatedBuffer,
        VkImage dedicatedImage,
        VkFlags dedicatedBufferImageUsage,
        const VmaAllocationCreateInfo& createInfo,
        uint32_t memTypeIndex,
        VmaSuballocationType suballocType,
        VmaDedicatedAllocationList& dedicatedAllocations,
        VmaBlockVector& blockVector,
        size_t allocationCount,
        VmaAllocation* pAllocations);

    // Helper function only to be used inside AllocateDedicatedMemory.
    VkResult AllocateDedicatedMemoryPage(
        VmaPool pool,
        VkDeviceSize size,
        VmaSuballocationType suballocType,
        uint32_t memTypeIndex,
        const VkMemoryAllocateInfo& allocInfo,
        bool map,
        bool isUserDataString,
        bool isMappingAllowed,
        void* pUserData,
        VmaAllocation* pAllocation);

    // Allocates and registers new VkDeviceMemory specifically for dedicated allocations.
    VkResult AllocateDedicatedMemory(
        VmaPool pool,
        VkDeviceSize size,
        VmaSuballocationType suballocType,
        VmaDedicatedAllocationList& dedicatedAllocations,
        uint32_t memTypeIndex,
        bool map,
        bool isUserDataString,
        bool isMappingAllowed,
        bool canAliasMemory,
        void* pUserData,
        float priority,
        VkBuffer dedicatedBuffer,
        VkImage dedicatedImage,
        VkFlags dedicatedBufferImageUsage,
        size_t allocationCount,
        VmaAllocation* pAllocations,
        const void* pNextChain = nullptr);

    void FreeDedicatedMemory(const VmaAllocation allocation);

    VkResult CalcMemTypeParams(
        VmaAllocationCreateInfo& outCreateInfo,
        uint32_t memTypeIndex,
        VkDeviceSize size,
        size_t allocationCount);
    VkResult CalcAllocationParams(
        VmaAllocationCreateInfo& outCreateInfo,
        bool dedicatedRequired,
        bool dedicatedPreferred);

    /*
    Calculates and returns bit mask of memory types that can support defragmentation
    on GPU as they support creation of required buffer for copy operations.
    */
    uint32_t CalculateGpuDefragmentationMemoryTypeBits() const;
    uint32_t CalculateGlobalMemoryTypeBits() const;

    bool GetFlushOrInvalidateRange(
        VmaAllocation allocation,
        VkDeviceSize offset, VkDeviceSize size,
        VkMappedMemoryRange& outRange) const;

#if VMA_MEMORY_BUDGET
    void UpdateVulkanBudget();
#endif // #if VMA_MEMORY_BUDGET
};


#ifndef _VMA_MEMORY_FUNCTIONS
static void* VmaMalloc(VmaAllocator hAllocator, size_t size, size_t alignment)
{
    return VmaMalloc(&hAllocator->m_AllocationCallbacks, size, alignment);
}

static void VmaFree(VmaAllocator hAllocator, void* ptr)
{
    VmaFree(&hAllocator->m_AllocationCallbacks, ptr);
}

template<typename T>
static T* VmaAllocate(VmaAllocator hAllocator)
{
    return (T*)VmaMalloc(hAllocator, sizeof(T), VMA_ALIGN_OF(T));
}

template<typename T>
static T* VmaAllocateArray(VmaAllocator hAllocator, size_t count)
{
    return (T*)VmaMalloc(hAllocator, sizeof(T) * count, VMA_ALIGN_OF(T));
}

template<typename T>
static void vma_delete(VmaAllocator hAllocator, T* ptr)
{
    if(ptr != VMA_NULL)
    {
        ptr->~T();
        VmaFree(hAllocator, ptr);
    }
}

template<typename T>
static void vma_delete_array(VmaAllocator hAllocator, T* ptr, size_t count)
{
    if(ptr != VMA_NULL)
    {
        for(size_t i = count; i--; )
            ptr[i].~T();
        VmaFree(hAllocator, ptr);
    }
}
#endif // _VMA_MEMORY_FUNCTIONS

#ifndef _VMA_DEVICE_MEMORY_BLOCK_FUNCTIONS
VmaDeviceMemoryBlock::VmaDeviceMemoryBlock(VmaAllocator hAllocator)
    : m_pMetadata(VMA_NULL),
    m_MemoryTypeIndex(UINT32_MAX),
    m_Id(0),
    m_hMemory(VK_NULL_HANDLE),
    m_MapCount(0),
    m_pMappedData(VMA_NULL) {}

VmaDeviceMemoryBlock::~VmaDeviceMemoryBlock()
{
    VMA_ASSERT(m_MapCount == 0 && "VkDeviceMemory block is being destroyed while it is still mapped.");
    VMA_ASSERT(m_hMemory == VK_NULL_HANDLE);
}

void VmaDeviceMemoryBlock::Init(
    VmaAllocator hAllocator,
    VmaPool hParentPool,
    uint32_t newMemoryTypeIndex,
    VkDeviceMemory newMemory,
    VkDeviceSize newSize,
    uint32_t id,
    uint32_t algorithm,
    VkDeviceSize bufferImageGranularity)
{
    VMA_ASSERT(m_hMemory == VK_NULL_HANDLE);

    m_hParentPool = hParentPool;
    m_MemoryTypeIndex = newMemoryTypeIndex;
    m_Id = id;
    m_hMemory = newMemory;

    switch (algorithm)
    {
    case VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT:
        m_pMetadata = vma_new(hAllocator, VmaBlockMetadata_Linear)(hAllocator->GetAllocationCallbacks(),
            bufferImageGranularity, false); // isVirtual
        break;
    default:
        VMA_ASSERT(0);
        // Fall-through.
    case 0:
        m_pMetadata = vma_new(hAllocator, VmaBlockMetadata_TLSF)(hAllocator->GetAllocationCallbacks(),
            bufferImageGranularity, false); // isVirtual
    }
    m_pMetadata->Init(newSize);
}

void VmaDeviceMemoryBlock::Destroy(VmaAllocator allocator)
{
    // Define macro VMA_DEBUG_LOG to receive the list of the unfreed allocations
    if (!m_pMetadata->IsEmpty())
        m_pMetadata->DebugLogAllAllocations();
    // This is the most important assert in the entire library.
    // Hitting it means you have some memory leak - unreleased VmaAllocation objects.
    VMA_ASSERT(m_pMetadata->IsEmpty() && "Some allocations were not freed before destruction of this memory block!");

    VMA_ASSERT(m_hMemory != VK_NULL_HANDLE);
    allocator->FreeVulkanMemory(m_MemoryTypeIndex, m_pMetadata->GetSize(), m_hMemory);
    m_hMemory = VK_NULL_HANDLE;

    vma_delete(allocator, m_pMetadata);
    m_pMetadata = VMA_NULL;
}

void VmaDeviceMemoryBlock::PostFree(VmaAllocator hAllocator)
{
    if(m_MappingHysteresis.PostFree())
    {
        VMA_ASSERT(m_MappingHysteresis.GetExtraMapping() == 0);
        if (m_MapCount == 0)
        {
            m_pMappedData = VMA_NULL;
            (*hAllocator->GetVulkanFunctions().vkUnmapMemory)(hAllocator->m_hDevice, m_hMemory);
        }
    }
}

bool VmaDeviceMemoryBlock::Validate() const
{
    VMA_VALIDATE((m_hMemory != VK_NULL_HANDLE) &&
        (m_pMetadata->GetSize() != 0));

    return m_pMetadata->Validate();
}

VkResult VmaDeviceMemoryBlock::CheckCorruption(VmaAllocator hAllocator)
{
    void* pData = nullptr;
    VkResult res = Map(hAllocator, 1, &pData);
    if (res != VK_SUCCESS)
    {
        return res;
    }

    res = m_pMetadata->CheckCorruption(pData);

    Unmap(hAllocator, 1);

    return res;
}

VkResult VmaDeviceMemoryBlock::Map(VmaAllocator hAllocator, uint32_t count, void** ppData)
{
    if (count == 0)
    {
        return VK_SUCCESS;
    }

    VmaMutexLock lock(m_MapAndBindMutex, hAllocator->m_UseMutex);
    const uint32_t oldTotalMapCount = m_MapCount + m_MappingHysteresis.GetExtraMapping();
    m_MappingHysteresis.PostMap();
    if (oldTotalMapCount != 0)
    {
        m_MapCount += count;
        VMA_ASSERT(m_pMappedData != VMA_NULL);
        if (ppData != VMA_NULL)
        {
            *ppData = m_pMappedData;
        }
        return VK_SUCCESS;
    }
    else
    {
        VkResult result = (*hAllocator->GetVulkanFunctions().vkMapMemory)(
            hAllocator->m_hDevice,
            m_hMemory,
            0, // offset
            VK_WHOLE_SIZE,
            0, // flags
            &m_pMappedData);
        if (result == VK_SUCCESS)
        {
            if (ppData != VMA_NULL)
            {
                *ppData = m_pMappedData;
            }
            m_MapCount = count;
        }
        return result;
    }
}

void VmaDeviceMemoryBlock::Unmap(VmaAllocator hAllocator, uint32_t count)
{
    if (count == 0)
    {
        return;
    }

    VmaMutexLock lock(m_MapAndBindMutex, hAllocator->m_UseMutex);
    if (m_MapCount >= count)
    {
        m_MapCount -= count;
        const uint32_t totalMapCount = m_MapCount + m_MappingHysteresis.GetExtraMapping();
        if (totalMapCount == 0)
        {
            m_pMappedData = VMA_NULL;
            (*hAllocator->GetVulkanFunctions().vkUnmapMemory)(hAllocator->m_hDevice, m_hMemory);
        }
        m_MappingHysteresis.PostUnmap();
    }
    else
    {
        VMA_ASSERT(0 && "VkDeviceMemory block is being unmapped while it was not previously mapped.");
    }
}

VkResult VmaDeviceMemoryBlock::WriteMagicValueAfterAllocation(VmaAllocator hAllocator, VkDeviceSize allocOffset, VkDeviceSize allocSize)
{
    VMA_ASSERT(VMA_DEBUG_MARGIN > 0 && VMA_DEBUG_MARGIN % 4 == 0 && VMA_DEBUG_DETECT_CORRUPTION);

    void* pData;
    VkResult res = Map(hAllocator, 1, &pData);
    if (res != VK_SUCCESS)
    {
        return res;
    }

    VmaWriteMagicValue(pData, allocOffset + allocSize);

    Unmap(hAllocator, 1);
    return VK_SUCCESS;
}

VkResult VmaDeviceMemoryBlock::ValidateMagicValueAfterAllocation(VmaAllocator hAllocator, VkDeviceSize allocOffset, VkDeviceSize allocSize)
{
    VMA_ASSERT(VMA_DEBUG_MARGIN > 0 && VMA_DEBUG_MARGIN % 4 == 0 && VMA_DEBUG_DETECT_CORRUPTION);

    void* pData;
    VkResult res = Map(hAllocator, 1, &pData);
    if (res != VK_SUCCESS)
    {
        return res;
    }

    if (!VmaValidateMagicValue(pData, allocOffset + allocSize))
    {
        VMA_ASSERT(0 && "MEMORY CORRUPTION DETECTED AFTER FREED ALLOCATION!");
    }

    Unmap(hAllocator, 1);
    return VK_SUCCESS;
}

VkResult VmaDeviceMemoryBlock::BindBufferMemory(
    const VmaAllocator hAllocator,
    const VmaAllocation hAllocation,
    VkDeviceSize allocationLocalOffset,
    VkBuffer hBuffer,
    const void* pNext)
{
    VMA_ASSERT(hAllocation->GetType() == VmaAllocation_T::ALLOCATION_TYPE_BLOCK &&
        hAllocation->GetBlock() == this);
    VMA_ASSERT(allocationLocalOffset < hAllocation->GetSize() &&
        "Invalid allocationLocalOffset. Did you forget that this offset is relative to the beginning of the allocation, not the whole memory block?");
    const VkDeviceSize memoryOffset = hAllocation->GetOffset() + allocationLocalOffset;
    // This lock is important so that we don't call vkBind... and/or vkMap... simultaneously on the same VkDeviceMemory from multiple threads.
    VmaMutexLock lock(m_MapAndBindMutex, hAllocator->m_UseMutex);
    return hAllocator->BindVulkanBuffer(m_hMemory, memoryOffset, hBuffer, pNext);
}

VkResult VmaDeviceMemoryBlock::BindImageMemory(
    const VmaAllocator hAllocator,
    const VmaAllocation hAllocation,
    VkDeviceSize allocationLocalOffset,
    VkImage hImage,
    const void* pNext)
{
    VMA_ASSERT(hAllocation->GetType() == VmaAllocation_T::ALLOCATION_TYPE_BLOCK &&
        hAllocation->GetBlock() == this);
    VMA_ASSERT(allocationLocalOffset < hAllocation->GetSize() &&
        "Invalid allocationLocalOffset. Did you forget that this offset is relative to the beginning of the allocation, not the whole memory block?");
    const VkDeviceSize memoryOffset = hAllocation->GetOffset() + allocationLocalOffset;
    // This lock is important so that we don't call vkBind... and/or vkMap... simultaneously on the same VkDeviceMemory from multiple threads.
    VmaMutexLock lock(m_MapAndBindMutex, hAllocator->m_UseMutex);
    return hAllocator->BindVulkanImage(m_hMemory, memoryOffset, hImage, pNext);
}
#endif // _VMA_DEVICE_MEMORY_BLOCK_FUNCTIONS

#ifndef _VMA_ALLOCATION_T_FUNCTIONS
VmaAllocation_T::VmaAllocation_T(bool mappingAllowed)
    : m_Alignment{ 1 },
    m_Size{ 0 },
    m_pUserData{ VMA_NULL },
    m_pName{ VMA_NULL },
    m_MemoryTypeIndex{ 0 },
    m_Type{ (uint8_t)ALLOCATION_TYPE_NONE },
    m_SuballocationType{ (uint8_t)VMA_SUBALLOCATION_TYPE_UNKNOWN },
    m_MapCount{ 0 },
    m_Flags{ 0 }
{
    if(mappingAllowed)
        m_Flags |= (uint8_t)FLAG_MAPPING_ALLOWED;

#if VMA_STATS_STRING_ENABLED
    m_BufferImageUsage = 0;
#endif
}

VmaAllocation_T::~VmaAllocation_T()
{
    VMA_ASSERT(m_MapCount == 0 && "Allocation was not unmapped before destruction.");

    // Check if owned string was freed.
    VMA_ASSERT(m_pName == VMA_NULL);
}

void VmaAllocation_T::InitBlockAllocation(
    VmaDeviceMemoryBlock* block,
    VmaAllocHandle allocHandle,
    VkDeviceSize alignment,
    VkDeviceSize size,
    uint32_t memoryTypeIndex,
    VmaSuballocationType suballocationType,
    bool mapped)
{
    VMA_ASSERT(m_Type == ALLOCATION_TYPE_NONE);
    VMA_ASSERT(block != VMA_NULL);
    m_Type = (uint8_t)ALLOCATION_TYPE_BLOCK;
    m_Alignment = alignment;
    m_Size = size;
    m_MemoryTypeIndex = memoryTypeIndex;
    if(mapped)
    {
        VMA_ASSERT(IsMappingAllowed() && "Mapping is not allowed on this allocation! Please use one of the new VMA_ALLOCATION_CREATE_HOST_ACCESS_* flags when creating it.");
        m_Flags |= (uint8_t)FLAG_PERSISTENT_MAP;
    }
    m_SuballocationType = (uint8_t)suballocationType;
    m_BlockAllocation.m_Block = block;
    m_BlockAllocation.m_AllocHandle = allocHandle;
}

void VmaAllocation_T::InitDedicatedAllocation(
    VmaPool hParentPool,
    uint32_t memoryTypeIndex,
    VkDeviceMemory hMemory,
    VmaSuballocationType suballocationType,
    void* pMappedData,
    VkDeviceSize size)
{
    VMA_ASSERT(m_Type == ALLOCATION_TYPE_NONE);
    VMA_ASSERT(hMemory != VK_NULL_HANDLE);
    m_Type = (uint8_t)ALLOCATION_TYPE_DEDICATED;
    m_Alignment = 0;
    m_Size = size;
    m_MemoryTypeIndex = memoryTypeIndex;
    m_SuballocationType = (uint8_t)suballocationType;
    if(pMappedData != VMA_NULL)
    {
        VMA_ASSERT(IsMappingAllowed() && "Mapping is not allowed on this allocation! Please use one of the new VMA_ALLOCATION_CREATE_HOST_ACCESS_* flags when creating it.");
        m_Flags |= (uint8_t)FLAG_PERSISTENT_MAP;
    }
    m_DedicatedAllocation.m_hParentPool = hParentPool;
    m_DedicatedAllocation.m_hMemory = hMemory;
    m_DedicatedAllocation.m_pMappedData = pMappedData;
    m_DedicatedAllocation.m_Prev = VMA_NULL;
    m_DedicatedAllocation.m_Next = VMA_NULL;
}

void VmaAllocation_T::SetName(VmaAllocator hAllocator, const char* pName)
{
    VMA_ASSERT(pName == VMA_NULL || pName != m_pName);

    FreeName(hAllocator);

    if (pName != VMA_NULL)
        m_pName = VmaCreateStringCopy(hAllocator->GetAllocationCallbacks(), pName);
}

uint8_t VmaAllocation_T::SwapBlockAllocation(VmaAllocator hAllocator, VmaAllocation allocation)
{
    VMA_ASSERT(allocation != VMA_NULL);
    VMA_ASSERT(m_Type == ALLOCATION_TYPE_BLOCK);
    VMA_ASSERT(allocation->m_Type == ALLOCATION_TYPE_BLOCK);

    if (m_MapCount != 0)
        m_BlockAllocation.m_Block->Unmap(hAllocator, m_MapCount);

    m_BlockAllocation.m_Block->m_pMetadata->SetAllocationUserData(m_BlockAllocation.m_AllocHandle, allocation);
    VMA_SWAP(m_BlockAllocation, allocation->m_BlockAllocation);
    m_BlockAllocation.m_Block->m_pMetadata->SetAllocationUserData(m_BlockAllocation.m_AllocHandle, this);

#if VMA_STATS_STRING_ENABLED
    VMA_SWAP(m_BufferImageUsage, allocation->m_BufferImageUsage);
#endif
    return m_MapCount;
}

VmaAllocHandle VmaAllocation_T::GetAllocHandle() const
{
    switch (m_Type)
    {
    case ALLOCATION_TYPE_BLOCK:
        return m_BlockAllocation.m_AllocHandle;
    case ALLOCATION_TYPE_DEDICATED:
        return VK_NULL_HANDLE;
    default:
        VMA_ASSERT(0);
        return VK_NULL_HANDLE;
    }
}

VkDeviceSize VmaAllocation_T::GetOffset() const
{
    switch (m_Type)
    {
    case ALLOCATION_TYPE_BLOCK:
        return m_BlockAllocation.m_Block->m_pMetadata->GetAllocationOffset(m_BlockAllocation.m_AllocHandle);
    case ALLOCATION_TYPE_DEDICATED:
        return 0;
    default:
        VMA_ASSERT(0);
        return 0;
    }
}

VmaPool VmaAllocation_T::GetParentPool() const
{
    switch (m_Type)
    {
    case ALLOCATION_TYPE_BLOCK:
        return m_BlockAllocation.m_Block->GetParentPool();
    case ALLOCATION_TYPE_DEDICATED:
        return m_DedicatedAllocation.m_hParentPool;
    default:
        VMA_ASSERT(0);
        return VK_NULL_HANDLE;
    }
}

VkDeviceMemory VmaAllocation_T::GetMemory() const
{
    switch (m_Type)
    {
    case ALLOCATION_TYPE_BLOCK:
        return m_BlockAllocation.m_Block->GetDeviceMemory();
    case ALLOCATION_TYPE_DEDICATED:
        return m_DedicatedAllocation.m_hMemory;
    default:
        VMA_ASSERT(0);
        return VK_NULL_HANDLE;
    }
}

void* VmaAllocation_T::GetMappedData() const
{
    switch (m_Type)
    {
    case ALLOCATION_TYPE_BLOCK:
        if (m_MapCount != 0 || IsPersistentMap())
        {
            void* pBlockData = m_BlockAllocation.m_Block->GetMappedData();
            VMA_ASSERT(pBlockData != VMA_NULL);
            return (char*)pBlockData + GetOffset();
        }
        else
        {
            return VMA_NULL;
        }
        break;
    case ALLOCATION_TYPE_DEDICATED:
        VMA_ASSERT((m_DedicatedAllocation.m_pMappedData != VMA_NULL) == (m_MapCount != 0 || IsPersistentMap()));
        return m_DedicatedAllocation.m_pMappedData;
    default:
        VMA_ASSERT(0);
        return VMA_NULL;
    }
}

void VmaAllocation_T::BlockAllocMap()
{
    VMA_ASSERT(GetType() == ALLOCATION_TYPE_BLOCK);
    VMA_ASSERT(IsMappingAllowed() && "Mapping is not allowed on this allocation! Please use one of the new VMA_ALLOCATION_CREATE_HOST_ACCESS_* flags when creating it.");

    if (m_MapCount < 0xFF)
    {
        ++m_MapCount;
    }
    else
    {
        VMA_ASSERT(0 && "Allocation mapped too many times simultaneously.");
    }
}

void VmaAllocation_T::BlockAllocUnmap()
{
    VMA_ASSERT(GetType() == ALLOCATION_TYPE_BLOCK);

    if (m_MapCount > 0)
    {
        --m_MapCount;
    }
    else
    {
        VMA_ASSERT(0 && "Unmapping allocation not previously mapped.");
    }
}

VkResult VmaAllocation_T::DedicatedAllocMap(VmaAllocator hAllocator, void** ppData)
{
    VMA_ASSERT(GetType() == ALLOCATION_TYPE_DEDICATED);
    VMA_ASSERT(IsMappingAllowed() && "Mapping is not allowed on this allocation! Please use one of the new VMA_ALLOCATION_CREATE_HOST_ACCESS_* flags when creating it.");

    if (m_MapCount != 0 || IsPersistentMap())
    {
        if (m_MapCount < 0xFF)
        {
            VMA_ASSERT(m_DedicatedAllocation.m_pMappedData != VMA_NULL);
            *ppData = m_DedicatedAllocation.m_pMappedData;
            ++m_MapCount;
            return VK_SUCCESS;
        }
        else
        {
            VMA_ASSERT(0 && "Dedicated allocation mapped too many times simultaneously.");
            return VK_ERROR_MEMORY_MAP_FAILED;
        }
    }
    else
    {
        VkResult result = (*hAllocator->GetVulkanFunctions().vkMapMemory)(
            hAllocator->m_hDevice,
            m_DedicatedAllocation.m_hMemory,
            0, // offset
            VK_WHOLE_SIZE,
            0, // flags
            ppData);
        if (result == VK_SUCCESS)
        {
            m_DedicatedAllocation.m_pMappedData = *ppData;
            m_MapCount = 1;
        }
        return result;
    }
}

void VmaAllocation_T::DedicatedAllocUnmap(VmaAllocator hAllocator)
{
    VMA_ASSERT(GetType() == ALLOCATION_TYPE_DEDICATED);

    if (m_MapCount > 0)
    {
        --m_MapCount;
        if (m_MapCount == 0 && !IsPersistentMap())
        {
            m_DedicatedAllocation.m_pMappedData = VMA_NULL;
            (*hAllocator->GetVulkanFunctions().vkUnmapMemory)(
                hAllocator->m_hDevice,
                m_DedicatedAllocation.m_hMemory);
        }
    }
    else
    {
        VMA_ASSERT(0 && "Unmapping dedicated allocation not previously mapped.");
    }
}

#if VMA_STATS_STRING_ENABLED
void VmaAllocation_T::InitBufferImageUsage(uint32_t bufferImageUsage)
{
    VMA_ASSERT(m_BufferImageUsage == 0);
    m_BufferImageUsage = bufferImageUsage;
}

void VmaAllocation_T::PrintParameters(class VmaJsonWriter& json) const
{
    json.WriteString("Type");
    json.WriteString(VMA_SUBALLOCATION_TYPE_NAMES[m_SuballocationType]);

    json.WriteString("Size");
    json.WriteNumber(m_Size);
    json.WriteString("Usage");
    json.WriteNumber(m_BufferImageUsage);

    if (m_pUserData != VMA_NULL)
    {
        json.WriteString("CustomData");
        json.BeginString();
        json.ContinueString_Pointer(m_pUserData);
        json.EndString();
    }
    if (m_pName != VMA_NULL)
    {
        json.WriteString("Name");
        json.WriteString(m_pName);
    }
}
#endif // VMA_STATS_STRING_ENABLED

void VmaAllocation_T::FreeName(VmaAllocator hAllocator)
{
    if(m_pName)
    {
        VmaFreeString(hAllocator->GetAllocationCallbacks(), m_pName);
        m_pName = VMA_NULL;
    }
}
#endif // _VMA_ALLOCATION_T_FUNCTIONS

#ifndef _VMA_BLOCK_VECTOR_FUNCTIONS
VmaBlockVector::VmaBlockVector(
    VmaAllocator hAllocator,
    VmaPool hParentPool,
    uint32_t memoryTypeIndex,
    VkDeviceSize preferredBlockSize,
    size_t minBlockCount,
    size_t maxBlockCount,
    VkDeviceSize bufferImageGranularity,
    bool explicitBlockSize,
    uint32_t algorithm,
    float priority,
    VkDeviceSize minAllocationAlignment,
    void* pMemoryAllocateNext)
    : m_hAllocator(hAllocator),
    m_hParentPool(hParentPool),
    m_MemoryTypeIndex(memoryTypeIndex),
    m_PreferredBlockSize(preferredBlockSize),
    m_MinBlockCount(minBlockCount),
    m_MaxBlockCount(maxBlockCount),
    m_BufferImageGranularity(bufferImageGranularity),
    m_ExplicitBlockSize(explicitBlockSize),
    m_Algorithm(algorithm),
    m_Priority(priority),
    m_MinAllocationAlignment(minAllocationAlignment),
    m_pMemoryAllocateNext(pMemoryAllocateNext),
    m_Blocks(VmaStlAllocator<VmaDeviceMemoryBlock*>(hAllocator->GetAllocationCallbacks())),
    m_NextBlockId(0) {}

VmaBlockVector::~VmaBlockVector()
{
    for (size_t i = m_Blocks.size(); i--; )
    {
        m_Blocks[i]->Destroy(m_hAllocator);
        vma_delete(m_hAllocator, m_Blocks[i]);
    }
}

VkResult VmaBlockVector::CreateMinBlocks()
{
    for (size_t i = 0; i < m_MinBlockCount; ++i)
    {
        VkResult res = CreateBlock(m_PreferredBlockSize, VMA_NULL);
        if (res != VK_SUCCESS)
        {
            return res;
        }
    }
    return VK_SUCCESS;
}

void VmaBlockVector::AddStatistics(VmaStatistics& inoutStats)
{
    VmaMutexLockRead lock(m_Mutex, m_hAllocator->m_UseMutex);

    const size_t blockCount = m_Blocks.size();
    for (uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex)
    {
        const VmaDeviceMemoryBlock* const pBlock = m_Blocks[blockIndex];
        VMA_ASSERT(pBlock);
        VMA_HEAVY_ASSERT(pBlock->Validate());
        pBlock->m_pMetadata->AddStatistics(inoutStats);
    }
}

void VmaBlockVector::AddDetailedStatistics(VmaDetailedStatistics& inoutStats)
{
    VmaMutexLockRead lock(m_Mutex, m_hAllocator->m_UseMutex);

    const size_t blockCount = m_Blocks.size();
    for (uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex)
    {
        const VmaDeviceMemoryBlock* const pBlock = m_Blocks[blockIndex];
        VMA_ASSERT(pBlock);
        VMA_HEAVY_ASSERT(pBlock->Validate());
        pBlock->m_pMetadata->AddDetailedStatistics(inoutStats);
    }
}

bool VmaBlockVector::IsEmpty()
{
    VmaMutexLockRead lock(m_Mutex, m_hAllocator->m_UseMutex);
    return m_Blocks.empty();
}

bool VmaBlockVector::IsCorruptionDetectionEnabled() const
{
    const uint32_t requiredMemFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return (VMA_DEBUG_DETECT_CORRUPTION != 0) &&
        (VMA_DEBUG_MARGIN > 0) &&
        (m_Algorithm == 0 || m_Algorithm == VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT) &&
        (m_hAllocator->m_MemProps.memoryTypes[m_MemoryTypeIndex].propertyFlags & requiredMemFlags) == requiredMemFlags;
}

VkResult VmaBlockVector::Allocate(
    VkDeviceSize size,
    VkDeviceSize alignment,
    const VmaAllocationCreateInfo& createInfo,
    VmaSuballocationType suballocType,
    size_t allocationCount,
    VmaAllocation* pAllocations)
{
    size_t allocIndex;
    VkResult res = VK_SUCCESS;

    alignment = VMA_MAX(alignment, m_MinAllocationAlignment);

    if (IsCorruptionDetectionEnabled())
    {
        size = VmaAlignUp<VkDeviceSize>(size, sizeof(VMA_CORRUPTION_DETECTION_MAGIC_VALUE));
        alignment = VmaAlignUp<VkDeviceSize>(alignment, sizeof(VMA_CORRUPTION_DETECTION_MAGIC_VALUE));
    }

    {
        VmaMutexLockWrite lock(m_Mutex, m_hAllocator->m_UseMutex);
        for (allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
        {
            res = AllocatePage(
                size,
                alignment,
                createInfo,
                suballocType,
                pAllocations + allocIndex);
            if (res != VK_SUCCESS)
            {
                break;
            }
        }
    }

    if (res != VK_SUCCESS)
    {
        // Free all already created allocations.
        while (allocIndex--)
            Free(pAllocations[allocIndex]);
        memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);
    }

    return res;
}

VkResult VmaBlockVector::AllocatePage(
    VkDeviceSize size,
    VkDeviceSize alignment,
    const VmaAllocationCreateInfo& createInfo,
    VmaSuballocationType suballocType,
    VmaAllocation* pAllocation)
{
    const bool isUpperAddress = (createInfo.flags & VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT) != 0;

    VkDeviceSize freeMemory;
    {
        const uint32_t heapIndex = m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex);
        VmaBudget heapBudget = {};
        m_hAllocator->GetHeapBudgets(&heapBudget, heapIndex, 1);
        freeMemory = (heapBudget.usage < heapBudget.budget) ? (heapBudget.budget - heapBudget.usage) : 0;
    }

    const bool canFallbackToDedicated = !HasExplicitBlockSize() &&
        (createInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0;
    const bool canCreateNewBlock =
        ((createInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0) &&
        (m_Blocks.size() < m_MaxBlockCount) &&
        (freeMemory >= size || !canFallbackToDedicated);
    uint32_t strategy = createInfo.flags & VMA_ALLOCATION_CREATE_STRATEGY_MASK;

    // Upper address can only be used with linear allocator and within single memory block.
    if (isUpperAddress &&
        (m_Algorithm != VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT || m_MaxBlockCount > 1))
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    // Early reject: requested allocation size is larger that maximum block size for this block vector.
    if (size + VMA_DEBUG_MARGIN > m_PreferredBlockSize)
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    // 1. Search existing allocations. Try to allocate.
    if (m_Algorithm == VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT)
    {
        // Use only last block.
        if (!m_Blocks.empty())
        {
            VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks.back();
            VMA_ASSERT(pCurrBlock);
            VkResult res = AllocateFromBlock(
                pCurrBlock, size, alignment, createInfo.flags, createInfo.pUserData, suballocType, strategy, pAllocation);
            if (res == VK_SUCCESS)
            {
                VMA_DEBUG_LOG("    Returned from last block #%u", pCurrBlock->GetId());
                IncrementallySortBlocks();
                return VK_SUCCESS;
            }
        }
    }
    else
    {
        if (strategy != VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT) // MIN_MEMORY or default
        {
            const bool isHostVisible =
                (m_hAllocator->m_MemProps.memoryTypes[m_MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            if(isHostVisible)
            {
                const bool isMappingAllowed = (createInfo.flags &
                    (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0;
                /*
                For non-mappable allocations, check blocks that are not mapped first.
                For mappable allocations, check blocks that are already mapped first.
                This way, having many blocks, we will separate mappable and non-mappable allocations,
                hopefully limiting the number of blocks that are mapped, which will help tools like RenderDoc.
                */
                for(size_t mappingI = 0; mappingI < 2; ++mappingI)
                {
                    // Forward order in m_Blocks - prefer blocks with smallest amount of free space.
                    for (size_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
                    {
                        VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks[blockIndex];
                        VMA_ASSERT(pCurrBlock);
                        const bool isBlockMapped = pCurrBlock->GetMappedData() != VMA_NULL;
                        if((mappingI == 0) == (isMappingAllowed == isBlockMapped))
                        {
                            VkResult res = AllocateFromBlock(
                                pCurrBlock, size, alignment, createInfo.flags, createInfo.pUserData, suballocType, strategy, pAllocation);
                            if (res == VK_SUCCESS)
                            {
                                VMA_DEBUG_LOG("    Returned from existing block #%u", pCurrBlock->GetId());
                                IncrementallySortBlocks();
                                return VK_SUCCESS;
                            }
                        }
                    }
                }
            }
            else
            {
                // Forward order in m_Blocks - prefer blocks with smallest amount of free space.
                for (size_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
                {
                    VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks[blockIndex];
                    VMA_ASSERT(pCurrBlock);
                    VkResult res = AllocateFromBlock(
                        pCurrBlock, size, alignment, createInfo.flags, createInfo.pUserData, suballocType, strategy, pAllocation);
                    if (res == VK_SUCCESS)
                    {
                        VMA_DEBUG_LOG("    Returned from existing block #%u", pCurrBlock->GetId());
                        IncrementallySortBlocks();
                        return VK_SUCCESS;
                    }
                }
            }
        }
        else // VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT
        {
            // Backward order in m_Blocks - prefer blocks with largest amount of free space.
            for (size_t blockIndex = m_Blocks.size(); blockIndex--; )
            {
                VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks[blockIndex];
                VMA_ASSERT(pCurrBlock);
                VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, createInfo.flags, createInfo.pUserData, suballocType, strategy, pAllocation);
                if (res == VK_SUCCESS)
                {
                    VMA_DEBUG_LOG("    Returned from existing block #%u", pCurrBlock->GetId());
                    IncrementallySortBlocks();
                    return VK_SUCCESS;
                }
            }
        }
    }

    // 2. Try to create new block.
    if (canCreateNewBlock)
    {
        // Calculate optimal size for new block.
        VkDeviceSize newBlockSize = m_PreferredBlockSize;
        uint32_t newBlockSizeShift = 0;
        const uint32_t NEW_BLOCK_SIZE_SHIFT_MAX = 3;

        if (!m_ExplicitBlockSize)
        {
            // Allocate 1/8, 1/4, 1/2 as first blocks.
            const VkDeviceSize maxExistingBlockSize = CalcMaxBlockSize();
            for (uint32_t i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
            {
                const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
                if (smallerNewBlockSize > maxExistingBlockSize && smallerNewBlockSize >= size * 2)
                {
                    newBlockSize = smallerNewBlockSize;
                    ++newBlockSizeShift;
                }
                else
                {
                    break;
                }
            }
        }

        size_t newBlockIndex = 0;
        VkResult res = (newBlockSize <= freeMemory || !canFallbackToDedicated) ?
            CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
        // Allocation of this size failed? Try 1/2, 1/4, 1/8 of m_PreferredBlockSize.
        if (!m_ExplicitBlockSize)
        {
            while (res < 0 && newBlockSizeShift < NEW_BLOCK_SIZE_SHIFT_MAX)
            {
                const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
                if (smallerNewBlockSize >= size)
                {
                    newBlockSize = smallerNewBlockSize;
                    ++newBlockSizeShift;
                    res = (newBlockSize <= freeMemory || !canFallbackToDedicated) ?
                        CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
                }
                else
                {
                    break;
                }
            }
        }

        if (res == VK_SUCCESS)
        {
            VmaDeviceMemoryBlock* const pBlock = m_Blocks[newBlockIndex];
            VMA_ASSERT(pBlock->m_pMetadata->GetSize() >= size);

            res = AllocateFromBlock(
                pBlock, size, alignment, createInfo.flags, createInfo.pUserData, suballocType, strategy, pAllocation);
            if (res == VK_SUCCESS)
            {
                VMA_DEBUG_LOG("    Created new block #%u Size=%llu", pBlock->GetId(), newBlockSize);
                IncrementallySortBlocks();
                return VK_SUCCESS;
            }
            else
            {
                // Allocation from new block failed, possibly due to VMA_DEBUG_MARGIN or alignment.
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
        }
    }

    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VmaBlockVector::Free(const VmaAllocation hAllocation)
{
    VmaDeviceMemoryBlock* pBlockToDelete = VMA_NULL;

    bool budgetExceeded = false;
    {
        const uint32_t heapIndex = m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex);
        VmaBudget heapBudget = {};
        m_hAllocator->GetHeapBudgets(&heapBudget, heapIndex, 1);
        budgetExceeded = heapBudget.usage >= heapBudget.budget;
    }

    // Scope for lock.
    {
        VmaMutexLockWrite lock(m_Mutex, m_hAllocator->m_UseMutex);

        VmaDeviceMemoryBlock* pBlock = hAllocation->GetBlock();

        if (IsCorruptionDetectionEnabled())
        {
            VkResult res = pBlock->ValidateMagicValueAfterAllocation(m_hAllocator, hAllocation->GetOffset(), hAllocation->GetSize());
            VMA_ASSERT(res == VK_SUCCESS && "Couldn't map block memory to validate magic value.");
        }

        if (hAllocation->IsPersistentMap())
        {
            pBlock->Unmap(m_hAllocator, 1);
        }

        const bool hadEmptyBlockBeforeFree = HasEmptyBlock();
        pBlock->m_pMetadata->Free(hAllocation->GetAllocHandle());
        pBlock->PostFree(m_hAllocator);
        VMA_HEAVY_ASSERT(pBlock->Validate());

        VMA_DEBUG_LOG("  Freed from MemoryTypeIndex=%u", m_MemoryTypeIndex);

        const bool canDeleteBlock = m_Blocks.size() > m_MinBlockCount;
        // pBlock became empty after this deallocation.
        if (pBlock->m_pMetadata->IsEmpty())
        {
            // Already had empty block. We don't want to have two, so delete this one.
            if ((hadEmptyBlockBeforeFree || budgetExceeded) && canDeleteBlock)
            {
                pBlockToDelete = pBlock;
                Remove(pBlock);
            }
            // else: We now have one empty block - leave it. A hysteresis to avoid allocating whole block back and forth.
        }
        // pBlock didn't become empty, but we have another empty block - find and free that one.
        // (This is optional, heuristics.)
        else if (hadEmptyBlockBeforeFree && canDeleteBlock)
        {
            VmaDeviceMemoryBlock* pLastBlock = m_Blocks.back();
            if (pLastBlock->m_pMetadata->IsEmpty())
            {
                pBlockToDelete = pLastBlock;
                m_Blocks.pop_back();
            }
        }

        IncrementallySortBlocks();
    }

    // Destruction of a free block. Deferred until this point, outside of mutex
    // lock, for performance reason.
    if (pBlockToDelete != VMA_NULL)
    {
        VMA_DEBUG_LOG("    Deleted empty block #%u", pBlockToDelete->GetId());
        pBlockToDelete->Destroy(m_hAllocator);
        vma_delete(m_hAllocator, pBlockToDelete);
    }

    m_hAllocator->m_Budget.RemoveAllocation(m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex), hAllocation->GetSize());
    m_hAllocator->m_AllocationObjectAllocator.Free(hAllocation);
}

VkDeviceSize VmaBlockVector::CalcMaxBlockSize() const
{
    VkDeviceSize result = 0;
    for (size_t i = m_Blocks.size(); i--; )
    {
        result = VMA_MAX(result, m_Blocks[i]->m_pMetadata->GetSize());
        if (result >= m_PreferredBlockSize)
        {
            break;
        }
    }
    return result;
}

void VmaBlockVector::Remove(VmaDeviceMemoryBlock* pBlock)
{
    for (uint32_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
    {
        if (m_Blocks[blockIndex] == pBlock)
        {
            VmaVectorRemove(m_Blocks, blockIndex);
            return;
        }
    }
    VMA_ASSERT(0);
}

void VmaBlockVector::IncrementallySortBlocks()
{
    if (!m_IncrementalSort)
        return;
    if (m_Algorithm != VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT)
    {
        // Bubble sort only until first swap.
        for (size_t i = 1; i < m_Blocks.size(); ++i)
        {
            if (m_Blocks[i - 1]->m_pMetadata->GetSumFreeSize() > m_Blocks[i]->m_pMetadata->GetSumFreeSize())
            {
                VMA_SWAP(m_Blocks[i - 1], m_Blocks[i]);
                return;
            }
        }
    }
}

void VmaBlockVector::SortByFreeSize()
{
    VMA_SORT(m_Blocks.begin(), m_Blocks.end(),
        [](VmaDeviceMemoryBlock* b1, VmaDeviceMemoryBlock* b2) -> bool
        {
            return b1->m_pMetadata->GetSumFreeSize() < b2->m_pMetadata->GetSumFreeSize();
        });
}

VkResult VmaBlockVector::AllocateFromBlock(
    VmaDeviceMemoryBlock* pBlock,
    VkDeviceSize size,
    VkDeviceSize alignment,
    VmaAllocationCreateFlags allocFlags,
    void* pUserData,
    VmaSuballocationType suballocType,
    uint32_t strategy,
    VmaAllocation* pAllocation)
{
    const bool isUpperAddress = (allocFlags & VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT) != 0;

    VmaAllocationRequest currRequest = {};
    if (pBlock->m_pMetadata->CreateAllocationRequest(
        size,
        alignment,
        isUpperAddress,
        suballocType,
        strategy,
        &currRequest))
    {
        return CommitAllocationRequest(currRequest, pBlock, alignment, allocFlags, pUserData, suballocType, pAllocation);
    }
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VkResult VmaBlockVector::CommitAllocationRequest(
    VmaAllocationRequest& allocRequest,
    VmaDeviceMemoryBlock* pBlock,
    VkDeviceSize alignment,
    VmaAllocationCreateFlags allocFlags,
    void* pUserData,
    VmaSuballocationType suballocType,
    VmaAllocation* pAllocation)
{
    const bool mapped = (allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
    const bool isUserDataString = (allocFlags & VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT) != 0;
    const bool isMappingAllowed = (allocFlags &
        (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0;

    pBlock->PostAlloc();
    // Allocate from pCurrBlock.
    if (mapped)
    {
        VkResult res = pBlock->Map(m_hAllocator, 1, VMA_NULL);
        if (res != VK_SUCCESS)
        {
            return res;
        }
    }

    *pAllocation = m_hAllocator->m_AllocationObjectAllocator.Allocate(isMappingAllowed);
    pBlock->m_pMetadata->Alloc(allocRequest, suballocType, *pAllocation);
    (*pAllocation)->InitBlockAllocation(
        pBlock,
        allocRequest.allocHandle,
        alignment,
        allocRequest.size, // Not size, as actual allocation size may be larger than requested!
        m_MemoryTypeIndex,
        suballocType,
        mapped);
    VMA_HEAVY_ASSERT(pBlock->Validate());
    if (isUserDataString)
        (*pAllocation)->SetName(m_hAllocator, (const char*)pUserData);
    else
        (*pAllocation)->SetUserData(m_hAllocator, pUserData);
    m_hAllocator->m_Budget.AddAllocation(m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex), allocRequest.size);
    if (VMA_DEBUG_INITIALIZE_ALLOCATIONS)
    {
        m_hAllocator->FillAllocation(*pAllocation, VMA_ALLOCATION_FILL_PATTERN_CREATED);
    }
    if (IsCorruptionDetectionEnabled())
    {
        VkResult res = pBlock->WriteMagicValueAfterAllocation(m_hAllocator, (*pAllocation)->GetOffset(), allocRequest.size);
        VMA_ASSERT(res == VK_SUCCESS && "Couldn't map block memory to write magic value.");
    }
    return VK_SUCCESS;
}

VkResult VmaBlockVector::CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex)
{
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.pNext = m_pMemoryAllocateNext;
    allocInfo.memoryTypeIndex = m_MemoryTypeIndex;
    allocInfo.allocationSize = blockSize;

#if VMA_BUFFER_DEVICE_ADDRESS
    // Every standalone block can potentially contain a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT - always enable the feature.
    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
    if (m_hAllocator->m_UseKhrBufferDeviceAddress)
    {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        VmaPnextChainPushFront(&allocInfo, &allocFlagsInfo);
    }
#endif // VMA_BUFFER_DEVICE_ADDRESS

#if VMA_MEMORY_PRIORITY
    VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
    if (m_hAllocator->m_UseExtMemoryPriority)
    {
        VMA_ASSERT(m_Priority >= 0.f && m_Priority <= 1.f);
        priorityInfo.priority = m_Priority;
        VmaPnextChainPushFront(&allocInfo, &priorityInfo);
    }
#endif // VMA_MEMORY_PRIORITY

#if VMA_EXTERNAL_MEMORY
    // Attach VkExportMemoryAllocateInfoKHR if necessary.
    VkExportMemoryAllocateInfoKHR exportMemoryAllocInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR };
    exportMemoryAllocInfo.handleTypes = m_hAllocator->GetExternalMemoryHandleTypeFlags(m_MemoryTypeIndex);
    if (exportMemoryAllocInfo.handleTypes != 0)
    {
        VmaPnextChainPushFront(&allocInfo, &exportMemoryAllocInfo);
    }
#endif // VMA_EXTERNAL_MEMORY

    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkResult res = m_hAllocator->AllocateVulkanMemory(&allocInfo, &mem);
    if (res < 0)
    {
        return res;
    }

    // New VkDeviceMemory successfully created.

    // Create new Allocation for it.
    VmaDeviceMemoryBlock* const pBlock = vma_new(m_hAllocator, VmaDeviceMemoryBlock)(m_hAllocator);
    pBlock->Init(
        m_hAllocator,
        m_hParentPool,
        m_MemoryTypeIndex,
        mem,
        allocInfo.allocationSize,
        m_NextBlockId++,
        m_Algorithm,
        m_BufferImageGranularity);

    m_Blocks.push_back(pBlock);
    if (pNewBlockIndex != VMA_NULL)
    {
        *pNewBlockIndex = m_Blocks.size() - 1;
    }

    return VK_SUCCESS;
}

bool VmaBlockVector::HasEmptyBlock()
{
    for (size_t index = 0, count = m_Blocks.size(); index < count; ++index)
    {
        VmaDeviceMemoryBlock* const pBlock = m_Blocks[index];
        if (pBlock->m_pMetadata->IsEmpty())
        {
            return true;
        }
    }
    return false;
}

#if VMA_STATS_STRING_ENABLED
void VmaBlockVector::PrintDetailedMap(class VmaJsonWriter& json)
{
    VmaMutexLockRead lock(m_Mutex, m_hAllocator->m_UseMutex);


    json.BeginObject();
    for (size_t i = 0; i < m_Blocks.size(); ++i)
    {
        json.BeginString();
        json.ContinueString(m_Blocks[i]->GetId());
        json.EndString();

        json.BeginObject();
        json.WriteString("MapRefCount");
        json.WriteNumber(m_Blocks[i]->GetMapRefCount());

        m_Blocks[i]->m_pMetadata->PrintDetailedMap(json);
        json.EndObject();
    }
    json.EndObject();
}
#endif // VMA_STATS_STRING_ENABLED

VkResult VmaBlockVector::CheckCorruption()
{
    if (!IsCorruptionDetectionEnabled())
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    VmaMutexLockRead lock(m_Mutex, m_hAllocator->m_UseMutex);
    for (uint32_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
    {
        VmaDeviceMemoryBlock* const pBlock = m_Blocks[blockIndex];
        VMA_ASSERT(pBlock);
        VkResult res = pBlock->CheckCorruption(m_hAllocator);
        if (res != VK_SUCCESS)
        {
            return res;
        }
    }
    return VK_SUCCESS;
}

#endif // _VMA_BLOCK_VECTOR_FUNCTIONS

#ifndef _VMA_DEFRAGMENTATION_CONTEXT_FUNCTIONS
VmaDefragmentationContext_T::VmaDefragmentationContext_T(
    VmaAllocator hAllocator,
    const VmaDefragmentationInfo& info)
    : m_MaxPassBytes(info.maxBytesPerPass == 0 ? VK_WHOLE_SIZE : info.maxBytesPerPass),
    m_MaxPassAllocations(info.maxAllocationsPerPass == 0 ? UINT32_MAX : info.maxAllocationsPerPass),
    m_MoveAllocator(hAllocator->GetAllocationCallbacks()),
    m_Moves(m_MoveAllocator)
{
    m_Algorithm = info.flags & VMA_DEFRAGMENTATION_FLAG_ALGORITHM_MASK;

    if (info.pool != VMA_NULL)
    {
        m_BlockVectorCount = 1;
        m_PoolBlockVector = &info.pool->m_BlockVector;
        m_pBlockVectors = &m_PoolBlockVector;
        m_PoolBlockVector->SetIncrementalSort(false);
        m_PoolBlockVector->SortByFreeSize();
    }
    else
    {
        m_BlockVectorCount = hAllocator->GetMemoryTypeCount();
        m_PoolBlockVector = VMA_NULL;
        m_pBlockVectors = hAllocator->m_pBlockVectors;
        for (uint32_t i = 0; i < m_BlockVectorCount; ++i)
        {
            VmaBlockVector* vector = m_pBlockVectors[i];
            if (vector != VMA_NULL)
            {
                vector->SetIncrementalSort(false);
                vector->SortByFreeSize();
            }
        }
    }

    switch (m_Algorithm)
    {
    case 0: // Default algorithm
        m_Algorithm = VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT;
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT:
    {
        m_AlgorithmState = vma_new_array(hAllocator, StateBalanced, m_BlockVectorCount);
        break;
    }
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT:
    {
        if (hAllocator->GetBufferImageGranularity() > 1)
        {
            m_AlgorithmState = vma_new_array(hAllocator, StateExtensive, m_BlockVectorCount);
        }
        break;
    }
    }
}

VmaDefragmentationContext_T::~VmaDefragmentationContext_T()
{
    if (m_PoolBlockVector != VMA_NULL)
    {
        m_PoolBlockVector->SetIncrementalSort(true);
    }
    else
    {
        for (uint32_t i = 0; i < m_BlockVectorCount; ++i)
        {
            VmaBlockVector* vector = m_pBlockVectors[i];
            if (vector != VMA_NULL)
                vector->SetIncrementalSort(true);
        }
    }

    if (m_AlgorithmState)
    {
        switch (m_Algorithm)
        {
        case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT:
            vma_delete_array(m_MoveAllocator.m_pCallbacks, reinterpret_cast<StateBalanced*>(m_AlgorithmState), m_BlockVectorCount);
            break;
        case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT:
            vma_delete_array(m_MoveAllocator.m_pCallbacks, reinterpret_cast<StateExtensive*>(m_AlgorithmState), m_BlockVectorCount);
            break;
        default:
            VMA_ASSERT(0);
        }
    }
}

VkResult VmaDefragmentationContext_T::DefragmentPassBegin(VmaDefragmentationPassMoveInfo& moveInfo)
{
    if (m_PoolBlockVector != VMA_NULL)
    {
        VmaMutexLockWrite lock(m_PoolBlockVector->GetMutex(), m_PoolBlockVector->GetAllocator()->m_UseMutex);

        if (m_PoolBlockVector->GetBlockCount() > 1)
            ComputeDefragmentation(*m_PoolBlockVector, 0);
        else if (m_PoolBlockVector->GetBlockCount() == 1)
            ReallocWithinBlock(*m_PoolBlockVector, m_PoolBlockVector->GetBlock(0));
    }
    else
    {
        for (uint32_t i = 0; i < m_BlockVectorCount; ++i)
        {
            if (m_pBlockVectors[i] != VMA_NULL)
            {
                VmaMutexLockWrite lock(m_pBlockVectors[i]->GetMutex(), m_pBlockVectors[i]->GetAllocator()->m_UseMutex);

                if (m_pBlockVectors[i]->GetBlockCount() > 1)
                {
                    if (ComputeDefragmentation(*m_pBlockVectors[i], i))
                        break;
                }
                else if (m_pBlockVectors[i]->GetBlockCount() == 1)
                {
                    if (ReallocWithinBlock(*m_pBlockVectors[i], m_pBlockVectors[i]->GetBlock(0)))
                        break;
                }
            }
        }
    }

    moveInfo.moveCount = static_cast<uint32_t>(m_Moves.size());
    if (moveInfo.moveCount > 0)
    {
        moveInfo.pMoves = m_Moves.data();
        return VK_INCOMPLETE;
    }

    moveInfo.pMoves = VMA_NULL;
    return VK_SUCCESS;
}

VkResult VmaDefragmentationContext_T::DefragmentPassEnd(VmaDefragmentationPassMoveInfo& moveInfo)
{
    VMA_ASSERT(moveInfo.moveCount > 0 ? moveInfo.pMoves != VMA_NULL : true);

    VkResult result = VK_SUCCESS;
    VmaStlAllocator<FragmentedBlock> blockAllocator(m_MoveAllocator.m_pCallbacks);
    VmaVector<FragmentedBlock, VmaStlAllocator<FragmentedBlock>> immovableBlocks(blockAllocator);
    VmaVector<FragmentedBlock, VmaStlAllocator<FragmentedBlock>> mappedBlocks(blockAllocator);

    VmaAllocator allocator = VMA_NULL;
    for (uint32_t i = 0; i < moveInfo.moveCount; ++i)
    {
        VmaDefragmentationMove& move = moveInfo.pMoves[i];
        size_t prevCount = 0, currentCount = 0;
        VkDeviceSize freedBlockSize = 0;

        uint32_t vectorIndex;
        VmaBlockVector* vector;
        if (m_PoolBlockVector != VMA_NULL)
        {
            vectorIndex = 0;
            vector = m_PoolBlockVector;
        }
        else
        {
            vectorIndex = move.srcAllocation->GetMemoryTypeIndex();
            vector = m_pBlockVectors[vectorIndex];
            VMA_ASSERT(vector != VMA_NULL);
        }

        switch (move.operation)
        {
        case VMA_DEFRAGMENTATION_MOVE_OPERATION_COPY:
        {
            uint8_t mapCount = move.srcAllocation->SwapBlockAllocation(vector->m_hAllocator, move.dstTmpAllocation);
            if (mapCount > 0)
            {
                allocator = vector->m_hAllocator;
                VmaDeviceMemoryBlock* newMapBlock = move.srcAllocation->GetBlock();
                bool notPresent = true;
                for (FragmentedBlock& block : mappedBlocks)
                {
                    if (block.block == newMapBlock)
                    {
                        notPresent = false;
                        block.data += mapCount;
                        break;
                    }
                }
                if (notPresent)
                    mappedBlocks.push_back({ mapCount, newMapBlock });
            }

            // Scope for locks, Free have it's own lock
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                prevCount = vector->GetBlockCount();
                freedBlockSize = move.dstTmpAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            vector->Free(move.dstTmpAllocation);
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                currentCount = vector->GetBlockCount();
            }

            result = VK_INCOMPLETE;
            break;
        }
        case VMA_DEFRAGMENTATION_MOVE_OPERATION_IGNORE:
        {
            m_PassStats.bytesMoved -= move.srcAllocation->GetSize();
            --m_PassStats.allocationsMoved;
            vector->Free(move.dstTmpAllocation);

            VmaDeviceMemoryBlock* newBlock = move.srcAllocation->GetBlock();
            bool notPresent = true;
            for (const FragmentedBlock& block : immovableBlocks)
            {
                if (block.block == newBlock)
                {
                    notPresent = false;
                    break;
                }
            }
            if (notPresent)
                immovableBlocks.push_back({ vectorIndex, newBlock });
            break;
        }
        case VMA_DEFRAGMENTATION_MOVE_OPERATION_DESTROY:
        {
            m_PassStats.bytesMoved -= move.srcAllocation->GetSize();
            --m_PassStats.allocationsMoved;
            // Scope for locks, Free have it's own lock
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                prevCount = vector->GetBlockCount();
                freedBlockSize = move.srcAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            vector->Free(move.srcAllocation);
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                currentCount = vector->GetBlockCount();
            }
            freedBlockSize *= prevCount - currentCount;

            VkDeviceSize dstBlockSize;
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                dstBlockSize = move.dstTmpAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            vector->Free(move.dstTmpAllocation);
            {
                VmaMutexLockRead lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);
                freedBlockSize += dstBlockSize * (currentCount - vector->GetBlockCount());
                currentCount = vector->GetBlockCount();
            }

            result = VK_INCOMPLETE;
            break;
        }
        default:
            VMA_ASSERT(0);
        }

        if (prevCount > currentCount)
        {
            size_t freedBlocks = prevCount - currentCount;
            m_PassStats.deviceMemoryBlocksFreed += static_cast<uint32_t>(freedBlocks);
            m_PassStats.bytesFreed += freedBlockSize;
        }

        switch (m_Algorithm)
        {
        case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT:
        {
            if (m_AlgorithmState != VMA_NULL)
            {
                // Avoid unnecessary tries to allocate when new free block is avaiable
                StateExtensive& state = reinterpret_cast<StateExtensive*>(m_AlgorithmState)[vectorIndex];
                if (state.firstFreeBlock != SIZE_MAX)
                {
                    const size_t diff = prevCount - currentCount;
                    if (state.firstFreeBlock >= diff)
                    {
                        state.firstFreeBlock -= diff;
                        if (state.firstFreeBlock != 0)
                            state.firstFreeBlock -= vector->GetBlock(state.firstFreeBlock - 1)->m_pMetadata->IsEmpty();
                    }
                    else
                        state.firstFreeBlock = 0;
                }
            }
        }
        }
    }
    moveInfo.moveCount = 0;
    moveInfo.pMoves = VMA_NULL;
    m_Moves.clear();

    // Update stats
    m_GlobalStats.allocationsMoved += m_PassStats.allocationsMoved;
    m_GlobalStats.bytesFreed += m_PassStats.bytesFreed;
    m_GlobalStats.bytesMoved += m_PassStats.bytesMoved;
    m_GlobalStats.deviceMemoryBlocksFreed += m_PassStats.deviceMemoryBlocksFreed;
    m_PassStats = { 0 };

    // Move blocks with immovable allocations according to algorithm
    if (immovableBlocks.size() > 0)
    {
        switch (m_Algorithm)
        {
        case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT:
        {
            if (m_AlgorithmState != VMA_NULL)
            {
                bool swapped = false;
                // Move to the start of free blocks range
                for (const FragmentedBlock& block : immovableBlocks)
                {
                    StateExtensive& state = reinterpret_cast<StateExtensive*>(m_AlgorithmState)[block.data];
                    if (state.operation != StateExtensive::Operation::Cleanup)
                    {
                        VmaBlockVector* vector = m_pBlockVectors[block.data];
                        VmaMutexLockWrite lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);

                        for (size_t i = 0, count = vector->GetBlockCount() - m_ImmovableBlockCount; i < count; ++i)
                        {
                            if (vector->GetBlock(i) == block.block)
                            {
                                VMA_SWAP(vector->m_Blocks[i], vector->m_Blocks[vector->GetBlockCount() - ++m_ImmovableBlockCount]);
                                if (state.firstFreeBlock != SIZE_MAX)
                                {
                                    if (i + 1 < state.firstFreeBlock)
                                    {
                                        if (state.firstFreeBlock > 1)
                                            VMA_SWAP(vector->m_Blocks[i], vector->m_Blocks[--state.firstFreeBlock]);
                                        else
                                            --state.firstFreeBlock;
                                    }
                                }
                                swapped = true;
                                break;
                            }
                        }
                    }
                }
                if (swapped)
                    result = VK_INCOMPLETE;
                break;
            }
        }
        default:
        {
            // Move to the begining
            for (const FragmentedBlock& block : immovableBlocks)
            {
                VmaBlockVector* vector = m_pBlockVectors[block.data];
                VmaMutexLockWrite lock(vector->GetMutex(), vector->GetAllocator()->m_UseMutex);

                for (size_t i = m_ImmovableBlockCount; i < vector->GetBlockCount(); ++i)
                {
                    if (vector->GetBlock(i) == block.block)
                    {
                        VMA_SWAP(vector->m_Blocks[i], vector->m_Blocks[m_ImmovableBlockCount++]);
                        break;
                    }
                }
            }
            break;
        }
        }
    }

    // Bulk-map destination blocks
    for (const FragmentedBlock& block : mappedBlocks)
    {
        VkResult res = block.block->Map(allocator, block.data, VMA_NULL);
        VMA_ASSERT(res == VK_SUCCESS);
    }
    return result;
}

bool VmaDefragmentationContext_T::ComputeDefragmentation(VmaBlockVector& vector, size_t index)
{
    switch (m_Algorithm)
    {
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FAST_BIT:
        return ComputeDefragmentation_Fast(vector);
    default:
        VMA_ASSERT(0);
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT:
        return ComputeDefragmentation_Balanced(vector, index, true);
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FULL_BIT:
        return ComputeDefragmentation_Full(vector);
    case VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT:
        return ComputeDefragmentation_Extensive(vector, index);
    }
}

VmaDefragmentationContext_T::MoveAllocationData VmaDefragmentationContext_T::GetMoveData(
    VmaAllocHandle handle, VmaBlockMetadata* metadata)
{
    MoveAllocationData moveData;
    moveData.move.srcAllocation = (VmaAllocation)metadata->GetAllocationUserData(handle);
    moveData.size = moveData.move.srcAllocation->GetSize();
    moveData.alignment = moveData.move.srcAllocation->GetAlignment();
    moveData.type = moveData.move.srcAllocation->GetSuballocationType();
    moveData.flags = 0;

    if (moveData.move.srcAllocation->IsPersistentMap())
        moveData.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if (moveData.move.srcAllocation->IsMappingAllowed())
        moveData.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    return moveData;
}

VmaDefragmentationContext_T::CounterStatus VmaDefragmentationContext_T::CheckCounters(VkDeviceSize bytes)
{
    // Ignore allocation if will exceed max size for copy
    if (m_PassStats.bytesMoved + bytes > m_MaxPassBytes)
    {
        if (++m_IgnoredAllocs < MAX_ALLOCS_TO_IGNORE)
            return CounterStatus::Ignore;
        else
            return CounterStatus::End;
    }
    return CounterStatus::Pass;
}

bool VmaDefragmentationContext_T::IncrementCounters(VkDeviceSize bytes)
{
    m_PassStats.bytesMoved += bytes;
    // Early return when max found
    if (++m_PassStats.allocationsMoved >= m_MaxPassAllocations || m_PassStats.bytesMoved >= m_MaxPassBytes)
    {
        VMA_ASSERT(m_PassStats.allocationsMoved == m_MaxPassAllocations ||
            m_PassStats.bytesMoved == m_MaxPassBytes && "Exceeded maximal pass threshold!");
        return true;
    }
    return false;
}

bool VmaDefragmentationContext_T::ReallocWithinBlock(VmaBlockVector& vector, VmaDeviceMemoryBlock* block)
{
    VmaBlockMetadata* metadata = block->m_pMetadata;

    for (VmaAllocHandle handle = metadata->GetAllocationListBegin();
        handle != VK_NULL_HANDLE;
        handle = metadata->GetNextAllocation(handle))
    {
        MoveAllocationData moveData = GetMoveData(handle, metadata);
        // Ignore newly created allocations by defragmentation algorithm
        if (moveData.move.srcAllocation->GetUserData() == this)
            continue;
        switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
        {
        case CounterStatus::Ignore:
            continue;
        case CounterStatus::End:
            return true;
        default:
            VMA_ASSERT(0);
        case CounterStatus::Pass:
            break;
        }

        VkDeviceSize offset = moveData.move.srcAllocation->GetOffset();
        if (offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
        {
            VmaAllocationRequest request = {};
            if (metadata->CreateAllocationRequest(
                moveData.size,
                moveData.alignment,
                false,
                moveData.type,
                VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,
                &request))
            {
                if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                {
                    if (vector.CommitAllocationRequest(
                        request,
                        block,
                        moveData.alignment,
                        moveData.flags,
                        this,
                        moveData.type,
                        &moveData.move.dstTmpAllocation) == VK_SUCCESS)
                    {
                        m_Moves.push_back(moveData.move);
                        if (IncrementCounters(moveData.size))
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

bool VmaDefragmentationContext_T::AllocInOtherBlock(size_t start, size_t end, MoveAllocationData& data, VmaBlockVector& vector)
{
    for (; start < end; ++start)
    {
        VmaDeviceMemoryBlock* dstBlock = vector.GetBlock(start);
        if (dstBlock->m_pMetadata->GetSumFreeSize() >= data.size)
        {
            if (vector.AllocateFromBlock(dstBlock,
                data.size,
                data.alignment,
                data.flags,
                this,
                data.type,
                0,
                &data.move.dstTmpAllocation) == VK_SUCCESS)
            {
                m_Moves.push_back(data.move);
                if (IncrementCounters(data.size))
                    return true;
                break;
            }
        }
    }
    return false;
}

bool VmaDefragmentationContext_T::ComputeDefragmentation_Fast(VmaBlockVector& vector)
{
    // Move only between blocks

    // Go through allocations in last blocks and try to fit them inside first ones
    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        VmaBlockMetadata* metadata = vector.GetBlock(i)->m_pMetadata;

        for (VmaAllocHandle handle = metadata->GetAllocationListBegin();
            handle != VK_NULL_HANDLE;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.srcAllocation->GetUserData() == this)
                continue;
            switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                VMA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;
        }
    }
    return false;
}

bool VmaDefragmentationContext_T::ComputeDefragmentation_Balanced(VmaBlockVector& vector, size_t index, bool update)
{
    // Go over every allocation and try to fit it in previous blocks at lowest offsets,
    // if not possible: realloc within single block to minimize offset (exclude offset == 0),
    // but only if there are noticable gaps between them (some heuristic, ex. average size of allocation in block)
    VMA_ASSERT(m_AlgorithmState != VMA_NULL);

    StateBalanced& vectorState = reinterpret_cast<StateBalanced*>(m_AlgorithmState)[index];
    if (update && vectorState.avgAllocSize == UINT64_MAX)
        UpdateVectorStatistics(vector, vectorState);

    const size_t startMoveCount = m_Moves.size();
    VkDeviceSize minimalFreeRegion = vectorState.avgFreeSize / 2;
    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        VmaDeviceMemoryBlock* block = vector.GetBlock(i);
        VmaBlockMetadata* metadata = block->m_pMetadata;
        VkDeviceSize prevFreeRegionSize = 0;

        for (VmaAllocHandle handle = metadata->GetAllocationListBegin();
            handle != VK_NULL_HANDLE;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.srcAllocation->GetUserData() == this)
                continue;
            switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                VMA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            const size_t prevMoveCount = m_Moves.size();
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;

            VkDeviceSize nextFreeRegionSize = metadata->GetNextFreeRegionSize(handle);
            // If no room found then realloc within block for lower offset
            VkDeviceSize offset = moveData.move.srcAllocation->GetOffset();
            if (prevMoveCount == m_Moves.size() && offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
            {
                // Check if realloc will make sense
                if (prevFreeRegionSize >= minimalFreeRegion ||
                    nextFreeRegionSize >= minimalFreeRegion ||
                    moveData.size <= vectorState.avgFreeSize ||
                    moveData.size <= vectorState.avgAllocSize)
                {
                    VmaAllocationRequest request = {};
                    if (metadata->CreateAllocationRequest(
                        moveData.size,
                        moveData.alignment,
                        false,
                        moveData.type,
                        VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,
                        &request))
                    {
                        if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                        {
                            if (vector.CommitAllocationRequest(
                                request,
                                block,
                                moveData.alignment,
                                moveData.flags,
                                this,
                                moveData.type,
                                &moveData.move.dstTmpAllocation) == VK_SUCCESS)
                            {
                                m_Moves.push_back(moveData.move);
                                if (IncrementCounters(moveData.size))
                                    return true;
                            }
                        }
                    }
                }
            }
            prevFreeRegionSize = nextFreeRegionSize;
        }
    }

    // No moves perfomed, update statistics to current vector state
    if (startMoveCount == m_Moves.size() && !update)
    {
        vectorState.avgAllocSize = UINT64_MAX;
        return ComputeDefragmentation_Balanced(vector, index, false);
    }
    return false;
}

bool VmaDefragmentationContext_T::ComputeDefragmentation_Full(VmaBlockVector& vector)
{
    // Go over every allocation and try to fit it in previous blocks at lowest offsets,
    // if not possible: realloc within single block to minimize offset (exclude offset == 0)

    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        VmaDeviceMemoryBlock* block = vector.GetBlock(i);
        VmaBlockMetadata* metadata = block->m_pMetadata;

        for (VmaAllocHandle handle = metadata->GetAllocationListBegin();
            handle != VK_NULL_HANDLE;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.srcAllocation->GetUserData() == this)
                continue;
            switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                VMA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            const size_t prevMoveCount = m_Moves.size();
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;

            // If no room found then realloc within block for lower offset
            VkDeviceSize offset = moveData.move.srcAllocation->GetOffset();
            if (prevMoveCount == m_Moves.size() && offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
            {
                VmaAllocationRequest request = {};
                if (metadata->CreateAllocationRequest(
                    moveData.size,
                    moveData.alignment,
                    false,
                    moveData.type,
                    VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,
                    &request))
                {
                    if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                    {
                        if (vector.CommitAllocationRequest(
                            request,
                            block,
                            moveData.alignment,
                            moveData.flags,
                            this,
                            moveData.type,
                            &moveData.move.dstTmpAllocation) == VK_SUCCESS)
                        {
                            m_Moves.push_back(moveData.move);
                            if (IncrementCounters(moveData.size))
                                return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool VmaDefragmentationContext_T::ComputeDefragmentation_Extensive(VmaBlockVector& vector, size_t index)
{
    // First free single block, then populate it to the brim, then free another block, and so on

    // Fallback to previous algorithm since without granularity conflicts it can achieve max packing
    if (vector.m_BufferImageGranularity == 1)
        return ComputeDefragmentation_Full(vector);

    VMA_ASSERT(m_AlgorithmState != VMA_NULL);

    StateExtensive& vectorState = reinterpret_cast<StateExtensive*>(m_AlgorithmState)[index];

    bool texturePresent = false, bufferPresent = false, otherPresent = false;
    switch (vectorState.operation)
    {
    case StateExtensive::Operation::Done: // Vector defragmented
        return false;
    case StateExtensive::Operation::FindFreeBlockBuffer:
    case StateExtensive::Operation::FindFreeBlockTexture:
    case StateExtensive::Operation::FindFreeBlockAll:
    {
        // No more blocks to free, just perform fast realloc and move to cleanup
        if (vectorState.firstFreeBlock == 0)
        {
            vectorState.operation = StateExtensive::Operation::Cleanup;
            return ComputeDefragmentation_Fast(vector);
        }

        // No free blocks, have to clear last one
        size_t last = (vectorState.firstFreeBlock == SIZE_MAX ? vector.GetBlockCount() : vectorState.firstFreeBlock) - 1;
        VmaBlockMetadata* freeMetadata = vector.GetBlock(last)->m_pMetadata;

        const size_t prevMoveCount = m_Moves.size();
        for (VmaAllocHandle handle = freeMetadata->GetAllocationListBegin();
            handle != VK_NULL_HANDLE;
            handle = freeMetadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, freeMetadata);
            switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                VMA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            if (AllocInOtherBlock(0, last, moveData, vector))
            {
                // Full clear performed already
                if (prevMoveCount != m_Moves.size() && freeMetadata->GetNextAllocation(handle) == VK_NULL_HANDLE)
                    reinterpret_cast<size_t*>(m_AlgorithmState)[index] = last;
                return true;
            }
        }

        if (prevMoveCount == m_Moves.size())
        {
            // Cannot perform full clear, have to move data in other blocks around
            if (last != 0)
            {
                for (size_t i = last - 1; i; --i)
                {
                    if (ReallocWithinBlock(vector, vector.GetBlock(i)))
                        return true;
                }
            }

            if (prevMoveCount == m_Moves.size())
            {
                // No possible reallocs within blocks, try to move them around fast
                return ComputeDefragmentation_Fast(vector);
            }
        }
        else
        {
            switch (vectorState.operation)
            {
            case StateExtensive::Operation::FindFreeBlockBuffer:
                vectorState.operation = StateExtensive::Operation::MoveBuffers;
                break;
            default:
                VMA_ASSERT(0);
            case StateExtensive::Operation::FindFreeBlockTexture:
                vectorState.operation = StateExtensive::Operation::MoveTextures;
                break;
            case StateExtensive::Operation::FindFreeBlockAll:
                vectorState.operation = StateExtensive::Operation::MoveAll;
                break;
            }
            vectorState.firstFreeBlock = last;
            // Nothing done, block found without reallocations, can perform another reallocs in same pass
            return ComputeDefragmentation_Extensive(vector, index);
        }
        break;
    }
    case StateExtensive::Operation::MoveTextures:
    {
        if (MoveDataToFreeBlocks(VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL, vector,
            vectorState.firstFreeBlock, texturePresent, bufferPresent, otherPresent))
        {
            if (texturePresent)
            {
                vectorState.operation = StateExtensive::Operation::FindFreeBlockTexture;
                return ComputeDefragmentation_Extensive(vector, index);
            }

            if (!bufferPresent && !otherPresent)
            {
                vectorState.operation = StateExtensive::Operation::Cleanup;
                break;
            }

            // No more textures to move, check buffers
            vectorState.operation = StateExtensive::Operation::MoveBuffers;
            bufferPresent = false;
            otherPresent = false;
        }
        else
            break;
    }
    case StateExtensive::Operation::MoveBuffers:
    {
        if (MoveDataToFreeBlocks(VMA_SUBALLOCATION_TYPE_BUFFER, vector,
            vectorState.firstFreeBlock, texturePresent, bufferPresent, otherPresent))
        {
            if (bufferPresent)
            {
                vectorState.operation = StateExtensive::Operation::FindFreeBlockBuffer;
                return ComputeDefragmentation_Extensive(vector, index);
            }

            if (!otherPresent)
            {
                vectorState.operation = StateExtensive::Operation::Cleanup;
                break;
            }

            // No more buffers to move, check all others
            vectorState.operation = StateExtensive::Operation::MoveAll;
            otherPresent = false;
        }
        else
            break;
    }
    case StateExtensive::Operation::MoveAll:
    {
        if (MoveDataToFreeBlocks(VMA_SUBALLOCATION_TYPE_FREE, vector,
            vectorState.firstFreeBlock, texturePresent, bufferPresent, otherPresent))
        {
            if (otherPresent)
            {
                vectorState.operation = StateExtensive::Operation::FindFreeBlockBuffer;
                return ComputeDefragmentation_Extensive(vector, index);
            }
            // Everything moved
            vectorState.operation = StateExtensive::Operation::Cleanup;
        }
        break;
    }
    case StateExtensive::Operation::Cleanup:
        // Cleanup is handled below so that other operations may reuse the cleanup code. This case is here to prevent the unhandled enum value warning (C4062).
        break;
    }

    if (vectorState.operation == StateExtensive::Operation::Cleanup)
    {
        // All other work done, pack data in blocks even tighter if possible
        const size_t prevMoveCount = m_Moves.size();
        for (size_t i = 0; i < vector.GetBlockCount(); ++i)
        {
            if (ReallocWithinBlock(vector, vector.GetBlock(i)))
                return true;
        }

        if (prevMoveCount == m_Moves.size())
            vectorState.operation = StateExtensive::Operation::Done;
    }
    return false;
}

void VmaDefragmentationContext_T::UpdateVectorStatistics(VmaBlockVector& vector, StateBalanced& state)
{
    size_t allocCount = 0;
    size_t freeCount = 0;
    state.avgFreeSize = 0;
    state.avgAllocSize = 0;

    for (size_t i = 0; i < vector.GetBlockCount(); ++i)
    {
        VmaBlockMetadata* metadata = vector.GetBlock(i)->m_pMetadata;

        allocCount += metadata->GetAllocationCount();
        freeCount += metadata->GetFreeRegionsCount();
        state.avgFreeSize += metadata->GetSumFreeSize();
        state.avgAllocSize += metadata->GetSize();
    }

    state.avgAllocSize = (state.avgAllocSize - state.avgFreeSize) / allocCount;
    state.avgFreeSize /= freeCount;
}

bool VmaDefragmentationContext_T::MoveDataToFreeBlocks(VmaSuballocationType currentType,
    VmaBlockVector& vector, size_t firstFreeBlock,
    bool& texturePresent, bool& bufferPresent, bool& otherPresent)
{
    const size_t prevMoveCount = m_Moves.size();
    for (size_t i = firstFreeBlock ; i;)
    {
        VmaDeviceMemoryBlock* block = vector.GetBlock(--i);
        VmaBlockMetadata* metadata = block->m_pMetadata;

        for (VmaAllocHandle handle = metadata->GetAllocationListBegin();
            handle != VK_NULL_HANDLE;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.srcAllocation->GetUserData() == this)
                continue;
            switch (CheckCounters(moveData.move.srcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                VMA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Move only single type of resources at once
            if (!VmaIsBufferImageGranularityConflict(moveData.type, currentType))
            {
                // Try to fit allocation into free blocks
                if (AllocInOtherBlock(firstFreeBlock, vector.GetBlockCount(), moveData, vector))
                    return false;
            }

            if (!VmaIsBufferImageGranularityConflict(moveData.type, VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL))
                texturePresent = true;
            else if (!VmaIsBufferImageGranularityConflict(moveData.type, VMA_SUBALLOCATION_TYPE_BUFFER))
                bufferPresent = true;
            else
                otherPresent = true;
        }
    }
    return prevMoveCount == m_Moves.size();
}
#endif // _VMA_DEFRAGMENTATION_CONTEXT_FUNCTIONS

#ifndef _VMA_POOL_T_FUNCTIONS
VmaPool_T::VmaPool_T(
    VmaAllocator hAllocator,
    const VmaPoolCreateInfo& createInfo,
    VkDeviceSize preferredBlockSize)
    : m_BlockVector(
        hAllocator,
        this, // hParentPool
        createInfo.memoryTypeIndex,
        createInfo.blockSize != 0 ? createInfo.blockSize : preferredBlockSize,
        createInfo.minBlockCount,
        createInfo.maxBlockCount,
        (createInfo.flags& VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT) != 0 ? 1 : hAllocator->GetBufferImageGranularity(),
        createInfo.blockSize != 0, // explicitBlockSize
        createInfo.flags & VMA_POOL_CREATE_ALGORITHM_MASK, // algorithm
        createInfo.priority,
        VMA_MAX(hAllocator->GetMemoryTypeMinAlignment(createInfo.memoryTypeIndex), createInfo.minAllocationAlignment),
        createInfo.pMemoryAllocateNext),
    m_Id(0),
    m_Name(VMA_NULL) {}

VmaPool_T::~VmaPool_T()
{
    VMA_ASSERT(m_PrevPool == VMA_NULL && m_NextPool == VMA_NULL);
}

void VmaPool_T::SetName(const char* pName)
{
    const VkAllocationCallbacks* allocs = m_BlockVector.GetAllocator()->GetAllocationCallbacks();
    VmaFreeString(allocs, m_Name);

    if (pName != VMA_NULL)
    {
        m_Name = VmaCreateStringCopy(allocs, pName);
    }
    else
    {
        m_Name = VMA_NULL;
    }
}
#endif // _VMA_POOL_T_FUNCTIONS

#ifndef _VMA_ALLOCATOR_T_FUNCTIONS
VmaAllocator_T::VmaAllocator_T(const VmaAllocatorCreateInfo* pCreateInfo) :
    m_UseMutex((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT) == 0),
    m_VulkanApiVersion(pCreateInfo->vulkanApiVersion != 0 ? pCreateInfo->vulkanApiVersion : VK_API_VERSION_1_0),
    m_UseKhrDedicatedAllocation((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT) != 0),
    m_UseKhrBindMemory2((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT) != 0),
    m_UseExtMemoryBudget((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT) != 0),
    m_UseAmdDeviceCoherentMemory((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT) != 0),
    m_UseKhrBufferDeviceAddress((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT) != 0),
    m_UseExtMemoryPriority((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT) != 0),
    m_hDevice(pCreateInfo->device),
    m_hInstance(pCreateInfo->instance),
    m_AllocationCallbacksSpecified(pCreateInfo->pAllocationCallbacks != VMA_NULL),
    m_AllocationCallbacks(pCreateInfo->pAllocationCallbacks ?
        *pCreateInfo->pAllocationCallbacks : VmaEmptyAllocationCallbacks),
    m_AllocationObjectAllocator(&m_AllocationCallbacks),
    m_HeapSizeLimitMask(0),
    m_DeviceMemoryCount(0),
    m_PreferredLargeHeapBlockSize(0),
    m_PhysicalDevice(pCreateInfo->physicalDevice),
    m_GpuDefragmentationMemoryTypeBits(UINT32_MAX),
    m_NextPoolId(0),
    m_GlobalMemoryTypeBits(UINT32_MAX)
{
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        m_UseKhrDedicatedAllocation = false;
        m_UseKhrBindMemory2 = false;
    }

    if(VMA_DEBUG_DETECT_CORRUPTION)
    {
        // Needs to be multiply of uint32_t size because we are going to write VMA_CORRUPTION_DETECTION_MAGIC_VALUE to it.
        VMA_ASSERT(VMA_DEBUG_MARGIN % sizeof(uint32_t) == 0);
    }

    VMA_ASSERT(pCreateInfo->physicalDevice && pCreateInfo->device && pCreateInfo->instance);

    if(m_VulkanApiVersion < VK_MAKE_VERSION(1, 1, 0))
    {
#if !(VMA_DEDICATED_ALLOCATION)
        if((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT) != 0)
        {
            VMA_ASSERT(0 && "VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT set but required extensions are disabled by preprocessor macros.");
        }
#endif
#if !(VMA_BIND_MEMORY2)
        if((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT) != 0)
        {
            VMA_ASSERT(0 && "VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT set but required extension is disabled by preprocessor macros.");
        }
#endif
    }
#if !(VMA_MEMORY_BUDGET)
    if((pCreateInfo->flags & VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT) != 0)
    {
        VMA_ASSERT(0 && "VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT set but required extension is disabled by preprocessor macros.");
    }
#endif
#if !(VMA_BUFFER_DEVICE_ADDRESS)
    if(m_UseKhrBufferDeviceAddress)
    {
        VMA_ASSERT(0 && "VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT is set but required extension or Vulkan 1.2 is not available in your Vulkan header or its support in VMA has been disabled by a preprocessor macro.");
    }
#endif
#if VMA_VULKAN_VERSION < 1003000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
        VMA_ASSERT(0 && "vulkanApiVersion >= VK_API_VERSION_1_3 but required Vulkan version is disabled by preprocessor macros.");
    }
#endif
#if VMA_VULKAN_VERSION < 1002000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
        VMA_ASSERT(0 && "vulkanApiVersion >= VK_API_VERSION_1_2 but required Vulkan version is disabled by preprocessor macros.");
    }
#endif
#if VMA_VULKAN_VERSION < 1001000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VMA_ASSERT(0 && "vulkanApiVersion >= VK_API_VERSION_1_1 but required Vulkan version is disabled by preprocessor macros.");
    }
#endif
#if !(VMA_MEMORY_PRIORITY)
    if(m_UseExtMemoryPriority)
    {
        VMA_ASSERT(0 && "VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT is set but required extension is not available in your Vulkan header or its support in VMA has been disabled by a preprocessor macro.");
    }
#endif

    memset(&m_DeviceMemoryCallbacks, 0 ,sizeof(m_DeviceMemoryCallbacks));
    memset(&m_PhysicalDeviceProperties, 0, sizeof(m_PhysicalDeviceProperties));
    memset(&m_MemProps, 0, sizeof(m_MemProps));

    memset(&m_pBlockVectors, 0, sizeof(m_pBlockVectors));
    memset(&m_VulkanFunctions, 0, sizeof(m_VulkanFunctions));

#if VMA_EXTERNAL_MEMORY
    memset(&m_TypeExternalMemoryHandleTypes, 0, sizeof(m_TypeExternalMemoryHandleTypes));
#endif // #if VMA_EXTERNAL_MEMORY

    if(pCreateInfo->pDeviceMemoryCallbacks != VMA_NULL)
    {
        m_DeviceMemoryCallbacks.pUserData = pCreateInfo->pDeviceMemoryCallbacks->pUserData;
        m_DeviceMemoryCallbacks.pfnAllocate = pCreateInfo->pDeviceMemoryCallbacks->pfnAllocate;
        m_DeviceMemoryCallbacks.pfnFree = pCreateInfo->pDeviceMemoryCallbacks->pfnFree;
    }

    ImportVulkanFunctions(pCreateInfo->pVulkanFunctions);

    (*m_VulkanFunctions.vkGetPhysicalDeviceProperties)(m_PhysicalDevice, &m_PhysicalDeviceProperties);
    (*m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties)(m_PhysicalDevice, &m_MemProps);

    VMA_ASSERT(VmaIsPow2(VMA_MIN_ALIGNMENT));
    VMA_ASSERT(VmaIsPow2(VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY));
    VMA_ASSERT(VmaIsPow2(m_PhysicalDeviceProperties.limits.bufferImageGranularity));
    VMA_ASSERT(VmaIsPow2(m_PhysicalDeviceProperties.limits.nonCoherentAtomSize));

    m_PreferredLargeHeapBlockSize = (pCreateInfo->preferredLargeHeapBlockSize != 0) ?
        pCreateInfo->preferredLargeHeapBlockSize : static_cast<VkDeviceSize>(VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE);

    m_GlobalMemoryTypeBits = CalculateGlobalMemoryTypeBits();

#if VMA_EXTERNAL_MEMORY
    if(pCreateInfo->pTypeExternalMemoryHandleTypes != VMA_NULL)
    {
        memcpy(m_TypeExternalMemoryHandleTypes, pCreateInfo->pTypeExternalMemoryHandleTypes,
            sizeof(VkExternalMemoryHandleTypeFlagsKHR) * GetMemoryTypeCount());
    }
#endif // #if VMA_EXTERNAL_MEMORY

    if(pCreateInfo->pHeapSizeLimit != VMA_NULL)
    {
        for(uint32_t heapIndex = 0; heapIndex < GetMemoryHeapCount(); ++heapIndex)
        {
            const VkDeviceSize limit = pCreateInfo->pHeapSizeLimit[heapIndex];
            if(limit != VK_WHOLE_SIZE)
            {
                m_HeapSizeLimitMask |= 1u << heapIndex;
                if(limit < m_MemProps.memoryHeaps[heapIndex].size)
                {
                    m_MemProps.memoryHeaps[heapIndex].size = limit;
                }
            }
        }
    }

    for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
    {
        // Create only supported types
        if((m_GlobalMemoryTypeBits & (1u << memTypeIndex)) != 0)
        {
            const VkDeviceSize preferredBlockSize = CalcPreferredBlockSize(memTypeIndex);
            m_pBlockVectors[memTypeIndex] = vma_new(this, VmaBlockVector)(
                this,
                VK_NULL_HANDLE, // hParentPool
                memTypeIndex,
                preferredBlockSize,
                0,
                SIZE_MAX,
                GetBufferImageGranularity(),
                false, // explicitBlockSize
                0, // algorithm
                0.5f, // priority (0.5 is the default per Vulkan spec)
                GetMemoryTypeMinAlignment(memTypeIndex), // minAllocationAlignment
                VMA_NULL); // // pMemoryAllocateNext
            // No need to call m_pBlockVectors[memTypeIndex][blockVectorTypeIndex]->CreateMinBlocks here,
            // becase minBlockCount is 0.
        }
    }
}

VkResult VmaAllocator_T::Init(const VmaAllocatorCreateInfo* pCreateInfo)
{
    VkResult res = VK_SUCCESS;

#if VMA_MEMORY_BUDGET
    if(m_UseExtMemoryBudget)
    {
        UpdateVulkanBudget();
    }
#endif // #if VMA_MEMORY_BUDGET

    return res;
}

VmaAllocator_T::~VmaAllocator_T()
{
    VMA_ASSERT(m_Pools.IsEmpty());

    for(size_t memTypeIndex = GetMemoryTypeCount(); memTypeIndex--; )
    {
        vma_delete(this, m_pBlockVectors[memTypeIndex]);
    }
}

void VmaAllocator_T::ImportVulkanFunctions(const VmaVulkanFunctions* pVulkanFunctions)
{
#if VMA_STATIC_VULKAN_FUNCTIONS == 1
    ImportVulkanFunctions_Static();
#endif

    if(pVulkanFunctions != VMA_NULL)
    {
        ImportVulkanFunctions_Custom(pVulkanFunctions);
    }

#if VMA_DYNAMIC_VULKAN_FUNCTIONS == 1
    ImportVulkanFunctions_Dynamic();
#endif

    ValidateVulkanFunctions();
}

#if VMA_STATIC_VULKAN_FUNCTIONS == 1

void VmaAllocator_T::ImportVulkanFunctions_Static()
{
    // Vulkan 1.0
    m_VulkanFunctions.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr;
    m_VulkanFunctions.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetDeviceProcAddr;
    m_VulkanFunctions.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetPhysicalDeviceProperties;
    m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetPhysicalDeviceMemoryProperties;
    m_VulkanFunctions.vkAllocateMemory = (PFN_vkAllocateMemory)vkAllocateMemory;
    m_VulkanFunctions.vkFreeMemory = (PFN_vkFreeMemory)vkFreeMemory;
    m_VulkanFunctions.vkMapMemory = (PFN_vkMapMemory)vkMapMemory;
    m_VulkanFunctions.vkUnmapMemory = (PFN_vkUnmapMemory)vkUnmapMemory;
    m_VulkanFunctions.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkFlushMappedMemoryRanges;
    m_VulkanFunctions.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkInvalidateMappedMemoryRanges;
    m_VulkanFunctions.vkBindBufferMemory = (PFN_vkBindBufferMemory)vkBindBufferMemory;
    m_VulkanFunctions.vkBindImageMemory = (PFN_vkBindImageMemory)vkBindImageMemory;
    m_VulkanFunctions.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetBufferMemoryRequirements;
    m_VulkanFunctions.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkGetImageMemoryRequirements;
    m_VulkanFunctions.vkCreateBuffer = (PFN_vkCreateBuffer)vkCreateBuffer;
    m_VulkanFunctions.vkDestroyBuffer = (PFN_vkDestroyBuffer)vkDestroyBuffer;
    m_VulkanFunctions.vkCreateImage = (PFN_vkCreateImage)vkCreateImage;
    m_VulkanFunctions.vkDestroyImage = (PFN_vkDestroyImage)vkDestroyImage;
    m_VulkanFunctions.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkCmdCopyBuffer;

    // Vulkan 1.1
#if VMA_VULKAN_VERSION >= 1001000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        m_VulkanFunctions.vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2)vkGetBufferMemoryRequirements2;
        m_VulkanFunctions.vkGetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2)vkGetImageMemoryRequirements2;
        m_VulkanFunctions.vkBindBufferMemory2KHR = (PFN_vkBindBufferMemory2)vkBindBufferMemory2;
        m_VulkanFunctions.vkBindImageMemory2KHR = (PFN_vkBindImageMemory2)vkBindImageMemory2;
        m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2)vkGetPhysicalDeviceMemoryProperties2;
    }
#endif

#if VMA_VULKAN_VERSION >= 1003000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
        m_VulkanFunctions.vkGetDeviceBufferMemoryRequirements = (PFN_vkGetDeviceBufferMemoryRequirements)vkGetDeviceBufferMemoryRequirements;
        m_VulkanFunctions.vkGetDeviceImageMemoryRequirements = (PFN_vkGetDeviceImageMemoryRequirements)vkGetDeviceImageMemoryRequirements;
    }
#endif
}

#endif // VMA_STATIC_VULKAN_FUNCTIONS == 1

void VmaAllocator_T::ImportVulkanFunctions_Custom(const VmaVulkanFunctions* pVulkanFunctions)
{
    VMA_ASSERT(pVulkanFunctions != VMA_NULL);

#define VMA_COPY_IF_NOT_NULL(funcName) \
    if(pVulkanFunctions->funcName != VMA_NULL) m_VulkanFunctions.funcName = pVulkanFunctions->funcName;

    VMA_COPY_IF_NOT_NULL(vkGetInstanceProcAddr);
    VMA_COPY_IF_NOT_NULL(vkGetDeviceProcAddr);
    VMA_COPY_IF_NOT_NULL(vkGetPhysicalDeviceProperties);
    VMA_COPY_IF_NOT_NULL(vkGetPhysicalDeviceMemoryProperties);
    VMA_COPY_IF_NOT_NULL(vkAllocateMemory);
    VMA_COPY_IF_NOT_NULL(vkFreeMemory);
    VMA_COPY_IF_NOT_NULL(vkMapMemory);
    VMA_COPY_IF_NOT_NULL(vkUnmapMemory);
    VMA_COPY_IF_NOT_NULL(vkFlushMappedMemoryRanges);
    VMA_COPY_IF_NOT_NULL(vkInvalidateMappedMemoryRanges);
    VMA_COPY_IF_NOT_NULL(vkBindBufferMemory);
    VMA_COPY_IF_NOT_NULL(vkBindImageMemory);
    VMA_COPY_IF_NOT_NULL(vkGetBufferMemoryRequirements);
    VMA_COPY_IF_NOT_NULL(vkGetImageMemoryRequirements);
    VMA_COPY_IF_NOT_NULL(vkCreateBuffer);
    VMA_COPY_IF_NOT_NULL(vkDestroyBuffer);
    VMA_COPY_IF_NOT_NULL(vkCreateImage);
    VMA_COPY_IF_NOT_NULL(vkDestroyImage);
    VMA_COPY_IF_NOT_NULL(vkCmdCopyBuffer);

#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    VMA_COPY_IF_NOT_NULL(vkGetBufferMemoryRequirements2KHR);
    VMA_COPY_IF_NOT_NULL(vkGetImageMemoryRequirements2KHR);
#endif

#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
    VMA_COPY_IF_NOT_NULL(vkBindBufferMemory2KHR);
    VMA_COPY_IF_NOT_NULL(vkBindImageMemory2KHR);
#endif

#if VMA_MEMORY_BUDGET
    VMA_COPY_IF_NOT_NULL(vkGetPhysicalDeviceMemoryProperties2KHR);
#endif

#if VMA_VULKAN_VERSION >= 1003000
    VMA_COPY_IF_NOT_NULL(vkGetDeviceBufferMemoryRequirements);
    VMA_COPY_IF_NOT_NULL(vkGetDeviceImageMemoryRequirements);
#endif

#undef VMA_COPY_IF_NOT_NULL
}

#if VMA_DYNAMIC_VULKAN_FUNCTIONS == 1

void VmaAllocator_T::ImportVulkanFunctions_Dynamic()
{
    VMA_ASSERT(m_VulkanFunctions.vkGetInstanceProcAddr && m_VulkanFunctions.vkGetDeviceProcAddr &&
        "To use VMA_DYNAMIC_VULKAN_FUNCTIONS in new versions of VMA you now have to pass "
        "VmaVulkanFunctions::vkGetInstanceProcAddr and vkGetDeviceProcAddr as VmaAllocatorCreateInfo::pVulkanFunctions. "
        "Other members can be null.");

#define VMA_FETCH_INSTANCE_FUNC(memberName, functionPointerType, functionNameString) \
    if(m_VulkanFunctions.memberName == VMA_NULL) \
        m_VulkanFunctions.memberName = \
            (functionPointerType)m_VulkanFunctions.vkGetInstanceProcAddr(m_hInstance, functionNameString);
#define VMA_FETCH_DEVICE_FUNC(memberName, functionPointerType, functionNameString) \
    if(m_VulkanFunctions.memberName == VMA_NULL) \
        m_VulkanFunctions.memberName = \
            (functionPointerType)m_VulkanFunctions.vkGetDeviceProcAddr(m_hDevice, functionNameString);

    VMA_FETCH_INSTANCE_FUNC(vkGetPhysicalDeviceProperties, PFN_vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
    VMA_FETCH_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties, PFN_vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
    VMA_FETCH_DEVICE_FUNC(vkAllocateMemory, PFN_vkAllocateMemory, "vkAllocateMemory");
    VMA_FETCH_DEVICE_FUNC(vkFreeMemory, PFN_vkFreeMemory, "vkFreeMemory");
    VMA_FETCH_DEVICE_FUNC(vkMapMemory, PFN_vkMapMemory, "vkMapMemory");
    VMA_FETCH_DEVICE_FUNC(vkUnmapMemory, PFN_vkUnmapMemory, "vkUnmapMemory");
    VMA_FETCH_DEVICE_FUNC(vkFlushMappedMemoryRanges, PFN_vkFlushMappedMemoryRanges, "vkFlushMappedMemoryRanges");
    VMA_FETCH_DEVICE_FUNC(vkInvalidateMappedMemoryRanges, PFN_vkInvalidateMappedMemoryRanges, "vkInvalidateMappedMemoryRanges");
    VMA_FETCH_DEVICE_FUNC(vkBindBufferMemory, PFN_vkBindBufferMemory, "vkBindBufferMemory");
    VMA_FETCH_DEVICE_FUNC(vkBindImageMemory, PFN_vkBindImageMemory, "vkBindImageMemory");
    VMA_FETCH_DEVICE_FUNC(vkGetBufferMemoryRequirements, PFN_vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
    VMA_FETCH_DEVICE_FUNC(vkGetImageMemoryRequirements, PFN_vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
    VMA_FETCH_DEVICE_FUNC(vkCreateBuffer, PFN_vkCreateBuffer, "vkCreateBuffer");
    VMA_FETCH_DEVICE_FUNC(vkDestroyBuffer, PFN_vkDestroyBuffer, "vkDestroyBuffer");
    VMA_FETCH_DEVICE_FUNC(vkCreateImage, PFN_vkCreateImage, "vkCreateImage");
    VMA_FETCH_DEVICE_FUNC(vkDestroyImage, PFN_vkDestroyImage, "vkDestroyImage");
    VMA_FETCH_DEVICE_FUNC(vkCmdCopyBuffer, PFN_vkCmdCopyBuffer, "vkCmdCopyBuffer");

#if VMA_VULKAN_VERSION >= 1001000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VMA_FETCH_DEVICE_FUNC(vkGetBufferMemoryRequirements2KHR, PFN_vkGetBufferMemoryRequirements2, "vkGetBufferMemoryRequirements2");
        VMA_FETCH_DEVICE_FUNC(vkGetImageMemoryRequirements2KHR, PFN_vkGetImageMemoryRequirements2, "vkGetImageMemoryRequirements2");
        VMA_FETCH_DEVICE_FUNC(vkBindBufferMemory2KHR, PFN_vkBindBufferMemory2, "vkBindBufferMemory2");
        VMA_FETCH_DEVICE_FUNC(vkBindImageMemory2KHR, PFN_vkBindImageMemory2, "vkBindImageMemory2");
        VMA_FETCH_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties2KHR, PFN_vkGetPhysicalDeviceMemoryProperties2, "vkGetPhysicalDeviceMemoryProperties2");
    }
#endif

#if VMA_DEDICATED_ALLOCATION
    if(m_UseKhrDedicatedAllocation)
    {
        VMA_FETCH_DEVICE_FUNC(vkGetBufferMemoryRequirements2KHR, PFN_vkGetBufferMemoryRequirements2KHR, "vkGetBufferMemoryRequirements2KHR");
        VMA_FETCH_DEVICE_FUNC(vkGetImageMemoryRequirements2KHR, PFN_vkGetImageMemoryRequirements2KHR, "vkGetImageMemoryRequirements2KHR");
    }
#endif

#if VMA_BIND_MEMORY2
    if(m_UseKhrBindMemory2)
    {
        VMA_FETCH_DEVICE_FUNC(vkBindBufferMemory2KHR, PFN_vkBindBufferMemory2KHR, "vkBindBufferMemory2KHR");
        VMA_FETCH_DEVICE_FUNC(vkBindImageMemory2KHR, PFN_vkBindImageMemory2KHR, "vkBindImageMemory2KHR");
    }
#endif // #if VMA_BIND_MEMORY2

#if VMA_MEMORY_BUDGET
    if(m_UseExtMemoryBudget)
    {
        VMA_FETCH_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties2KHR, PFN_vkGetPhysicalDeviceMemoryProperties2KHR, "vkGetPhysicalDeviceMemoryProperties2KHR");
    }
#endif // #if VMA_MEMORY_BUDGET

#if VMA_VULKAN_VERSION >= 1003000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
        VMA_FETCH_DEVICE_FUNC(vkGetDeviceBufferMemoryRequirements, PFN_vkGetDeviceBufferMemoryRequirements, "vkGetDeviceBufferMemoryRequirements");
        VMA_FETCH_DEVICE_FUNC(vkGetDeviceImageMemoryRequirements, PFN_vkGetDeviceImageMemoryRequirements, "vkGetDeviceImageMemoryRequirements");
    }
#endif

#undef VMA_FETCH_DEVICE_FUNC
#undef VMA_FETCH_INSTANCE_FUNC
}

#endif // VMA_DYNAMIC_VULKAN_FUNCTIONS == 1

void VmaAllocator_T::ValidateVulkanFunctions()
{
    VMA_ASSERT(m_VulkanFunctions.vkGetPhysicalDeviceProperties != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkAllocateMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkFreeMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkMapMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkUnmapMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkFlushMappedMemoryRanges != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkInvalidateMappedMemoryRanges != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkBindBufferMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkBindImageMemory != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkGetBufferMemoryRequirements != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkGetImageMemoryRequirements != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkCreateBuffer != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkDestroyBuffer != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkCreateImage != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkDestroyImage != VMA_NULL);
    VMA_ASSERT(m_VulkanFunctions.vkCmdCopyBuffer != VMA_NULL);

#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0) || m_UseKhrDedicatedAllocation)
    {
        VMA_ASSERT(m_VulkanFunctions.vkGetBufferMemoryRequirements2KHR != VMA_NULL);
        VMA_ASSERT(m_VulkanFunctions.vkGetImageMemoryRequirements2KHR != VMA_NULL);
    }
#endif

#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0) || m_UseKhrBindMemory2)
    {
        VMA_ASSERT(m_VulkanFunctions.vkBindBufferMemory2KHR != VMA_NULL);
        VMA_ASSERT(m_VulkanFunctions.vkBindImageMemory2KHR != VMA_NULL);
    }
#endif

#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
    if(m_UseExtMemoryBudget || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VMA_ASSERT(m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR != VMA_NULL);
    }
#endif

#if VMA_VULKAN_VERSION >= 1003000
    if(m_VulkanApiVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
        VMA_ASSERT(m_VulkanFunctions.vkGetDeviceBufferMemoryRequirements != VMA_NULL);
        VMA_ASSERT(m_VulkanFunctions.vkGetDeviceImageMemoryRequirements != VMA_NULL);
    }
#endif
}

VkDeviceSize VmaAllocator_T::CalcPreferredBlockSize(uint32_t memTypeIndex)
{
    const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memTypeIndex);
    const VkDeviceSize heapSize = m_MemProps.memoryHeaps[heapIndex].size;
    const bool isSmallHeap = heapSize <= VMA_SMALL_HEAP_MAX_SIZE;
    return VmaAlignUp(isSmallHeap ? (heapSize / 8) : m_PreferredLargeHeapBlockSize, (VkDeviceSize)32);
}

VkResult VmaAllocator_T::AllocateMemoryOfType(
    VmaPool pool,
    VkDeviceSize size,
    VkDeviceSize alignment,
    bool dedicatedPreferred,
    VkBuffer dedicatedBuffer,
    VkImage dedicatedImage,
    VkFlags dedicatedBufferImageUsage,
    const VmaAllocationCreateInfo& createInfo,
    uint32_t memTypeIndex,
    VmaSuballocationType suballocType,
    VmaDedicatedAllocationList& dedicatedAllocations,
    VmaBlockVector& blockVector,
    size_t allocationCount,
    VmaAllocation* pAllocations)
{
    VMA_ASSERT(pAllocations != VMA_NULL);
    VMA_DEBUG_LOG("  AllocateMemory: MemoryTypeIndex=%u, AllocationCount=%zu, Size=%llu", memTypeIndex, allocationCount, size);

    VmaAllocationCreateInfo finalCreateInfo = createInfo;
    VkResult res = CalcMemTypeParams(
        finalCreateInfo,
        memTypeIndex,
        size,
        allocationCount);
    if(res != VK_SUCCESS)
        return res;

    if((finalCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0)
    {
        return AllocateDedicatedMemory(
            pool,
            size,
            suballocType,
            dedicatedAllocations,
            memTypeIndex,
            (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
            (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT) != 0,
            (finalCreateInfo.flags &
                (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
            (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
            finalCreateInfo.pUserData,
            finalCreateInfo.priority,
            dedicatedBuffer,
            dedicatedImage,
            dedicatedBufferImageUsage,
            allocationCount,
            pAllocations,
            blockVector.GetAllocationNextPtr());
    }
    else
    {
        const bool canAllocateDedicated =
            (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0 &&
            (pool == VK_NULL_HANDLE || !blockVector.HasExplicitBlockSize());

        if(canAllocateDedicated)
        {
            // Heuristics: Allocate dedicated memory if requested size if greater than half of preferred block size.
            if(size > blockVector.GetPreferredBlockSize() / 2)
            {
                dedicatedPreferred = true;
            }
            // Protection against creating each allocation as dedicated when we reach or exceed heap size/budget,
            // which can quickly deplete maxMemoryAllocationCount: Don't prefer dedicated allocations when above
            // 3/4 of the maximum allocation count.
            if(m_DeviceMemoryCount.load() > m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount * 3 / 4)
            {
                dedicatedPreferred = false;
            }

            if(dedicatedPreferred)
            {
                res = AllocateDedicatedMemory(
                    pool,
                    size,
                    suballocType,
                    dedicatedAllocations,
                    memTypeIndex,
                    (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
                    (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT) != 0,
                    (finalCreateInfo.flags &
                        (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
                    (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
                    finalCreateInfo.pUserData,
                    finalCreateInfo.priority,
                    dedicatedBuffer,
                    dedicatedImage,
                    dedicatedBufferImageUsage,
                    allocationCount,
                    pAllocations,
                    blockVector.GetAllocationNextPtr());
                if(res == VK_SUCCESS)
                {
                    // Succeeded: AllocateDedicatedMemory function already filld pMemory, nothing more to do here.
                    VMA_DEBUG_LOG("    Allocated as DedicatedMemory");
                    return VK_SUCCESS;
                }
            }
        }

        res = blockVector.Allocate(
            size,
            alignment,
            finalCreateInfo,
            suballocType,
            allocationCount,
            pAllocations);
        if(res == VK_SUCCESS)
            return VK_SUCCESS;

        // Try dedicated memory.
        if(canAllocateDedicated && !dedicatedPreferred)
        {
            res = AllocateDedicatedMemory(
                pool,
                size,
                suballocType,
                dedicatedAllocations,
                memTypeIndex,
                (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
                (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT) != 0,
                (finalCreateInfo.flags &
                    (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
                (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
                finalCreateInfo.pUserData,
                finalCreateInfo.priority,
                dedicatedBuffer,
                dedicatedImage,
                dedicatedBufferImageUsage,
                allocationCount,
                pAllocations,
                blockVector.GetAllocationNextPtr());
            if(res == VK_SUCCESS)
            {
                // Succeeded: AllocateDedicatedMemory function already filld pMemory, nothing more to do here.
                VMA_DEBUG_LOG("    Allocated as DedicatedMemory");
                return VK_SUCCESS;
            }
        }
        // Everything failed: Return error code.
        VMA_DEBUG_LOG("    vkAllocateMemory FAILED");
        return res;
    }
}

VkResult VmaAllocator_T::AllocateDedicatedMemory(
    VmaPool pool,
    VkDeviceSize size,
    VmaSuballocationType suballocType,
    VmaDedicatedAllocationList& dedicatedAllocations,
    uint32_t memTypeIndex,
    bool map,
    bool isUserDataString,
    bool isMappingAllowed,
    bool canAliasMemory,
    void* pUserData,
    float priority,
    VkBuffer dedicatedBuffer,
    VkImage dedicatedImage,
    VkFlags dedicatedBufferImageUsage,
    size_t allocationCount,
    VmaAllocation* pAllocations,
    const void* pNextChain)
{
    VMA_ASSERT(allocationCount > 0 && pAllocations);

    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.memoryTypeIndex = memTypeIndex;
    allocInfo.allocationSize = size;
    allocInfo.pNext = pNextChain;

#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR };
    if(!canAliasMemory)
    {
        if(m_UseKhrDedicatedAllocation || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
        {
            if(dedicatedBuffer != VK_NULL_HANDLE)
            {
                VMA_ASSERT(dedicatedImage == VK_NULL_HANDLE);
                dedicatedAllocInfo.buffer = dedicatedBuffer;
                VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
            }
            else if(dedicatedImage != VK_NULL_HANDLE)
            {
                dedicatedAllocInfo.image = dedicatedImage;
                VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
            }
        }
    }
#endif // #if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000

#if VMA_BUFFER_DEVICE_ADDRESS
    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
    if(m_UseKhrBufferDeviceAddress)
    {
        bool canContainBufferWithDeviceAddress = true;
        if(dedicatedBuffer != VK_NULL_HANDLE)
        {
            canContainBufferWithDeviceAddress = dedicatedBufferImageUsage == UINT32_MAX || // Usage flags unknown
                (dedicatedBufferImageUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT) != 0;
        }
        else if(dedicatedImage != VK_NULL_HANDLE)
        {
            canContainBufferWithDeviceAddress = false;
        }
        if(canContainBufferWithDeviceAddress)
        {
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            VmaPnextChainPushFront(&allocInfo, &allocFlagsInfo);
        }
    }
#endif // #if VMA_BUFFER_DEVICE_ADDRESS

#if VMA_MEMORY_PRIORITY
    VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
    if(m_UseExtMemoryPriority)
    {
        VMA_ASSERT(priority >= 0.f && priority <= 1.f);
        priorityInfo.priority = priority;
        VmaPnextChainPushFront(&allocInfo, &priorityInfo);
    }
#endif // #if VMA_MEMORY_PRIORITY

#if VMA_EXTERNAL_MEMORY
    // Attach VkExportMemoryAllocateInfoKHR if necessary.
    VkExportMemoryAllocateInfoKHR exportMemoryAllocInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR };
    exportMemoryAllocInfo.handleTypes = GetExternalMemoryHandleTypeFlags(memTypeIndex);
    if(exportMemoryAllocInfo.handleTypes != 0)
    {
        VmaPnextChainPushFront(&allocInfo, &exportMemoryAllocInfo);
    }
#endif // #if VMA_EXTERNAL_MEMORY

    size_t allocIndex;
    VkResult res = VK_SUCCESS;
    for(allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
    {
        res = AllocateDedicatedMemoryPage(
            pool,
            size,
            suballocType,
            memTypeIndex,
            allocInfo,
            map,
            isUserDataString,
            isMappingAllowed,
            pUserData,
            pAllocations + allocIndex);
        if(res != VK_SUCCESS)
        {
            break;
        }
    }

    if(res == VK_SUCCESS)
    {
        for (allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
        {
            dedicatedAllocations.Register(pAllocations[allocIndex]);
        }
        VMA_DEBUG_LOG("    Allocated DedicatedMemory Count=%zu, MemoryTypeIndex=#%u", allocationCount, memTypeIndex);
    }
    else
    {
        // Free all already created allocations.
        while(allocIndex--)
        {
            VmaAllocation currAlloc = pAllocations[allocIndex];
            VkDeviceMemory hMemory = currAlloc->GetMemory();

            /*
            There is no need to call this, because Vulkan spec allows to skip vkUnmapMemory
            before vkFreeMemory.

            if(currAlloc->GetMappedData() != VMA_NULL)
            {
                (*m_VulkanFunctions.vkUnmapMemory)(m_hDevice, hMemory);
            }
            */

            FreeVulkanMemory(memTypeIndex, currAlloc->GetSize(), hMemory);
            m_Budget.RemoveAllocation(MemoryTypeIndexToHeapIndex(memTypeIndex), currAlloc->GetSize());
            m_AllocationObjectAllocator.Free(currAlloc);
        }

        memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);
    }

    return res;
}

VkResult VmaAllocator_T::AllocateDedicatedMemoryPage(
    VmaPool pool,
    VkDeviceSize size,
    VmaSuballocationType suballocType,
    uint32_t memTypeIndex,
    const VkMemoryAllocateInfo& allocInfo,
    bool map,
    bool isUserDataString,
    bool isMappingAllowed,
    void* pUserData,
    VmaAllocation* pAllocation)
{
    VkDeviceMemory hMemory = VK_NULL_HANDLE;
    VkResult res = AllocateVulkanMemory(&allocInfo, &hMemory);
    if(res < 0)
    {
        VMA_DEBUG_LOG("    vkAllocateMemory FAILED");
        return res;
    }

    void* pMappedData = VMA_NULL;
    if(map)
    {
        res = (*m_VulkanFunctions.vkMapMemory)(
            m_hDevice,
            hMemory,
            0,
            VK_WHOLE_SIZE,
            0,
            &pMappedData);
        if(res < 0)
        {
            VMA_DEBUG_LOG("    vkMapMemory FAILED");
            FreeVulkanMemory(memTypeIndex, size, hMemory);
            return res;
        }
    }

    *pAllocation = m_AllocationObjectAllocator.Allocate(isMappingAllowed);
    (*pAllocation)->InitDedicatedAllocation(pool, memTypeIndex, hMemory, suballocType, pMappedData, size);
    if (isUserDataString)
        (*pAllocation)->SetName(this, (const char*)pUserData);
    else
        (*pAllocation)->SetUserData(this, pUserData);
    m_Budget.AddAllocation(MemoryTypeIndexToHeapIndex(memTypeIndex), size);
    if(VMA_DEBUG_INITIALIZE_ALLOCATIONS)
    {
        FillAllocation(*pAllocation, VMA_ALLOCATION_FILL_PATTERN_CREATED);
    }

    return VK_SUCCESS;
}

void VmaAllocator_T::GetBufferMemoryRequirements(
    VkBuffer hBuffer,
    VkMemoryRequirements& memReq,
    bool& requiresDedicatedAllocation,
    bool& prefersDedicatedAllocation) const
{
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    if(m_UseKhrDedicatedAllocation || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VkBufferMemoryRequirementsInfo2KHR memReqInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR };
        memReqInfo.buffer = hBuffer;

        VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };

        VkMemoryRequirements2KHR memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VmaPnextChainPushFront(&memReq2, &memDedicatedReq);

        (*m_VulkanFunctions.vkGetBufferMemoryRequirements2KHR)(m_hDevice, &memReqInfo, &memReq2);

        memReq = memReq2.memoryRequirements;
        requiresDedicatedAllocation = (memDedicatedReq.requiresDedicatedAllocation != VK_FALSE);
        prefersDedicatedAllocation  = (memDedicatedReq.prefersDedicatedAllocation  != VK_FALSE);
    }
    else
#endif // #if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    {
        (*m_VulkanFunctions.vkGetBufferMemoryRequirements)(m_hDevice, hBuffer, &memReq);
        requiresDedicatedAllocation = false;
        prefersDedicatedAllocation  = false;
    }
}

void VmaAllocator_T::GetImageMemoryRequirements(
    VkImage hImage,
    VkMemoryRequirements& memReq,
    bool& requiresDedicatedAllocation,
    bool& prefersDedicatedAllocation) const
{
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    if(m_UseKhrDedicatedAllocation || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VkImageMemoryRequirementsInfo2KHR memReqInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR };
        memReqInfo.image = hImage;

        VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };

        VkMemoryRequirements2KHR memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VmaPnextChainPushFront(&memReq2, &memDedicatedReq);

        (*m_VulkanFunctions.vkGetImageMemoryRequirements2KHR)(m_hDevice, &memReqInfo, &memReq2);

        memReq = memReq2.memoryRequirements;
        requiresDedicatedAllocation = (memDedicatedReq.requiresDedicatedAllocation != VK_FALSE);
        prefersDedicatedAllocation  = (memDedicatedReq.prefersDedicatedAllocation  != VK_FALSE);
    }
    else
#endif // #if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    {
        (*m_VulkanFunctions.vkGetImageMemoryRequirements)(m_hDevice, hImage, &memReq);
        requiresDedicatedAllocation = false;
        prefersDedicatedAllocation  = false;
    }
}

VkResult VmaAllocator_T::FindMemoryTypeIndex(
    uint32_t memoryTypeBits,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkFlags bufImgUsage,
    uint32_t* pMemoryTypeIndex) const
{
    memoryTypeBits &= GetGlobalMemoryTypeBits();

    if(pAllocationCreateInfo->memoryTypeBits != 0)
    {
        memoryTypeBits &= pAllocationCreateInfo->memoryTypeBits;
    }

    VkMemoryPropertyFlags requiredFlags = 0, preferredFlags = 0, notPreferredFlags = 0;
    if(!FindMemoryPreferences(
        IsIntegratedGpu(),
        *pAllocationCreateInfo,
        bufImgUsage,
        requiredFlags, preferredFlags, notPreferredFlags))
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    *pMemoryTypeIndex = UINT32_MAX;
    uint32_t minCost = UINT32_MAX;
    for(uint32_t memTypeIndex = 0, memTypeBit = 1;
        memTypeIndex < GetMemoryTypeCount();
        ++memTypeIndex, memTypeBit <<= 1)
    {
        // This memory type is acceptable according to memoryTypeBits bitmask.
        if((memTypeBit & memoryTypeBits) != 0)
        {
            const VkMemoryPropertyFlags currFlags =
                m_MemProps.memoryTypes[memTypeIndex].propertyFlags;
            // This memory type contains requiredFlags.
            if((requiredFlags & ~currFlags) == 0)
            {
                // Calculate cost as number of bits from preferredFlags not present in this memory type.
                uint32_t currCost = VMA_COUNT_BITS_SET(preferredFlags & ~currFlags) +
                    VMA_COUNT_BITS_SET(currFlags & notPreferredFlags);
                // Remember memory type with lowest cost.
                if(currCost < minCost)
                {
                    *pMemoryTypeIndex = memTypeIndex;
                    if(currCost == 0)
                    {
                        return VK_SUCCESS;
                    }
                    minCost = currCost;
                }
            }
        }
    }
    return (*pMemoryTypeIndex != UINT32_MAX) ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult VmaAllocator_T::CalcMemTypeParams(
    VmaAllocationCreateInfo& inoutCreateInfo,
    uint32_t memTypeIndex,
    VkDeviceSize size,
    size_t allocationCount)
{
    // If memory type is not HOST_VISIBLE, disable MAPPED.
    if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0 &&
        (m_MemProps.memoryTypes[memTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
    {
        inoutCreateInfo.flags &= ~VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
        (inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT) != 0)
    {
        const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memTypeIndex);
        VmaBudget heapBudget = {};
        GetHeapBudgets(&heapBudget, heapIndex, 1);
        if(heapBudget.usage + size * allocationCount > heapBudget.budget)
        {
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }
    return VK_SUCCESS;
}

VkResult VmaAllocator_T::CalcAllocationParams(
    VmaAllocationCreateInfo& inoutCreateInfo,
    bool dedicatedRequired,
    bool dedicatedPreferred)
{
    VMA_ASSERT((inoutCreateInfo.flags &
        (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) !=
        (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) &&
        "Specifying both flags VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT and VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT is incorrect.");
    VMA_ASSERT((((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT) == 0 ||
        (inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0)) &&
        "Specifying VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT requires also VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.");
    if(inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO || inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE || inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
    {
        if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0)
        {
            VMA_ASSERT((inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0 &&
                "When using VMA_ALLOCATION_CREATE_MAPPED_BIT and usage = VMA_MEMORY_USAGE_AUTO*, you must also specify VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.");
        }
    }

    // If memory is lazily allocated, it should be always dedicated.
    if(dedicatedRequired ||
        inoutCreateInfo.usage == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED)
    {
        inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if(inoutCreateInfo.pool != VK_NULL_HANDLE)
    {
        if(inoutCreateInfo.pool->m_BlockVector.HasExplicitBlockSize() &&
            (inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0)
        {
            VMA_ASSERT(0 && "Specifying VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT while current custom pool doesn't support dedicated allocations.");
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        inoutCreateInfo.priority = inoutCreateInfo.pool->m_BlockVector.GetPriority();
    }

    if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
        (inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) != 0)
    {
        VMA_ASSERT(0 && "Specifying VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT together with VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT makes no sense.");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    if(VMA_DEBUG_ALWAYS_DEDICATED_MEMORY &&
        (inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) != 0)
    {
        inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    // Non-auto USAGE values imply HOST_ACCESS flags.
    // And so does VMA_MEMORY_USAGE_UNKNOWN because it is used with custom pools.
    // Which specific flag is used doesn't matter. They change things only when used with VMA_MEMORY_USAGE_AUTO*.
    // Otherwise they just protect from assert on mapping.
    if(inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO &&
        inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE &&
        inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
    {
        if((inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) == 0)
        {
            inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }
    }

    return VK_SUCCESS;
}

VkResult VmaAllocator_T::AllocateMemory(
    const VkMemoryRequirements& vkMemReq,
    bool requiresDedicatedAllocation,
    bool prefersDedicatedAllocation,
    VkBuffer dedicatedBuffer,
    VkImage dedicatedImage,
    VkFlags dedicatedBufferImageUsage,
    const VmaAllocationCreateInfo& createInfo,
    VmaSuballocationType suballocType,
    size_t allocationCount,
    VmaAllocation* pAllocations)
{
    memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);

    VMA_ASSERT(VmaIsPow2(vkMemReq.alignment));

    if(vkMemReq.size == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VmaAllocationCreateInfo createInfoFinal = createInfo;
    VkResult res = CalcAllocationParams(createInfoFinal, requiresDedicatedAllocation, prefersDedicatedAllocation);
    if(res != VK_SUCCESS)
        return res;

    if(createInfoFinal.pool != VK_NULL_HANDLE)
    {
        VmaBlockVector& blockVector = createInfoFinal.pool->m_BlockVector;
        return AllocateMemoryOfType(
            createInfoFinal.pool,
            vkMemReq.size,
            vkMemReq.alignment,
            prefersDedicatedAllocation,
            dedicatedBuffer,
            dedicatedImage,
            dedicatedBufferImageUsage,
            createInfoFinal,
            blockVector.GetMemoryTypeIndex(),
            suballocType,
            createInfoFinal.pool->m_DedicatedAllocations,
            blockVector,
            allocationCount,
            pAllocations);
    }
    else
    {
        // Bit mask of memory Vulkan types acceptable for this allocation.
        uint32_t memoryTypeBits = vkMemReq.memoryTypeBits;
        uint32_t memTypeIndex = UINT32_MAX;
        res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
        // Can't find any single memory type matching requirements. res is VK_ERROR_FEATURE_NOT_PRESENT.
        if(res != VK_SUCCESS)
            return res;
        do
        {
            VmaBlockVector* blockVector = m_pBlockVectors[memTypeIndex];
            VMA_ASSERT(blockVector && "Trying to use unsupported memory type!");
            res = AllocateMemoryOfType(
                VK_NULL_HANDLE,
                vkMemReq.size,
                vkMemReq.alignment,
                requiresDedicatedAllocation || prefersDedicatedAllocation,
                dedicatedBuffer,
                dedicatedImage,
                dedicatedBufferImageUsage,
                createInfoFinal,
                memTypeIndex,
                suballocType,
                m_DedicatedAllocations[memTypeIndex],
                *blockVector,
                allocationCount,
                pAllocations);
            // Allocation succeeded
            if(res == VK_SUCCESS)
                return VK_SUCCESS;

            // Remove old memTypeIndex from list of possibilities.
            memoryTypeBits &= ~(1u << memTypeIndex);
            // Find alternative memTypeIndex.
            res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
        } while(res == VK_SUCCESS);

        // No other matching memory type index could be found.
        // Not returning res, which is VK_ERROR_FEATURE_NOT_PRESENT, because we already failed to allocate once.
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
}

void VmaAllocator_T::FreeMemory(
    size_t allocationCount,
    const VmaAllocation* pAllocations)
{
    VMA_ASSERT(pAllocations);

    for(size_t allocIndex = allocationCount; allocIndex--; )
    {
        VmaAllocation allocation = pAllocations[allocIndex];

        if(allocation != VK_NULL_HANDLE)
        {
            if(VMA_DEBUG_INITIALIZE_ALLOCATIONS)
            {
                FillAllocation(allocation, VMA_ALLOCATION_FILL_PATTERN_DESTROYED);
            }

            allocation->FreeName(this);

            switch(allocation->GetType())
            {
            case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
                {
                    VmaBlockVector* pBlockVector = VMA_NULL;
                    VmaPool hPool = allocation->GetParentPool();
                    if(hPool != VK_NULL_HANDLE)
                    {
                        pBlockVector = &hPool->m_BlockVector;
                    }
                    else
                    {
                        const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
                        pBlockVector = m_pBlockVectors[memTypeIndex];
                        VMA_ASSERT(pBlockVector && "Trying to free memory of unsupported type!");
                    }
                    pBlockVector->Free(allocation);
                }
                break;
            case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
                FreeDedicatedMemory(allocation);
                break;
            default:
                VMA_ASSERT(0);
            }
        }
    }
}

void VmaAllocator_T::CalculateStatistics(VmaTotalStatistics* pStats)
{
    // Initialize.
    VmaClearDetailedStatistics(pStats->total);
    for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; ++i)
        VmaClearDetailedStatistics(pStats->memoryType[i]);
    for(uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i)
        VmaClearDetailedStatistics(pStats->memoryHeap[i]);

    // Process default pools.
    for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
    {
        VmaBlockVector* const pBlockVector = m_pBlockVectors[memTypeIndex];
        if (pBlockVector != VMA_NULL)
            pBlockVector->AddDetailedStatistics(pStats->memoryType[memTypeIndex]);
    }

    // Process custom pools.
    {
        VmaMutexLockRead lock(m_PoolsMutex, m_UseMutex);
        for(VmaPool pool = m_Pools.Front(); pool != VMA_NULL; pool = m_Pools.GetNext(pool))
        {
            VmaBlockVector& blockVector = pool->m_BlockVector;
            const uint32_t memTypeIndex = blockVector.GetMemoryTypeIndex();
            blockVector.AddDetailedStatistics(pStats->memoryType[memTypeIndex]);
            pool->m_DedicatedAllocations.AddDetailedStatistics(pStats->memoryType[memTypeIndex]);
        }
    }

    // Process dedicated allocations.
    for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
    {
        m_DedicatedAllocations[memTypeIndex].AddDetailedStatistics(pStats->memoryType[memTypeIndex]);
    }

    // Sum from memory types to memory heaps.
    for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
    {
        const uint32_t memHeapIndex = m_MemProps.memoryTypes[memTypeIndex].heapIndex;
        VmaAddDetailedStatistics(pStats->memoryHeap[memHeapIndex], pStats->memoryType[memTypeIndex]);
    }

    // Sum from memory heaps to total.
    for(uint32_t memHeapIndex = 0; memHeapIndex < GetMemoryHeapCount(); ++memHeapIndex)
        VmaAddDetailedStatistics(pStats->total, pStats->memoryHeap[memHeapIndex]);

    VMA_ASSERT(pStats->total.statistics.allocationCount == 0 ||
        pStats->total.allocationSizeMax >= pStats->total.allocationSizeMin);
    VMA_ASSERT(pStats->total.unusedRangeCount == 0 ||
        pStats->total.unusedRangeSizeMax >= pStats->total.unusedRangeSizeMin);
}

void VmaAllocator_T::GetHeapBudgets(VmaBudget* outBudgets, uint32_t firstHeap, uint32_t heapCount)
{
#if VMA_MEMORY_BUDGET
    if(m_UseExtMemoryBudget)
    {
        if(m_Budget.m_OperationsSinceBudgetFetch < 30)
        {
            VmaMutexLockRead lockRead(m_Budget.m_BudgetMutex, m_UseMutex);
            for(uint32_t i = 0; i < heapCount; ++i, ++outBudgets)
            {
                const uint32_t heapIndex = firstHeap + i;

                outBudgets->statistics.blockCount = m_Budget.m_BlockCount[heapIndex];
                outBudgets->statistics.allocationCount = m_Budget.m_AllocationCount[heapIndex];
                outBudgets->statistics.blockBytes = m_Budget.m_BlockBytes[heapIndex];
                outBudgets->statistics.allocationBytes = m_Budget.m_AllocationBytes[heapIndex];

                if(m_Budget.m_VulkanUsage[heapIndex] + outBudgets->statistics.blockBytes > m_Budget.m_BlockBytesAtBudgetFetch[heapIndex])
                {
                    outBudgets->usage = m_Budget.m_VulkanUsage[heapIndex] +
                        outBudgets->statistics.blockBytes - m_Budget.m_BlockBytesAtBudgetFetch[heapIndex];
                }
                else
                {
                    outBudgets->usage = 0;
                }

                // Have to take MIN with heap size because explicit HeapSizeLimit is included in it.
                outBudgets->budget = VMA_MIN(
                    m_Budget.m_VulkanBudget[heapIndex], m_MemProps.memoryHeaps[heapIndex].size);
            }
        }
        else
        {
            UpdateVulkanBudget(); // Outside of mutex lock
            GetHeapBudgets(outBudgets, firstHeap, heapCount); // Recursion
        }
    }
    else
#endif
    {
        for(uint32_t i = 0; i < heapCount; ++i, ++outBudgets)
        {
            const uint32_t heapIndex = firstHeap + i;

            outBudgets->statistics.blockCount = m_Budget.m_BlockCount[heapIndex];
            outBudgets->statistics.allocationCount = m_Budget.m_AllocationCount[heapIndex];
            outBudgets->statistics.blockBytes = m_Budget.m_BlockBytes[heapIndex];
            outBudgets->statistics.allocationBytes = m_Budget.m_AllocationBytes[heapIndex];

            outBudgets->usage = outBudgets->statistics.blockBytes;
            outBudgets->budget = m_MemProps.memoryHeaps[heapIndex].size * 8 / 10; // 80% heuristics.
        }
    }
}

void VmaAllocator_T::GetAllocationInfo(VmaAllocation hAllocation, VmaAllocationInfo* pAllocationInfo)
{
    pAllocationInfo->memoryType = hAllocation->GetMemoryTypeIndex();
    pAllocationInfo->deviceMemory = hAllocation->GetMemory();
    pAllocationInfo->offset = hAllocation->GetOffset();
    pAllocationInfo->size = hAllocation->GetSize();
    pAllocationInfo->pMappedData = hAllocation->GetMappedData();
    pAllocationInfo->pUserData = hAllocation->GetUserData();
    pAllocationInfo->pName = hAllocation->GetName();
}

VkResult VmaAllocator_T::CreatePool(const VmaPoolCreateInfo* pCreateInfo, VmaPool* pPool)
{
    VMA_DEBUG_LOG("  CreatePool: MemoryTypeIndex=%u, flags=%u", pCreateInfo->memoryTypeIndex, pCreateInfo->flags);

    VmaPoolCreateInfo newCreateInfo = *pCreateInfo;

    // Protection against uninitialized new structure member. If garbage data are left there, this pointer dereference would crash.
    if(pCreateInfo->pMemoryAllocateNext)
    {
        VMA_ASSERT(((const VkBaseInStructure*)pCreateInfo->pMemoryAllocateNext)->sType != 0);
    }

    if(newCreateInfo.maxBlockCount == 0)
    {
        newCreateInfo.maxBlockCount = SIZE_MAX;
    }
    if(newCreateInfo.minBlockCount > newCreateInfo.maxBlockCount)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // Memory type index out of range or forbidden.
    if(pCreateInfo->memoryTypeIndex >= GetMemoryTypeCount() ||
        ((1u << pCreateInfo->memoryTypeIndex) & m_GlobalMemoryTypeBits) == 0)
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    if(newCreateInfo.minAllocationAlignment > 0)
    {
        VMA_ASSERT(VmaIsPow2(newCreateInfo.minAllocationAlignment));
    }

    const VkDeviceSize preferredBlockSize = CalcPreferredBlockSize(newCreateInfo.memoryTypeIndex);

    *pPool = vma_new(this, VmaPool_T)(this, newCreateInfo, preferredBlockSize);

    VkResult res = (*pPool)->m_BlockVector.CreateMinBlocks();
    if(res != VK_SUCCESS)
    {
        vma_delete(this, *pPool);
        *pPool = VMA_NULL;
        return res;
    }

    // Add to m_Pools.
    {
        VmaMutexLockWrite lock(m_PoolsMutex, m_UseMutex);
        (*pPool)->SetId(m_NextPoolId++);
        m_Pools.PushBack(*pPool);
    }

    return VK_SUCCESS;
}

void VmaAllocator_T::DestroyPool(VmaPool pool)
{
    // Remove from m_Pools.
    {
        VmaMutexLockWrite lock(m_PoolsMutex, m_UseMutex);
        m_Pools.Remove(pool);
    }

    vma_delete(this, pool);
}

void VmaAllocator_T::GetPoolStatistics(VmaPool pool, VmaStatistics* pPoolStats)
{
    VmaClearStatistics(*pPoolStats);
    pool->m_BlockVector.AddStatistics(*pPoolStats);
    pool->m_DedicatedAllocations.AddStatistics(*pPoolStats);
}

void VmaAllocator_T::CalculatePoolStatistics(VmaPool pool, VmaDetailedStatistics* pPoolStats)
{
    VmaClearDetailedStatistics(*pPoolStats);
    pool->m_BlockVector.AddDetailedStatistics(*pPoolStats);
    pool->m_DedicatedAllocations.AddDetailedStatistics(*pPoolStats);
}

void VmaAllocator_T::SetCurrentFrameIndex(uint32_t frameIndex)
{
    m_CurrentFrameIndex.store(frameIndex);

#if VMA_MEMORY_BUDGET
    if(m_UseExtMemoryBudget)
    {
        UpdateVulkanBudget();
    }
#endif // #if VMA_MEMORY_BUDGET
}

VkResult VmaAllocator_T::CheckPoolCorruption(VmaPool hPool)
{
    return hPool->m_BlockVector.CheckCorruption();
}

VkResult VmaAllocator_T::CheckCorruption(uint32_t memoryTypeBits)
{
    VkResult finalRes = VK_ERROR_FEATURE_NOT_PRESENT;

    // Process default pools.
    for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
    {
        VmaBlockVector* const pBlockVector = m_pBlockVectors[memTypeIndex];
        if(pBlockVector != VMA_NULL)
        {
            VkResult localRes = pBlockVector->CheckCorruption();
            switch(localRes)
            {
            case VK_ERROR_FEATURE_NOT_PRESENT:
                break;
            case VK_SUCCESS:
                finalRes = VK_SUCCESS;
                break;
            default:
                return localRes;
            }
        }
    }

    // Process custom pools.
    {
        VmaMutexLockRead lock(m_PoolsMutex, m_UseMutex);
        for(VmaPool pool = m_Pools.Front(); pool != VMA_NULL; pool = m_Pools.GetNext(pool))
        {
            if(((1u << pool->m_BlockVector.GetMemoryTypeIndex()) & memoryTypeBits) != 0)
            {
                VkResult localRes = pool->m_BlockVector.CheckCorruption();
                switch(localRes)
                {
                case VK_ERROR_FEATURE_NOT_PRESENT:
                    break;
                case VK_SUCCESS:
                    finalRes = VK_SUCCESS;
                    break;
                default:
                    return localRes;
                }
            }
        }
    }

    return finalRes;
}

VkResult VmaAllocator_T::AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory)
{
    AtomicTransactionalIncrement<uint32_t> deviceMemoryCountIncrement;
    const uint64_t prevDeviceMemoryCount = deviceMemoryCountIncrement.Increment(&m_DeviceMemoryCount);
#if VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT
    if(prevDeviceMemoryCount >= m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount)
    {
        return VK_ERROR_TOO_MANY_OBJECTS;
    }
#endif

    const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(pAllocateInfo->memoryTypeIndex);

    // HeapSizeLimit is in effect for this heap.
    if((m_HeapSizeLimitMask & (1u << heapIndex)) != 0)
    {
        const VkDeviceSize heapSize = m_MemProps.memoryHeaps[heapIndex].size;
        VkDeviceSize blockBytes = m_Budget.m_BlockBytes[heapIndex];
        for(;;)
        {
            const VkDeviceSize blockBytesAfterAllocation = blockBytes + pAllocateInfo->allocationSize;
            if(blockBytesAfterAllocation > heapSize)
            {
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
            if(m_Budget.m_BlockBytes[heapIndex].compare_exchange_strong(blockBytes, blockBytesAfterAllocation))
            {
                break;
            }
        }
    }
    else
    {
        m_Budget.m_BlockBytes[heapIndex] += pAllocateInfo->allocationSize;
    }
    ++m_Budget.m_BlockCount[heapIndex];

    // VULKAN CALL vkAllocateMemory.
    VkResult res = (*m_VulkanFunctions.vkAllocateMemory)(m_hDevice, pAllocateInfo, GetAllocationCallbacks(), pMemory);

    if(res == VK_SUCCESS)
    {
#if VMA_MEMORY_BUDGET
        ++m_Budget.m_OperationsSinceBudgetFetch;
#endif

        // Informative callback.
        if(m_DeviceMemoryCallbacks.pfnAllocate != VMA_NULL)
        {
            (*m_DeviceMemoryCallbacks.pfnAllocate)(this, pAllocateInfo->memoryTypeIndex, *pMemory, pAllocateInfo->allocationSize, m_DeviceMemoryCallbacks.pUserData);
        }

        deviceMemoryCountIncrement.Commit();
    }
    else
    {
        --m_Budget.m_BlockCount[heapIndex];
        m_Budget.m_BlockBytes[heapIndex] -= pAllocateInfo->allocationSize;
    }

    return res;
}

void VmaAllocator_T::FreeVulkanMemory(uint32_t memoryType, VkDeviceSize size, VkDeviceMemory hMemory)
{
    // Informative callback.
    if(m_DeviceMemoryCallbacks.pfnFree != VMA_NULL)
    {
        (*m_DeviceMemoryCallbacks.pfnFree)(this, memoryType, hMemory, size, m_DeviceMemoryCallbacks.pUserData);
    }

    // VULKAN CALL vkFreeMemory.
    (*m_VulkanFunctions.vkFreeMemory)(m_hDevice, hMemory, GetAllocationCallbacks());

    const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memoryType);
    --m_Budget.m_BlockCount[heapIndex];
    m_Budget.m_BlockBytes[heapIndex] -= size;

    --m_DeviceMemoryCount;
}

VkResult VmaAllocator_T::BindVulkanBuffer(
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset,
    VkBuffer buffer,
    const void* pNext)
{
    if(pNext != VMA_NULL)
    {
#if VMA_VULKAN_VERSION >= 1001000 || VMA_BIND_MEMORY2
        if((m_UseKhrBindMemory2 || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0)) &&
            m_VulkanFunctions.vkBindBufferMemory2KHR != VMA_NULL)
        {
            VkBindBufferMemoryInfoKHR bindBufferMemoryInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };
            bindBufferMemoryInfo.pNext = pNext;
            bindBufferMemoryInfo.buffer = buffer;
            bindBufferMemoryInfo.memory = memory;
            bindBufferMemoryInfo.memoryOffset = memoryOffset;
            return (*m_VulkanFunctions.vkBindBufferMemory2KHR)(m_hDevice, 1, &bindBufferMemoryInfo);
        }
        else
#endif // #if VMA_VULKAN_VERSION >= 1001000 || VMA_BIND_MEMORY2
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    else
    {
        return (*m_VulkanFunctions.vkBindBufferMemory)(m_hDevice, buffer, memory, memoryOffset);
    }
}

VkResult VmaAllocator_T::BindVulkanImage(
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset,
    VkImage image,
    const void* pNext)
{
    if(pNext != VMA_NULL)
    {
#if VMA_VULKAN_VERSION >= 1001000 || VMA_BIND_MEMORY2
        if((m_UseKhrBindMemory2 || m_VulkanApiVersion >= VK_MAKE_VERSION(1, 1, 0)) &&
            m_VulkanFunctions.vkBindImageMemory2KHR != VMA_NULL)
        {
            VkBindImageMemoryInfoKHR bindBufferMemoryInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
            bindBufferMemoryInfo.pNext = pNext;
            bindBufferMemoryInfo.image = image;
            bindBufferMemoryInfo.memory = memory;
            bindBufferMemoryInfo.memoryOffset = memoryOffset;
            return (*m_VulkanFunctions.vkBindImageMemory2KHR)(m_hDevice, 1, &bindBufferMemoryInfo);
        }
        else
#endif // #if VMA_BIND_MEMORY2
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    else
    {
        return (*m_VulkanFunctions.vkBindImageMemory)(m_hDevice, image, memory, memoryOffset);
    }
}

VkResult VmaAllocator_T::Map(VmaAllocation hAllocation, void** ppData)
{
    switch(hAllocation->GetType())
    {
    case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
        {
            VmaDeviceMemoryBlock* const pBlock = hAllocation->GetBlock();
            char *pBytes = VMA_NULL;
            VkResult res = pBlock->Map(this, 1, (void**)&pBytes);
            if(res == VK_SUCCESS)
            {
                *ppData = pBytes + (ptrdiff_t)hAllocation->GetOffset();
                hAllocation->BlockAllocMap();
            }
            return res;
        }
    case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
        return hAllocation->DedicatedAllocMap(this, ppData);
    default:
        VMA_ASSERT(0);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
}

void VmaAllocator_T::Unmap(VmaAllocation hAllocation)
{
    switch(hAllocation->GetType())
    {
    case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
        {
            VmaDeviceMemoryBlock* const pBlock = hAllocation->GetBlock();
            hAllocation->BlockAllocUnmap();
            pBlock->Unmap(this, 1);
        }
        break;
    case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
        hAllocation->DedicatedAllocUnmap(this);
        break;
    default:
        VMA_ASSERT(0);
    }
}

VkResult VmaAllocator_T::BindBufferMemory(
    VmaAllocation hAllocation,
    VkDeviceSize allocationLocalOffset,
    VkBuffer hBuffer,
    const void* pNext)
{
    VkResult res = VK_SUCCESS;
    switch(hAllocation->GetType())
    {
    case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
        res = BindVulkanBuffer(hAllocation->GetMemory(), allocationLocalOffset, hBuffer, pNext);
        break;
    case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
    {
        VmaDeviceMemoryBlock* const pBlock = hAllocation->GetBlock();
        VMA_ASSERT(pBlock && "Binding buffer to allocation that doesn't belong to any block.");
        res = pBlock->BindBufferMemory(this, hAllocation, allocationLocalOffset, hBuffer, pNext);
        break;
    }
    default:
        VMA_ASSERT(0);
    }
    return res;
}

VkResult VmaAllocator_T::BindImageMemory(
    VmaAllocation hAllocation,
    VkDeviceSize allocationLocalOffset,
    VkImage hImage,
    const void* pNext)
{
    VkResult res = VK_SUCCESS;
    switch(hAllocation->GetType())
    {
    case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
        res = BindVulkanImage(hAllocation->GetMemory(), allocationLocalOffset, hImage, pNext);
        break;
    case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
    {
        VmaDeviceMemoryBlock* pBlock = hAllocation->GetBlock();
        VMA_ASSERT(pBlock && "Binding image to allocation that doesn't belong to any block.");
        res = pBlock->BindImageMemory(this, hAllocation, allocationLocalOffset, hImage, pNext);
        break;
    }
    default:
        VMA_ASSERT(0);
    }
    return res;
}

VkResult VmaAllocator_T::FlushOrInvalidateAllocation(
    VmaAllocation hAllocation,
    VkDeviceSize offset, VkDeviceSize size,
    VMA_CACHE_OPERATION op)
{
    VkResult res = VK_SUCCESS;

    VkMappedMemoryRange memRange = {};
    if(GetFlushOrInvalidateRange(hAllocation, offset, size, memRange))
    {
        switch(op)
        {
        case VMA_CACHE_FLUSH:
            res = (*GetVulkanFunctions().vkFlushMappedMemoryRanges)(m_hDevice, 1, &memRange);
            break;
        case VMA_CACHE_INVALIDATE:
            res = (*GetVulkanFunctions().vkInvalidateMappedMemoryRanges)(m_hDevice, 1, &memRange);
            break;
        default:
            VMA_ASSERT(0);
        }
    }
    // else: Just ignore this call.
    return res;
}

VkResult VmaAllocator_T::FlushOrInvalidateAllocations(
    uint32_t allocationCount,
    const VmaAllocation* allocations,
    const VkDeviceSize* offsets, const VkDeviceSize* sizes,
    VMA_CACHE_OPERATION op)
{
    typedef VmaStlAllocator<VkMappedMemoryRange> RangeAllocator;
    typedef VmaSmallVector<VkMappedMemoryRange, RangeAllocator, 16> RangeVector;
    RangeVector ranges = RangeVector(RangeAllocator(GetAllocationCallbacks()));

    for(uint32_t allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
    {
        const VmaAllocation alloc = allocations[allocIndex];
        const VkDeviceSize offset = offsets != VMA_NULL ? offsets[allocIndex] : 0;
        const VkDeviceSize size = sizes != VMA_NULL ? sizes[allocIndex] : VK_WHOLE_SIZE;
        VkMappedMemoryRange newRange;
        if(GetFlushOrInvalidateRange(alloc, offset, size, newRange))
        {
            ranges.push_back(newRange);
        }
    }

    VkResult res = VK_SUCCESS;
    if(!ranges.empty())
    {
        switch(op)
        {
        case VMA_CACHE_FLUSH:
            res = (*GetVulkanFunctions().vkFlushMappedMemoryRanges)(m_hDevice, (uint32_t)ranges.size(), ranges.data());
            break;
        case VMA_CACHE_INVALIDATE:
            res = (*GetVulkanFunctions().vkInvalidateMappedMemoryRanges)(m_hDevice, (uint32_t)ranges.size(), ranges.data());
            break;
        default:
            VMA_ASSERT(0);
        }
    }
    // else: Just ignore this call.
    return res;
}

void VmaAllocator_T::FreeDedicatedMemory(const VmaAllocation allocation)
{
    VMA_ASSERT(allocation && allocation->GetType() == VmaAllocation_T::ALLOCATION_TYPE_DEDICATED);

    const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
    VmaPool parentPool = allocation->GetParentPool();
    if(parentPool == VK_NULL_HANDLE)
    {
        // Default pool
        m_DedicatedAllocations[memTypeIndex].Unregister(allocation);
    }
    else
    {
        // Custom pool
        parentPool->m_DedicatedAllocations.Unregister(allocation);
    }

    VkDeviceMemory hMemory = allocation->GetMemory();

    /*
    There is no need to call this, because Vulkan spec allows to skip vkUnmapMemory
    before vkFreeMemory.

    if(allocation->GetMappedData() != VMA_NULL)
    {
        (*m_VulkanFunctions.vkUnmapMemory)(m_hDevice, hMemory);
    }
    */

    FreeVulkanMemory(memTypeIndex, allocation->GetSize(), hMemory);

    m_Budget.RemoveAllocation(MemoryTypeIndexToHeapIndex(allocation->GetMemoryTypeIndex()), allocation->GetSize());
    m_AllocationObjectAllocator.Free(allocation);

    VMA_DEBUG_LOG("    Freed DedicatedMemory MemoryTypeIndex=%u", memTypeIndex);
}

uint32_t VmaAllocator_T::CalculateGpuDefragmentationMemoryTypeBits() const
{
    VkBufferCreateInfo dummyBufCreateInfo;
    VmaFillGpuDefragmentationBufferCreateInfo(dummyBufCreateInfo);

    uint32_t memoryTypeBits = 0;

    // Create buffer.
    VkBuffer buf = VK_NULL_HANDLE;
    VkResult res = (*GetVulkanFunctions().vkCreateBuffer)(
        m_hDevice, &dummyBufCreateInfo, GetAllocationCallbacks(), &buf);
    if(res == VK_SUCCESS)
    {
        // Query for supported memory types.
        VkMemoryRequirements memReq;
        (*GetVulkanFunctions().vkGetBufferMemoryRequirements)(m_hDevice, buf, &memReq);
        memoryTypeBits = memReq.memoryTypeBits;

        // Destroy buffer.
        (*GetVulkanFunctions().vkDestroyBuffer)(m_hDevice, buf, GetAllocationCallbacks());
    }

    return memoryTypeBits;
}

uint32_t VmaAllocator_T::CalculateGlobalMemoryTypeBits() const
{
    // Make sure memory information is already fetched.
    VMA_ASSERT(GetMemoryTypeCount() > 0);

    uint32_t memoryTypeBits = UINT32_MAX;

    if(!m_UseAmdDeviceCoherentMemory)
    {
        // Exclude memory types that have VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD.
        for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
        {
            if((m_MemProps.memoryTypes[memTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY) != 0)
            {
                memoryTypeBits &= ~(1u << memTypeIndex);
            }
        }
    }

    return memoryTypeBits;
}

bool VmaAllocator_T::GetFlushOrInvalidateRange(
    VmaAllocation allocation,
    VkDeviceSize offset, VkDeviceSize size,
    VkMappedMemoryRange& outRange) const
{
    const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
    if(size > 0 && IsMemoryTypeNonCoherent(memTypeIndex))
    {
        const VkDeviceSize nonCoherentAtomSize = m_PhysicalDeviceProperties.limits.nonCoherentAtomSize;
        const VkDeviceSize allocationSize = allocation->GetSize();
        VMA_ASSERT(offset <= allocationSize);

        outRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        outRange.pNext = VMA_NULL;
        outRange.memory = allocation->GetMemory();

        switch(allocation->GetType())
        {
        case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
            outRange.offset = VmaAlignDown(offset, nonCoherentAtomSize);
            if(size == VK_WHOLE_SIZE)
            {
                outRange.size = allocationSize - outRange.offset;
            }
            else
            {
                VMA_ASSERT(offset + size <= allocationSize);
                outRange.size = VMA_MIN(
                    VmaAlignUp(size + (offset - outRange.offset), nonCoherentAtomSize),
                    allocationSize - outRange.offset);
            }
            break;
        case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
        {
            // 1. Still within this allocation.
            outRange.offset = VmaAlignDown(offset, nonCoherentAtomSize);
            if(size == VK_WHOLE_SIZE)
            {
                size = allocationSize - offset;
            }
            else
            {
                VMA_ASSERT(offset + size <= allocationSize);
            }
            outRange.size = VmaAlignUp(size + (offset - outRange.offset), nonCoherentAtomSize);

            // 2. Adjust to whole block.
            const VkDeviceSize allocationOffset = allocation->GetOffset();
            VMA_ASSERT(allocationOffset % nonCoherentAtomSize == 0);
            const VkDeviceSize blockSize = allocation->GetBlock()->m_pMetadata->GetSize();
            outRange.offset += allocationOffset;
            outRange.size = VMA_MIN(outRange.size, blockSize - outRange.offset);

            break;
        }
        default:
            VMA_ASSERT(0);
        }
        return true;
    }
    return false;
}

#if VMA_MEMORY_BUDGET
void VmaAllocator_T::UpdateVulkanBudget()
{
    VMA_ASSERT(m_UseExtMemoryBudget);

    VkPhysicalDeviceMemoryProperties2KHR memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR };

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VmaPnextChainPushFront(&memProps, &budgetProps);

    GetVulkanFunctions().vkGetPhysicalDeviceMemoryProperties2KHR(m_PhysicalDevice, &memProps);

    {
        VmaMutexLockWrite lockWrite(m_Budget.m_BudgetMutex, m_UseMutex);

        for(uint32_t heapIndex = 0; heapIndex < GetMemoryHeapCount(); ++heapIndex)
        {
            m_Budget.m_VulkanUsage[heapIndex] = budgetProps.heapUsage[heapIndex];
            m_Budget.m_VulkanBudget[heapIndex] = budgetProps.heapBudget[heapIndex];
            m_Budget.m_BlockBytesAtBudgetFetch[heapIndex] = m_Budget.m_BlockBytes[heapIndex].load();

            // Some bugged drivers return the budget incorrectly, e.g. 0 or much bigger than heap size.
            if(m_Budget.m_VulkanBudget[heapIndex] == 0)
            {
                m_Budget.m_VulkanBudget[heapIndex] = m_MemProps.memoryHeaps[heapIndex].size * 8 / 10; // 80% heuristics.
            }
            else if(m_Budget.m_VulkanBudget[heapIndex] > m_MemProps.memoryHeaps[heapIndex].size)
            {
                m_Budget.m_VulkanBudget[heapIndex] = m_MemProps.memoryHeaps[heapIndex].size;
            }
            if(m_Budget.m_VulkanUsage[heapIndex] == 0 && m_Budget.m_BlockBytesAtBudgetFetch[heapIndex] > 0)
            {
                m_Budget.m_VulkanUsage[heapIndex] = m_Budget.m_BlockBytesAtBudgetFetch[heapIndex];
            }
        }
        m_Budget.m_OperationsSinceBudgetFetch = 0;
    }
}
#endif // VMA_MEMORY_BUDGET

void VmaAllocator_T::FillAllocation(const VmaAllocation hAllocation, uint8_t pattern)
{
    if(VMA_DEBUG_INITIALIZE_ALLOCATIONS &&
        hAllocation->IsMappingAllowed() &&
        (m_MemProps.memoryTypes[hAllocation->GetMemoryTypeIndex()].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
    {
        void* pData = VMA_NULL;
        VkResult res = Map(hAllocation, &pData);
        if(res == VK_SUCCESS)
        {
            memset(pData, (int)pattern, (size_t)hAllocation->GetSize());
            FlushOrInvalidateAllocation(hAllocation, 0, VK_WHOLE_SIZE, VMA_CACHE_FLUSH);
            Unmap(hAllocation);
        }
        else
        {
            VMA_ASSERT(0 && "VMA_DEBUG_INITIALIZE_ALLOCATIONS is enabled, but couldn't map memory to fill allocation.");
        }
    }
}

uint32_t VmaAllocator_T::GetGpuDefragmentationMemoryTypeBits()
{
    uint32_t memoryTypeBits = m_GpuDefragmentationMemoryTypeBits.load();
    if(memoryTypeBits == UINT32_MAX)
    {
        memoryTypeBits = CalculateGpuDefragmentationMemoryTypeBits();
        m_GpuDefragmentationMemoryTypeBits.store(memoryTypeBits);
    }
    return memoryTypeBits;
}

#if VMA_STATS_STRING_ENABLED
void VmaAllocator_T::PrintDetailedMap(VmaJsonWriter& json)
{
    json.WriteString("DefaultPools");
    json.BeginObject();
    {
        for (uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
        {
            VmaBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex];
            VmaDedicatedAllocationList& dedicatedAllocList = m_DedicatedAllocations[memTypeIndex];
            if (pBlockVector != VMA_NULL)
            {
                json.BeginString("Type ");
                json.ContinueString(memTypeIndex);
                json.EndString();
                json.BeginObject();
                {
                    json.WriteString("PreferredBlockSize");
                    json.WriteNumber(pBlockVector->GetPreferredBlockSize());

                    json.WriteString("Blocks");
                    pBlockVector->PrintDetailedMap(json);

                    json.WriteString("DedicatedAllocations");
                    dedicatedAllocList.BuildStatsString(json);
                }
                json.EndObject();
            }
        }
    }
    json.EndObject();

    json.WriteString("CustomPools");
    json.BeginObject();
    {
        VmaMutexLockRead lock(m_PoolsMutex, m_UseMutex);
        if (!m_Pools.IsEmpty())
        {
            for (uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
            {
                bool displayType = true;
                size_t index = 0;
                for (VmaPool pool = m_Pools.Front(); pool != VMA_NULL; pool = m_Pools.GetNext(pool))
                {
                    VmaBlockVector& blockVector = pool->m_BlockVector;
                    if (blockVector.GetMemoryTypeIndex() == memTypeIndex)
                    {
                        if (displayType)
                        {
                            json.BeginString("Type ");
                            json.ContinueString(memTypeIndex);
                            json.EndString();
                            json.BeginArray();
                            displayType = false;
                        }

                        json.BeginObject();
                        {
                            json.WriteString("Name");
                            json.BeginString();
                            json.ContinueString_Size(index++);
                            if (pool->GetName())
                            {
                                json.ContinueString(" - ");
                                json.ContinueString(pool->GetName());
                            }
                            json.EndString();

                            json.WriteString("PreferredBlockSize");
                            json.WriteNumber(blockVector.GetPreferredBlockSize());

                            json.WriteString("Blocks");
                            blockVector.PrintDetailedMap(json);

                            json.WriteString("DedicatedAllocations");
                            pool->m_DedicatedAllocations.BuildStatsString(json);
                        }
                        json.EndObject();
                    }
                }

                if (!displayType)
                    json.EndArray();
            }
        }
    }
    json.EndObject();
}
#endif // VMA_STATS_STRING_ENABLED
#endif // _VMA_ALLOCATOR_T_FUNCTIONS


#ifndef _VMA_PUBLIC_INTERFACE
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAllocator(
    const VmaAllocatorCreateInfo* pCreateInfo,
    VmaAllocator* pAllocator)
{
    VMA_ASSERT(pCreateInfo && pAllocator);
    VMA_ASSERT(pCreateInfo->vulkanApiVersion == 0 ||
        (VK_VERSION_MAJOR(pCreateInfo->vulkanApiVersion) == 1 && VK_VERSION_MINOR(pCreateInfo->vulkanApiVersion) <= 3));
    VMA_DEBUG_LOG("vmaCreateAllocator");
    *pAllocator = vma_new(pCreateInfo->pAllocationCallbacks, VmaAllocator_T)(pCreateInfo);
    VkResult result = (*pAllocator)->Init(pCreateInfo);
    if(result < 0)
    {
        vma_delete(pCreateInfo->pAllocationCallbacks, *pAllocator);
        *pAllocator = VK_NULL_HANDLE;
    }
    return result;
}

VMA_CALL_PRE void VMA_CALL_POST vmaDestroyAllocator(
    VmaAllocator allocator)
{
    if(allocator != VK_NULL_HANDLE)
    {
        VMA_DEBUG_LOG("vmaDestroyAllocator");
        VkAllocationCallbacks allocationCallbacks = allocator->m_AllocationCallbacks; // Have to copy the callbacks when destroying.
        vma_delete(&allocationCallbacks, allocator);
    }
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocatorInfo(VmaAllocator allocator, VmaAllocatorInfo* pAllocatorInfo)
{
    VMA_ASSERT(allocator && pAllocatorInfo);
    pAllocatorInfo->instance = allocator->m_hInstance;
    pAllocatorInfo->physicalDevice = allocator->GetPhysicalDevice();
    pAllocatorInfo->device = allocator->m_hDevice;
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetPhysicalDeviceProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceProperties **ppPhysicalDeviceProperties)
{
    VMA_ASSERT(allocator && ppPhysicalDeviceProperties);
    *ppPhysicalDeviceProperties = &allocator->m_PhysicalDeviceProperties;
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetMemoryProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceMemoryProperties** ppPhysicalDeviceMemoryProperties)
{
    VMA_ASSERT(allocator && ppPhysicalDeviceMemoryProperties);
    *ppPhysicalDeviceMemoryProperties = &allocator->m_MemProps;
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetMemoryTypeProperties(
    VmaAllocator allocator,
    uint32_t memoryTypeIndex,
    VkMemoryPropertyFlags* pFlags)
{
    VMA_ASSERT(allocator && pFlags);
    VMA_ASSERT(memoryTypeIndex < allocator->GetMemoryTypeCount());
    *pFlags = allocator->m_MemProps.memoryTypes[memoryTypeIndex].propertyFlags;
}

VMA_CALL_PRE void VMA_CALL_POST vmaSetCurrentFrameIndex(
    VmaAllocator allocator,
    uint32_t frameIndex)
{
    VMA_ASSERT(allocator);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->SetCurrentFrameIndex(frameIndex);
}

VMA_CALL_PRE void VMA_CALL_POST vmaCalculateStatistics(
    VmaAllocator allocator,
    VmaTotalStatistics* pStats)
{
    VMA_ASSERT(allocator && pStats);
    VMA_DEBUG_GLOBAL_MUTEX_LOCK
    allocator->CalculateStatistics(pStats);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetHeapBudgets(
    VmaAllocator allocator,
    VmaBudget* pBudgets)
{
    VMA_ASSERT(allocator && pBudgets);
    VMA_DEBUG_GLOBAL_MUTEX_LOCK
    allocator->GetHeapBudgets(pBudgets, 0, allocator->GetMemoryHeapCount());
}

#if VMA_STATS_STRING_ENABLED

VMA_CALL_PRE void VMA_CALL_POST vmaBuildStatsString(
    VmaAllocator allocator,
    char** ppStatsString,
    VkBool32 detailedMap)
{
    VMA_ASSERT(allocator && ppStatsString);
    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VmaStringBuilder sb(allocator->GetAllocationCallbacks());
    {
        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        allocator->GetHeapBudgets(budgets, 0, allocator->GetMemoryHeapCount());

        VmaTotalStatistics stats;
        allocator->CalculateStatistics(&stats);

        VmaJsonWriter json(allocator->GetAllocationCallbacks(), sb);
        json.BeginObject();
        {
            json.WriteString("General");
            json.BeginObject();
            {
                const VkPhysicalDeviceProperties& deviceProperties = allocator->m_PhysicalDeviceProperties;
                const VkPhysicalDeviceMemoryProperties& memoryProperties = allocator->m_MemProps;

                json.WriteString("API");
                json.WriteString("Vulkan");

                json.WriteString("apiVersion");
                json.BeginString();
                json.ContinueString(VK_API_VERSION_MAJOR(deviceProperties.apiVersion));
                json.ContinueString(".");
                json.ContinueString(VK_API_VERSION_MINOR(deviceProperties.apiVersion));
                json.ContinueString(".");
                json.ContinueString(VK_API_VERSION_PATCH(deviceProperties.apiVersion));
                json.EndString();

                json.WriteString("GPU");
                json.WriteString(deviceProperties.deviceName);
                json.WriteString("deviceType");
                json.WriteNumber(static_cast<uint32_t>(deviceProperties.deviceType));

                json.WriteString("maxMemoryAllocationCount");
                json.WriteNumber(deviceProperties.limits.maxMemoryAllocationCount);
                json.WriteString("bufferImageGranularity");
                json.WriteNumber(deviceProperties.limits.bufferImageGranularity);
                json.WriteString("nonCoherentAtomSize");
                json.WriteNumber(deviceProperties.limits.nonCoherentAtomSize);

                json.WriteString("memoryHeapCount");
                json.WriteNumber(memoryProperties.memoryHeapCount);
                json.WriteString("memoryTypeCount");
                json.WriteNumber(memoryProperties.memoryTypeCount);
            }
            json.EndObject();
        }
        {
            json.WriteString("Total");
            VmaPrintDetailedStatistics(json, stats.total);
        }
        {
            json.WriteString("MemoryInfo");
            json.BeginObject();
            {
                for (uint32_t heapIndex = 0; heapIndex < allocator->GetMemoryHeapCount(); ++heapIndex)
                {
                    json.BeginString("Heap ");
                    json.ContinueString(heapIndex);
                    json.EndString();
                    json.BeginObject();
                    {
                        const VkMemoryHeap& heapInfo = allocator->m_MemProps.memoryHeaps[heapIndex];
                        json.WriteString("Flags");
                        json.BeginArray(true);
                        {
                            if (heapInfo.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                                json.WriteString("DEVICE_LOCAL");
                        #if VMA_VULKAN_VERSION >= 1001000
                            if (heapInfo.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
                                json.WriteString("MULTI_INSTANCE");
                        #endif

                            VkMemoryHeapFlags flags = heapInfo.flags &
                                ~(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
                        #if VMA_VULKAN_VERSION >= 1001000
                                    | VK_MEMORY_HEAP_MULTI_INSTANCE_BIT
                        #endif
                                    );
                            if (flags != 0)
                                json.WriteNumber(flags);
                        }
                        json.EndArray();

                        json.WriteString("Size");
                        json.WriteNumber(heapInfo.size);

                        json.WriteString("Budget");
                        json.BeginObject();
                        {
                            json.WriteString("BudgetBytes");
                            json.WriteNumber(budgets[heapIndex].budget);
                            json.WriteString("UsageBytes");
                            json.WriteNumber(budgets[heapIndex].usage);
                        }
                        json.EndObject();

                        json.WriteString("Stats");
                        VmaPrintDetailedStatistics(json, stats.memoryHeap[heapIndex]);

                        json.WriteString("MemoryPools");
                        json.BeginObject();
                        {
                            for (uint32_t typeIndex = 0; typeIndex < allocator->GetMemoryTypeCount(); ++typeIndex)
                            {
                                if (allocator->MemoryTypeIndexToHeapIndex(typeIndex) == heapIndex)
                                {
                                    json.BeginString("Type ");
                                    json.ContinueString(typeIndex);
                                    json.EndString();
                                    json.BeginObject();
                                    {
                                        json.WriteString("Flags");
                                        json.BeginArray(true);
                                        {
                                            VkMemoryPropertyFlags flags = allocator->m_MemProps.memoryTypes[typeIndex].propertyFlags;
                                            if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                                                json.WriteString("DEVICE_LOCAL");
                                            if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                                                json.WriteString("HOST_VISIBLE");
                                            if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                                                json.WriteString("HOST_COHERENT");
                                            if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                                                json.WriteString("HOST_CACHED");
                                            if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                                                json.WriteString("LAZILY_ALLOCATED");
                                        #if VMA_VULKAN_VERSION >= 1001000
                                            if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
                                                json.WriteString("PROTECTED");
                                        #endif
                                        #if VK_AMD_device_coherent_memory
                                            if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY)
                                                json.WriteString("DEVICE_COHERENT_AMD");
                                            if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY)
                                                json.WriteString("DEVICE_UNCACHED_AMD");
                                        #endif

                                            flags &= ~(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                        #if VMA_VULKAN_VERSION >= 1001000
                                                | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
                                        #endif
                                        #if VK_AMD_device_coherent_memory
                                                | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY
                                                | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY
                                        #endif
                                                | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
                                            if (flags != 0)
                                                json.WriteNumber(flags);
                                        }
                                        json.EndArray();

                                        json.WriteString("Stats");
                                        VmaPrintDetailedStatistics(json, stats.memoryType[typeIndex]);
                                    }
                                    json.EndObject();
                                }
                            }

                        }
                        json.EndObject();
                    }
                    json.EndObject();
                }
            }
            json.EndObject();
        }

        if (detailedMap == VK_TRUE)
            allocator->PrintDetailedMap(json);

        json.EndObject();
    }

    *ppStatsString = VmaCreateStringCopy(allocator->GetAllocationCallbacks(), sb.GetData(), sb.GetLength());
}

VMA_CALL_PRE void VMA_CALL_POST vmaFreeStatsString(
    VmaAllocator allocator,
    char* pStatsString)
{
    if(pStatsString != VMA_NULL)
    {
        VMA_ASSERT(allocator);
        VmaFreeString(allocator->GetAllocationCallbacks(), pStatsString);
    }
}

#endif // VMA_STATS_STRING_ENABLED

/*
This function is not protected by any mutex because it just reads immutable data.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndex(
    VmaAllocator allocator,
    uint32_t memoryTypeBits,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    uint32_t* pMemoryTypeIndex)
{
    VMA_ASSERT(allocator != VK_NULL_HANDLE);
    VMA_ASSERT(pAllocationCreateInfo != VMA_NULL);
    VMA_ASSERT(pMemoryTypeIndex != VMA_NULL);

    return allocator->FindMemoryTypeIndex(memoryTypeBits, pAllocationCreateInfo, UINT32_MAX, pMemoryTypeIndex);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndexForBufferInfo(
    VmaAllocator allocator,
    const VkBufferCreateInfo* pBufferCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    uint32_t* pMemoryTypeIndex)
{
    VMA_ASSERT(allocator != VK_NULL_HANDLE);
    VMA_ASSERT(pBufferCreateInfo != VMA_NULL);
    VMA_ASSERT(pAllocationCreateInfo != VMA_NULL);
    VMA_ASSERT(pMemoryTypeIndex != VMA_NULL);

    const VkDevice hDev = allocator->m_hDevice;
    const VmaVulkanFunctions* funcs = &allocator->GetVulkanFunctions();
    VkResult res;

#if VMA_VULKAN_VERSION >= 1003000
    if(funcs->vkGetDeviceBufferMemoryRequirements)
    {
        // Can query straight from VkBufferCreateInfo :)
        VkDeviceBufferMemoryRequirements devBufMemReq = {VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS};
        devBufMemReq.pCreateInfo = pBufferCreateInfo;

        VkMemoryRequirements2 memReq = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
        (*funcs->vkGetDeviceBufferMemoryRequirements)(hDev, &devBufMemReq, &memReq);

        res = allocator->FindMemoryTypeIndex(
            memReq.memoryRequirements.memoryTypeBits, pAllocationCreateInfo, pBufferCreateInfo->usage, pMemoryTypeIndex);
    }
    else
#endif // #if VMA_VULKAN_VERSION >= 1003000
    {
        // Must create a dummy buffer to query :(
        VkBuffer hBuffer = VK_NULL_HANDLE;
        res = funcs->vkCreateBuffer(
            hDev, pBufferCreateInfo, allocator->GetAllocationCallbacks(), &hBuffer);
        if(res == VK_SUCCESS)
        {
            VkMemoryRequirements memReq = {};
            funcs->vkGetBufferMemoryRequirements(hDev, hBuffer, &memReq);

            res = allocator->FindMemoryTypeIndex(
                memReq.memoryTypeBits, pAllocationCreateInfo, pBufferCreateInfo->usage, pMemoryTypeIndex);

            funcs->vkDestroyBuffer(
                hDev, hBuffer, allocator->GetAllocationCallbacks());
        }
    }
    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndexForImageInfo(
    VmaAllocator allocator,
    const VkImageCreateInfo* pImageCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    uint32_t* pMemoryTypeIndex)
{
    VMA_ASSERT(allocator != VK_NULL_HANDLE);
    VMA_ASSERT(pImageCreateInfo != VMA_NULL);
    VMA_ASSERT(pAllocationCreateInfo != VMA_NULL);
    VMA_ASSERT(pMemoryTypeIndex != VMA_NULL);

    const VkDevice hDev = allocator->m_hDevice;
    const VmaVulkanFunctions* funcs = &allocator->GetVulkanFunctions();
    VkResult res;

#if VMA_VULKAN_VERSION >= 1003000
    if(funcs->vkGetDeviceImageMemoryRequirements)
    {
        // Can query straight from VkImageCreateInfo :)
        VkDeviceImageMemoryRequirements devImgMemReq = {VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS};
        devImgMemReq.pCreateInfo = pImageCreateInfo;
        VMA_ASSERT(pImageCreateInfo->tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT_COPY && (pImageCreateInfo->flags & VK_IMAGE_CREATE_DISJOINT_BIT_COPY) == 0 &&
            "Cannot use this VkImageCreateInfo with vmaFindMemoryTypeIndexForImageInfo as I don't know what to pass as VkDeviceImageMemoryRequirements::planeAspect.");

        VkMemoryRequirements2 memReq = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
        (*funcs->vkGetDeviceImageMemoryRequirements)(hDev, &devImgMemReq, &memReq);

        res = allocator->FindMemoryTypeIndex(
            memReq.memoryRequirements.memoryTypeBits, pAllocationCreateInfo, pImageCreateInfo->usage, pMemoryTypeIndex);
    }
    else
#endif // #if VMA_VULKAN_VERSION >= 1003000
    {
        // Must create a dummy image to query :(
        VkImage hImage = VK_NULL_HANDLE;
        res = funcs->vkCreateImage(
            hDev, pImageCreateInfo, allocator->GetAllocationCallbacks(), &hImage);
        if(res == VK_SUCCESS)
        {
            VkMemoryRequirements memReq = {};
            funcs->vkGetImageMemoryRequirements(hDev, hImage, &memReq);

            res = allocator->FindMemoryTypeIndex(
                memReq.memoryTypeBits, pAllocationCreateInfo, pImageCreateInfo->usage, pMemoryTypeIndex);

            funcs->vkDestroyImage(
                hDev, hImage, allocator->GetAllocationCallbacks());
        }
    }
    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreatePool(
    VmaAllocator allocator,
    const VmaPoolCreateInfo* pCreateInfo,
    VmaPool* pPool)
{
    VMA_ASSERT(allocator && pCreateInfo && pPool);

    VMA_DEBUG_LOG("vmaCreatePool");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->CreatePool(pCreateInfo, pPool);
}

VMA_CALL_PRE void VMA_CALL_POST vmaDestroyPool(
    VmaAllocator allocator,
    VmaPool pool)
{
    VMA_ASSERT(allocator);

    if(pool == VK_NULL_HANDLE)
    {
        return;
    }

    VMA_DEBUG_LOG("vmaDestroyPool");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->DestroyPool(pool);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetPoolStatistics(
    VmaAllocator allocator,
    VmaPool pool,
    VmaStatistics* pPoolStats)
{
    VMA_ASSERT(allocator && pool && pPoolStats);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->GetPoolStatistics(pool, pPoolStats);
}

VMA_CALL_PRE void VMA_CALL_POST vmaCalculatePoolStatistics(
    VmaAllocator allocator,
    VmaPool pool,
    VmaDetailedStatistics* pPoolStats)
{
    VMA_ASSERT(allocator && pool && pPoolStats);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->CalculatePoolStatistics(pool, pPoolStats);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCheckPoolCorruption(VmaAllocator allocator, VmaPool pool)
{
    VMA_ASSERT(allocator && pool);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VMA_DEBUG_LOG("vmaCheckPoolCorruption");

    return allocator->CheckPoolCorruption(pool);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetPoolName(
    VmaAllocator allocator,
    VmaPool pool,
    const char** ppName)
{
    VMA_ASSERT(allocator && pool && ppName);

    VMA_DEBUG_LOG("vmaGetPoolName");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    *ppName = pool->GetName();
}

VMA_CALL_PRE void VMA_CALL_POST vmaSetPoolName(
    VmaAllocator allocator,
    VmaPool pool,
    const char* pName)
{
    VMA_ASSERT(allocator && pool);

    VMA_DEBUG_LOG("vmaSetPoolName");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    pool->SetName(pName);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemory(
    VmaAllocator allocator,
    const VkMemoryRequirements* pVkMemoryRequirements,
    const VmaAllocationCreateInfo* pCreateInfo,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && pVkMemoryRequirements && pCreateInfo && pAllocation);

    VMA_DEBUG_LOG("vmaAllocateMemory");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VkResult result = allocator->AllocateMemory(
        *pVkMemoryRequirements,
        false, // requiresDedicatedAllocation
        false, // prefersDedicatedAllocation
        VK_NULL_HANDLE, // dedicatedBuffer
        VK_NULL_HANDLE, // dedicatedImage
        UINT32_MAX, // dedicatedBufferImageUsage
        *pCreateInfo,
        VMA_SUBALLOCATION_TYPE_UNKNOWN,
        1, // allocationCount
        pAllocation);

    if(pAllocationInfo != VMA_NULL && result == VK_SUCCESS)
    {
        allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
    }

    return result;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryPages(
    VmaAllocator allocator,
    const VkMemoryRequirements* pVkMemoryRequirements,
    const VmaAllocationCreateInfo* pCreateInfo,
    size_t allocationCount,
    VmaAllocation* pAllocations,
    VmaAllocationInfo* pAllocationInfo)
{
    if(allocationCount == 0)
    {
        return VK_SUCCESS;
    }

    VMA_ASSERT(allocator && pVkMemoryRequirements && pCreateInfo && pAllocations);

    VMA_DEBUG_LOG("vmaAllocateMemoryPages");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VkResult result = allocator->AllocateMemory(
        *pVkMemoryRequirements,
        false, // requiresDedicatedAllocation
        false, // prefersDedicatedAllocation
        VK_NULL_HANDLE, // dedicatedBuffer
        VK_NULL_HANDLE, // dedicatedImage
        UINT32_MAX, // dedicatedBufferImageUsage
        *pCreateInfo,
        VMA_SUBALLOCATION_TYPE_UNKNOWN,
        allocationCount,
        pAllocations);

    if(pAllocationInfo != VMA_NULL && result == VK_SUCCESS)
    {
        for(size_t i = 0; i < allocationCount; ++i)
        {
            allocator->GetAllocationInfo(pAllocations[i], pAllocationInfo + i);
        }
    }

    return result;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryForBuffer(
    VmaAllocator allocator,
    VkBuffer buffer,
    const VmaAllocationCreateInfo* pCreateInfo,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && buffer != VK_NULL_HANDLE && pCreateInfo && pAllocation);

    VMA_DEBUG_LOG("vmaAllocateMemoryForBuffer");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VkMemoryRequirements vkMemReq = {};
    bool requiresDedicatedAllocation = false;
    bool prefersDedicatedAllocation = false;
    allocator->GetBufferMemoryRequirements(buffer, vkMemReq,
        requiresDedicatedAllocation,
        prefersDedicatedAllocation);

    VkResult result = allocator->AllocateMemory(
        vkMemReq,
        requiresDedicatedAllocation,
        prefersDedicatedAllocation,
        buffer, // dedicatedBuffer
        VK_NULL_HANDLE, // dedicatedImage
        UINT32_MAX, // dedicatedBufferImageUsage
        *pCreateInfo,
        VMA_SUBALLOCATION_TYPE_BUFFER,
        1, // allocationCount
        pAllocation);

    if(pAllocationInfo && result == VK_SUCCESS)
    {
        allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
    }

    return result;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryForImage(
    VmaAllocator allocator,
    VkImage image,
    const VmaAllocationCreateInfo* pCreateInfo,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && image != VK_NULL_HANDLE && pCreateInfo && pAllocation);

    VMA_DEBUG_LOG("vmaAllocateMemoryForImage");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    VkMemoryRequirements vkMemReq = {};
    bool requiresDedicatedAllocation = false;
    bool prefersDedicatedAllocation  = false;
    allocator->GetImageMemoryRequirements(image, vkMemReq,
        requiresDedicatedAllocation, prefersDedicatedAllocation);

    VkResult result = allocator->AllocateMemory(
        vkMemReq,
        requiresDedicatedAllocation,
        prefersDedicatedAllocation,
        VK_NULL_HANDLE, // dedicatedBuffer
        image, // dedicatedImage
        UINT32_MAX, // dedicatedBufferImageUsage
        *pCreateInfo,
        VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN,
        1, // allocationCount
        pAllocation);

    if(pAllocationInfo && result == VK_SUCCESS)
    {
        allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
    }

    return result;
}

VMA_CALL_PRE void VMA_CALL_POST vmaFreeMemory(
    VmaAllocator allocator,
    VmaAllocation allocation)
{
    VMA_ASSERT(allocator);

    if(allocation == VK_NULL_HANDLE)
    {
        return;
    }

    VMA_DEBUG_LOG("vmaFreeMemory");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->FreeMemory(
        1, // allocationCount
        &allocation);
}

VMA_CALL_PRE void VMA_CALL_POST vmaFreeMemoryPages(
    VmaAllocator allocator,
    size_t allocationCount,
    const VmaAllocation* pAllocations)
{
    if(allocationCount == 0)
    {
        return;
    }

    VMA_ASSERT(allocator);

    VMA_DEBUG_LOG("vmaFreeMemoryPages");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->FreeMemory(allocationCount, pAllocations);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocationInfo(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && allocation && pAllocationInfo);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->GetAllocationInfo(allocation, pAllocationInfo);
}

VMA_CALL_PRE void VMA_CALL_POST vmaSetAllocationUserData(
    VmaAllocator allocator,
    VmaAllocation allocation,
    void* pUserData)
{
    VMA_ASSERT(allocator && allocation);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocation->SetUserData(allocator, pUserData);
}

VMA_CALL_PRE void VMA_CALL_POST vmaSetAllocationName(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const char* VMA_NULLABLE pName)
{
    allocation->SetName(allocator, pName);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocationMemoryProperties(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkMemoryPropertyFlags* VMA_NOT_NULL pFlags)
{
    VMA_ASSERT(allocator && allocation && pFlags);
    const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
    *pFlags = allocator->m_MemProps.memoryTypes[memTypeIndex].propertyFlags;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaMapMemory(
    VmaAllocator allocator,
    VmaAllocation allocation,
    void** ppData)
{
    VMA_ASSERT(allocator && allocation && ppData);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->Map(allocation, ppData);
}

VMA_CALL_PRE void VMA_CALL_POST vmaUnmapMemory(
    VmaAllocator allocator,
    VmaAllocation allocation)
{
    VMA_ASSERT(allocator && allocation);

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    allocator->Unmap(allocation);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaFlushAllocation(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize offset,
    VkDeviceSize size)
{
    VMA_ASSERT(allocator && allocation);

    VMA_DEBUG_LOG("vmaFlushAllocation");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    const VkResult res = allocator->FlushOrInvalidateAllocation(allocation, offset, size, VMA_CACHE_FLUSH);

    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaInvalidateAllocation(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize offset,
    VkDeviceSize size)
{
    VMA_ASSERT(allocator && allocation);

    VMA_DEBUG_LOG("vmaInvalidateAllocation");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    const VkResult res = allocator->FlushOrInvalidateAllocation(allocation, offset, size, VMA_CACHE_INVALIDATE);

    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaFlushAllocations(
    VmaAllocator allocator,
    uint32_t allocationCount,
    const VmaAllocation* allocations,
    const VkDeviceSize* offsets,
    const VkDeviceSize* sizes)
{
    VMA_ASSERT(allocator);

    if(allocationCount == 0)
    {
        return VK_SUCCESS;
    }

    VMA_ASSERT(allocations);

    VMA_DEBUG_LOG("vmaFlushAllocations");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    const VkResult res = allocator->FlushOrInvalidateAllocations(allocationCount, allocations, offsets, sizes, VMA_CACHE_FLUSH);

    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaInvalidateAllocations(
    VmaAllocator allocator,
    uint32_t allocationCount,
    const VmaAllocation* allocations,
    const VkDeviceSize* offsets,
    const VkDeviceSize* sizes)
{
    VMA_ASSERT(allocator);

    if(allocationCount == 0)
    {
        return VK_SUCCESS;
    }

    VMA_ASSERT(allocations);

    VMA_DEBUG_LOG("vmaInvalidateAllocations");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    const VkResult res = allocator->FlushOrInvalidateAllocations(allocationCount, allocations, offsets, sizes, VMA_CACHE_INVALIDATE);

    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCheckCorruption(
    VmaAllocator allocator,
    uint32_t memoryTypeBits)
{
    VMA_ASSERT(allocator);

    VMA_DEBUG_LOG("vmaCheckCorruption");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->CheckCorruption(memoryTypeBits);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBeginDefragmentation(
    VmaAllocator allocator,
    const VmaDefragmentationInfo* pInfo,
    VmaDefragmentationContext* pContext)
{
    VMA_ASSERT(allocator && pInfo && pContext);

    VMA_DEBUG_LOG("vmaBeginDefragmentation");

    if (pInfo->pool != VMA_NULL)
    {
        // Check if run on supported algorithms
        if (pInfo->pool->m_BlockVector.GetAlgorithm() & VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT)
            return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    *pContext = vma_new(allocator, VmaDefragmentationContext_T)(allocator, *pInfo);
    return VK_SUCCESS;
}

VMA_CALL_PRE void VMA_CALL_POST vmaEndDefragmentation(
    VmaAllocator allocator,
    VmaDefragmentationContext context,
    VmaDefragmentationStats* pStats)
{
    VMA_ASSERT(allocator && context);

    VMA_DEBUG_LOG("vmaEndDefragmentation");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    if (pStats)
        context->GetStats(*pStats);
    vma_delete(allocator, context);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBeginDefragmentationPass(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaDefragmentationContext VMA_NOT_NULL context,
    VmaDefragmentationPassMoveInfo* VMA_NOT_NULL pPassInfo)
{
    VMA_ASSERT(context && pPassInfo);

    VMA_DEBUG_LOG("vmaBeginDefragmentationPass");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return context->DefragmentPassBegin(*pPassInfo);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaEndDefragmentationPass(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaDefragmentationContext VMA_NOT_NULL context,
    VmaDefragmentationPassMoveInfo* VMA_NOT_NULL pPassInfo)
{
    VMA_ASSERT(context && pPassInfo);

    VMA_DEBUG_LOG("vmaEndDefragmentationPass");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return context->DefragmentPassEnd(*pPassInfo);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindBufferMemory(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkBuffer buffer)
{
    VMA_ASSERT(allocator && allocation && buffer);

    VMA_DEBUG_LOG("vmaBindBufferMemory");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->BindBufferMemory(allocation, 0, buffer, VMA_NULL);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindBufferMemory2(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize allocationLocalOffset,
    VkBuffer buffer,
    const void* pNext)
{
    VMA_ASSERT(allocator && allocation && buffer);

    VMA_DEBUG_LOG("vmaBindBufferMemory2");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->BindBufferMemory(allocation, allocationLocalOffset, buffer, pNext);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindImageMemory(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkImage image)
{
    VMA_ASSERT(allocator && allocation && image);

    VMA_DEBUG_LOG("vmaBindImageMemory");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    return allocator->BindImageMemory(allocation, 0, image, VMA_NULL);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindImageMemory2(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize allocationLocalOffset,
    VkImage image,
    const void* pNext)
{
    VMA_ASSERT(allocator && allocation && image);

    VMA_DEBUG_LOG("vmaBindImageMemory2");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

        return allocator->BindImageMemory(allocation, allocationLocalOffset, image, pNext);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateBuffer(
    VmaAllocator allocator,
    const VkBufferCreateInfo* pBufferCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkBuffer* pBuffer,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && pBufferCreateInfo && pAllocationCreateInfo && pBuffer && pAllocation);

    if(pBufferCreateInfo->size == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if((pBufferCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_COPY) != 0 &&
        !allocator->m_UseKhrBufferDeviceAddress)
    {
        VMA_ASSERT(0 && "Creating a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is not valid if VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT was not used.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VMA_DEBUG_LOG("vmaCreateBuffer");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    *pBuffer = VK_NULL_HANDLE;
    *pAllocation = VK_NULL_HANDLE;

    // 1. Create VkBuffer.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateBuffer)(
        allocator->m_hDevice,
        pBufferCreateInfo,
        allocator->GetAllocationCallbacks(),
        pBuffer);
    if(res >= 0)
    {
        // 2. vkGetBufferMemoryRequirements.
        VkMemoryRequirements vkMemReq = {};
        bool requiresDedicatedAllocation = false;
        bool prefersDedicatedAllocation  = false;
        allocator->GetBufferMemoryRequirements(*pBuffer, vkMemReq,
            requiresDedicatedAllocation, prefersDedicatedAllocation);

        // 3. Allocate memory using allocator.
        res = allocator->AllocateMemory(
            vkMemReq,
            requiresDedicatedAllocation,
            prefersDedicatedAllocation,
            *pBuffer, // dedicatedBuffer
            VK_NULL_HANDLE, // dedicatedImage
            pBufferCreateInfo->usage, // dedicatedBufferImageUsage
            *pAllocationCreateInfo,
            VMA_SUBALLOCATION_TYPE_BUFFER,
            1, // allocationCount
            pAllocation);

        if(res >= 0)
        {
            // 3. Bind buffer with memory.
            if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
            {
                res = allocator->BindBufferMemory(*pAllocation, 0, *pBuffer, VMA_NULL);
            }
            if(res >= 0)
            {
                // All steps succeeded.
                #if VMA_STATS_STRING_ENABLED
                    (*pAllocation)->InitBufferImageUsage(pBufferCreateInfo->usage);
                #endif
                if(pAllocationInfo != VMA_NULL)
                {
                    allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
                }

                return VK_SUCCESS;
            }
            allocator->FreeMemory(
                1, // allocationCount
                pAllocation);
            *pAllocation = VK_NULL_HANDLE;
            (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
            *pBuffer = VK_NULL_HANDLE;
            return res;
        }
        (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
        *pBuffer = VK_NULL_HANDLE;
        return res;
    }
    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateBufferWithAlignment(
    VmaAllocator allocator,
    const VkBufferCreateInfo* pBufferCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkDeviceSize minAlignment,
    VkBuffer* pBuffer,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && pBufferCreateInfo && pAllocationCreateInfo && VmaIsPow2(minAlignment) && pBuffer && pAllocation);

    if(pBufferCreateInfo->size == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if((pBufferCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_COPY) != 0 &&
        !allocator->m_UseKhrBufferDeviceAddress)
    {
        VMA_ASSERT(0 && "Creating a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is not valid if VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT was not used.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VMA_DEBUG_LOG("vmaCreateBufferWithAlignment");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    *pBuffer = VK_NULL_HANDLE;
    *pAllocation = VK_NULL_HANDLE;

    // 1. Create VkBuffer.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateBuffer)(
        allocator->m_hDevice,
        pBufferCreateInfo,
        allocator->GetAllocationCallbacks(),
        pBuffer);
    if(res >= 0)
    {
        // 2. vkGetBufferMemoryRequirements.
        VkMemoryRequirements vkMemReq = {};
        bool requiresDedicatedAllocation = false;
        bool prefersDedicatedAllocation  = false;
        allocator->GetBufferMemoryRequirements(*pBuffer, vkMemReq,
            requiresDedicatedAllocation, prefersDedicatedAllocation);

        // 2a. Include minAlignment
        vkMemReq.alignment = VMA_MAX(vkMemReq.alignment, minAlignment);

        // 3. Allocate memory using allocator.
        res = allocator->AllocateMemory(
            vkMemReq,
            requiresDedicatedAllocation,
            prefersDedicatedAllocation,
            *pBuffer, // dedicatedBuffer
            VK_NULL_HANDLE, // dedicatedImage
            pBufferCreateInfo->usage, // dedicatedBufferImageUsage
            *pAllocationCreateInfo,
            VMA_SUBALLOCATION_TYPE_BUFFER,
            1, // allocationCount
            pAllocation);

        if(res >= 0)
        {
            // 3. Bind buffer with memory.
            if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
            {
                res = allocator->BindBufferMemory(*pAllocation, 0, *pBuffer, VMA_NULL);
            }
            if(res >= 0)
            {
                // All steps succeeded.
                #if VMA_STATS_STRING_ENABLED
                    (*pAllocation)->InitBufferImageUsage(pBufferCreateInfo->usage);
                #endif
                if(pAllocationInfo != VMA_NULL)
                {
                    allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
                }

                return VK_SUCCESS;
            }
            allocator->FreeMemory(
                1, // allocationCount
                pAllocation);
            *pAllocation = VK_NULL_HANDLE;
            (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
            *pBuffer = VK_NULL_HANDLE;
            return res;
        }
        (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
        *pBuffer = VK_NULL_HANDLE;
        return res;
    }
    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingBuffer(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pBuffer)
{
    VMA_ASSERT(allocator && pBufferCreateInfo && pBuffer && allocation);

    VMA_DEBUG_LOG("vmaCreateAliasingBuffer");

    *pBuffer = VK_NULL_HANDLE;

    if (pBufferCreateInfo->size == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if ((pBufferCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_COPY) != 0 &&
        !allocator->m_UseKhrBufferDeviceAddress)
    {
        VMA_ASSERT(0 && "Creating a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is not valid if VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT was not used.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    // 1. Create VkBuffer.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateBuffer)(
        allocator->m_hDevice,
        pBufferCreateInfo,
        allocator->GetAllocationCallbacks(),
        pBuffer);
    if (res >= 0)
    {
        // 2. Bind buffer with memory.
        res = allocator->BindBufferMemory(allocation, 0, *pBuffer, VMA_NULL);
        if (res >= 0)
        {
            return VK_SUCCESS;
        }
        (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
    }
    return res;
}

VMA_CALL_PRE void VMA_CALL_POST vmaDestroyBuffer(
    VmaAllocator allocator,
    VkBuffer buffer,
    VmaAllocation allocation)
{
    VMA_ASSERT(allocator);

    if(buffer == VK_NULL_HANDLE && allocation == VK_NULL_HANDLE)
    {
        return;
    }

    VMA_DEBUG_LOG("vmaDestroyBuffer");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    if(buffer != VK_NULL_HANDLE)
    {
        (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, buffer, allocator->GetAllocationCallbacks());
    }

    if(allocation != VK_NULL_HANDLE)
    {
        allocator->FreeMemory(
            1, // allocationCount
            &allocation);
    }
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateImage(
    VmaAllocator allocator,
    const VkImageCreateInfo* pImageCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkImage* pImage,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo)
{
    VMA_ASSERT(allocator && pImageCreateInfo && pAllocationCreateInfo && pImage && pAllocation);

    if(pImageCreateInfo->extent.width == 0 ||
        pImageCreateInfo->extent.height == 0 ||
        pImageCreateInfo->extent.depth == 0 ||
        pImageCreateInfo->mipLevels == 0 ||
        pImageCreateInfo->arrayLayers == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VMA_DEBUG_LOG("vmaCreateImage");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    *pImage = VK_NULL_HANDLE;
    *pAllocation = VK_NULL_HANDLE;

    // 1. Create VkImage.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateImage)(
        allocator->m_hDevice,
        pImageCreateInfo,
        allocator->GetAllocationCallbacks(),
        pImage);
    if(res >= 0)
    {
        VmaSuballocationType suballocType = pImageCreateInfo->tiling == VK_IMAGE_TILING_OPTIMAL ?
            VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL :
            VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR;

        // 2. Allocate memory using allocator.
        VkMemoryRequirements vkMemReq = {};
        bool requiresDedicatedAllocation = false;
        bool prefersDedicatedAllocation  = false;
        allocator->GetImageMemoryRequirements(*pImage, vkMemReq,
            requiresDedicatedAllocation, prefersDedicatedAllocation);

        res = allocator->AllocateMemory(
            vkMemReq,
            requiresDedicatedAllocation,
            prefersDedicatedAllocation,
            VK_NULL_HANDLE, // dedicatedBuffer
            *pImage, // dedicatedImage
            pImageCreateInfo->usage, // dedicatedBufferImageUsage
            *pAllocationCreateInfo,
            suballocType,
            1, // allocationCount
            pAllocation);

        if(res >= 0)
        {
            // 3. Bind image with memory.
            if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
            {
                res = allocator->BindImageMemory(*pAllocation, 0, *pImage, VMA_NULL);
            }
            if(res >= 0)
            {
                // All steps succeeded.
                #if VMA_STATS_STRING_ENABLED
                    (*pAllocation)->InitBufferImageUsage(pImageCreateInfo->usage);
                #endif
                if(pAllocationInfo != VMA_NULL)
                {
                    allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
                }

                return VK_SUCCESS;
            }
            allocator->FreeMemory(
                1, // allocationCount
                pAllocation);
            *pAllocation = VK_NULL_HANDLE;
            (*allocator->GetVulkanFunctions().vkDestroyImage)(allocator->m_hDevice, *pImage, allocator->GetAllocationCallbacks());
            *pImage = VK_NULL_HANDLE;
            return res;
        }
        (*allocator->GetVulkanFunctions().vkDestroyImage)(allocator->m_hDevice, *pImage, allocator->GetAllocationCallbacks());
        *pImage = VK_NULL_HANDLE;
        return res;
    }
    return res;
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingImage(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const VkImageCreateInfo* VMA_NOT_NULL pImageCreateInfo,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pImage)
{
    VMA_ASSERT(allocator && pImageCreateInfo && pImage && allocation);

    *pImage = VK_NULL_HANDLE;

    VMA_DEBUG_LOG("vmaCreateImage");

    if (pImageCreateInfo->extent.width == 0 ||
        pImageCreateInfo->extent.height == 0 ||
        pImageCreateInfo->extent.depth == 0 ||
        pImageCreateInfo->mipLevels == 0 ||
        pImageCreateInfo->arrayLayers == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    // 1. Create VkImage.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateImage)(
        allocator->m_hDevice,
        pImageCreateInfo,
        allocator->GetAllocationCallbacks(),
        pImage);
    if (res >= 0)
    {
        // 2. Bind image with memory.
        res = allocator->BindImageMemory(allocation, 0, *pImage, VMA_NULL);
        if (res >= 0)
        {
            return VK_SUCCESS;
        }
        (*allocator->GetVulkanFunctions().vkDestroyImage)(allocator->m_hDevice, *pImage, allocator->GetAllocationCallbacks());
    }
    return res;
}

VMA_CALL_PRE void VMA_CALL_POST vmaDestroyImage(
    VmaAllocator VMA_NOT_NULL allocator,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE image,
    VmaAllocation VMA_NULLABLE allocation)
{
    VMA_ASSERT(allocator);

    if(image == VK_NULL_HANDLE && allocation == VK_NULL_HANDLE)
    {
        return;
    }

    VMA_DEBUG_LOG("vmaDestroyImage");

    VMA_DEBUG_GLOBAL_MUTEX_LOCK

    if(image != VK_NULL_HANDLE)
    {
        (*allocator->GetVulkanFunctions().vkDestroyImage)(allocator->m_hDevice, image, allocator->GetAllocationCallbacks());
    }
    if(allocation != VK_NULL_HANDLE)
    {
        allocator->FreeMemory(
            1, // allocationCount
            &allocation);
    }
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateVirtualBlock(
    const VmaVirtualBlockCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaVirtualBlock VMA_NULLABLE * VMA_NOT_NULL pVirtualBlock)
{
    VMA_ASSERT(pCreateInfo && pVirtualBlock);
    VMA_ASSERT(pCreateInfo->size > 0);
    VMA_DEBUG_LOG("vmaCreateVirtualBlock");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    *pVirtualBlock = vma_new(pCreateInfo->pAllocationCallbacks, VmaVirtualBlock_T)(*pCreateInfo);
    VkResult res = (*pVirtualBlock)->Init();
    if(res < 0)
    {
        vma_delete(pCreateInfo->pAllocationCallbacks, *pVirtualBlock);
        *pVirtualBlock = VK_NULL_HANDLE;
    }
    return res;
}

VMA_CALL_PRE void VMA_CALL_POST vmaDestroyVirtualBlock(VmaVirtualBlock VMA_NULLABLE virtualBlock)
{
    if(virtualBlock != VK_NULL_HANDLE)
    {
        VMA_DEBUG_LOG("vmaDestroyVirtualBlock");
        VMA_DEBUG_GLOBAL_MUTEX_LOCK;
        VkAllocationCallbacks allocationCallbacks = virtualBlock->m_AllocationCallbacks; // Have to copy the callbacks when destroying.
        vma_delete(&allocationCallbacks, virtualBlock);
    }
}

VMA_CALL_PRE VkBool32 VMA_CALL_POST vmaIsVirtualBlockEmpty(VmaVirtualBlock VMA_NOT_NULL virtualBlock)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE);
    VMA_DEBUG_LOG("vmaIsVirtualBlockEmpty");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    return virtualBlock->IsEmpty() ? VK_TRUE : VK_FALSE;
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetVirtualAllocationInfo(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaVirtualAllocation VMA_NOT_NULL_NON_DISPATCHABLE allocation, VmaVirtualAllocationInfo* VMA_NOT_NULL pVirtualAllocInfo)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE && pVirtualAllocInfo != VMA_NULL);
    VMA_DEBUG_LOG("vmaGetVirtualAllocationInfo");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    virtualBlock->GetAllocationInfo(allocation, *pVirtualAllocInfo);
}

VMA_CALL_PRE VkResult VMA_CALL_POST vmaVirtualAllocate(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    const VmaVirtualAllocationCreateInfo* VMA_NOT_NULL pCreateInfo, VmaVirtualAllocation VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pAllocation,
    VkDeviceSize* VMA_NULLABLE pOffset)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE && pCreateInfo != VMA_NULL && pAllocation != VMA_NULL);
    VMA_DEBUG_LOG("vmaVirtualAllocate");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    return virtualBlock->Allocate(*pCreateInfo, *pAllocation, pOffset);
}

VMA_CALL_PRE void VMA_CALL_POST vmaVirtualFree(VmaVirtualBlock VMA_NOT_NULL virtualBlock, VmaVirtualAllocation VMA_NULLABLE_NON_DISPATCHABLE allocation)
{
    if(allocation != VK_NULL_HANDLE)
    {
        VMA_ASSERT(virtualBlock != VK_NULL_HANDLE);
        VMA_DEBUG_LOG("vmaVirtualFree");
        VMA_DEBUG_GLOBAL_MUTEX_LOCK;
        virtualBlock->Free(allocation);
    }
}

VMA_CALL_PRE void VMA_CALL_POST vmaClearVirtualBlock(VmaVirtualBlock VMA_NOT_NULL virtualBlock)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE);
    VMA_DEBUG_LOG("vmaClearVirtualBlock");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    virtualBlock->Clear();
}

VMA_CALL_PRE void VMA_CALL_POST vmaSetVirtualAllocationUserData(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaVirtualAllocation VMA_NOT_NULL_NON_DISPATCHABLE allocation, void* VMA_NULLABLE pUserData)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE);
    VMA_DEBUG_LOG("vmaSetVirtualAllocationUserData");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    virtualBlock->SetAllocationUserData(allocation, pUserData);
}

VMA_CALL_PRE void VMA_CALL_POST vmaGetVirtualBlockStatistics(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaStatistics* VMA_NOT_NULL pStats)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE && pStats != VMA_NULL);
    VMA_DEBUG_LOG("vmaGetVirtualBlockStatistics");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    virtualBlock->GetStatistics(*pStats);
}

VMA_CALL_PRE void VMA_CALL_POST vmaCalculateVirtualBlockStatistics(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaDetailedStatistics* VMA_NOT_NULL pStats)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE && pStats != VMA_NULL);
    VMA_DEBUG_LOG("vmaCalculateVirtualBlockStatistics");
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    virtualBlock->CalculateDetailedStatistics(*pStats);
}

#if VMA_STATS_STRING_ENABLED

VMA_CALL_PRE void VMA_CALL_POST vmaBuildVirtualBlockStatsString(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    char* VMA_NULLABLE * VMA_NOT_NULL ppStatsString, VkBool32 detailedMap)
{
    VMA_ASSERT(virtualBlock != VK_NULL_HANDLE && ppStatsString != VMA_NULL);
    VMA_DEBUG_GLOBAL_MUTEX_LOCK;
    const VkAllocationCallbacks* allocationCallbacks = virtualBlock->GetAllocationCallbacks();
    VmaStringBuilder sb(allocationCallbacks);
    virtualBlock->BuildStatsString(detailedMap != VK_FALSE, sb);
    *ppStatsString = VmaCreateStringCopy(allocationCallbacks, sb.GetData(), sb.GetLength());
}

VMA_CALL_PRE void VMA_CALL_POST vmaFreeVirtualBlockStatsString(VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    char* VMA_NULLABLE pStatsString)
{
    if(pStatsString != VMA_NULL)
    {
        VMA_ASSERT(virtualBlock != VK_NULL_HANDLE);
        VMA_DEBUG_GLOBAL_MUTEX_LOCK;
        VmaFreeString(virtualBlock->GetAllocationCallbacks(), pStatsString);
    }
}
#endif // VMA_STATS_STRING_ENABLED
#endif // _VMA_PUBLIC_INTERFACE

#undef VMA_IMPLEMENTATION