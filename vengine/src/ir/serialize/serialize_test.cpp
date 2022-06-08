#include "ser_context.h"
#include "deser_context.h"
#include "serializer.h"
namespace toolhub::ir {
class B  {
public:
	uint64 value;

	void Ser(SerContext& func) const  {
		func.Push(value);
	}
	void Deser(DeserContext& ptr)  {
		ptr.Pop(value);
	}
};
class C  {
public:
	uint value;
	B* ptr;

	void Ser(SerContext& func) const  {
		func.Push(value);
		func.Push(ptr);
	}
	void Deser(DeserContext& ptr)  {
		ptr.Pop(value);
		this->ptr = ptr.TryDeser<B>();
	}
};
class A  {
public:
	uint64 value;
	B* ptr;
    C* cPtr;

	void Ser(SerContext& func) const  {
		func.Push(value);
		func.Push(ptr);
        func.Push(cPtr);
	}
	void Deser(DeserContext& ptr)  {
		ptr.Pop(value);
		this->ptr = ptr.TryDeser<B>();
        this->cPtr = ptr.TryDeser<C>();
	}
};

}// namespace toolhub::ir
int main() {
	using namespace toolhub::ir;
	vstd::vector<vbyte> bytes;
	{
		SerContext ser;
		A a;
		B* b = new B();
        C* c = new C();
		a.value = 53;
		a.ptr = b;
		b->value = 69;
        c->value = 999;
        c->ptr = b;
        a.cPtr = c;
		ser.Serialize(&a);
		bytes = std::move(ser).data();
	}
	{
		A a;
		vstd::ObjectStackAlloc alloc(4096);
		DeserContext deser;
		deser.DeSerialize(bytes, &alloc, &a);
		std::cout << a.value << ' ' << a.ptr->value << ' ' << a.cPtr->value << ' ' << (a.cPtr->ptr == a.ptr ? "true" : "false") << '\n';
	}
	return 0;
}