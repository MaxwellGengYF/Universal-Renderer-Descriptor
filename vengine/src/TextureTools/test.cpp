#include <TextureTools/Allocator.h>
#ifdef _DEBUG
int main(){
    using namespace textools;
    Allocator alloc;
    alloc.Reset(1024);
    alloc.PrintVecSize();
    auto sb = alloc.Allocate(64);
    auto sb1 = alloc.Allocate(128);
    alloc.PrintVecSize();
    alloc.DeAllocate(sb1);
    alloc.PrintVecSize();
    alloc.DeAllocate(sb);
    alloc.PrintVecSize();

    return 0;
}
#endif