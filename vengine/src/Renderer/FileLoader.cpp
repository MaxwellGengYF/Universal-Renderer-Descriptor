#include <Common/Common.h>
#include <Common/LockFreeArrayQueue.h>
#include <Utility/TaskThread.h>
#include <Utility/BinaryReader.h>
namespace toolhub::dsk {

struct DataSpan : public vstd::IOperatorNewBase {
	void* data;
	size_t size;
	std::atomic_size_t refCount = 2;
	std::atomic_bool loadState = false;
	void Dispose() {
		if (--refCount == 0) {
			delete this;
		}
	}
};
struct DataSpanCopy : public vstd::IOperatorNewBase {
	void* data;
	size_t size;
	uint loadState;
};
struct BinaryDiskRead {
	DataSpan* sp;
	vstd::string path;
};
struct Data {
	TaskThread thread;
	vstd::LockFreeArrayQueue<BinaryDiskRead> queue;
	Data();
	void operator()();
	void Execute(BinaryDiskRead const& cmd);
};
vstd::optional<Data> data;
Data::Data() {
	thread.SetFunctor(*this);
}
void Data::operator()() {
	while (auto v = queue.Pop()) {
		Execute(*v);
	}
}
void Data::Execute(BinaryDiskRead const& cmd) {
	auto disp = vstd::create_disposer([&] {
		cmd.sp->loadState = true;
		cmd.sp->Dispose();
	});
	BinaryReader reader(cmd.path);
	if (!reader) {
		cmd.sp->data = nullptr;
		cmd.sp->size = 0;
		return;
	}
	cmd.sp->size = reader.GetLength();
	cmd.sp->data = vengine_malloc(cmd.sp->size);
	reader.Read(cmd.sp->data, cmd.sp->size);
}
VENGINE_UNITY_EXTERN void dskInit() {
	data.New();
}
VENGINE_UNITY_EXTERN void dskDestroy() {
	data.Delete();
}
VENGINE_UNITY_EXTERN void dskDestroyCmd(DataSpan* sp) {
	sp->Dispose();
}
struct AddCommand {
	char const* pathName;
	uint64 pathNameLen;
	DataSpan* result;
};
VENGINE_UNITY_EXTERN void dskAddCommands(AddCommand* cmd, uint64 count) {
	auto sp = new DataSpan();
	for (auto&& c : vstd::ptr_range(cmd, count)) {
		data->queue.Push(BinaryDiskRead{
			.sp = sp,
			.path = vstd::string(vstd::string_view(c.pathName, c.pathNameLen))});
	}
	data->thread.ExecuteNext();
	cmd->result = sp;
}
VENGINE_UNITY_EXTERN void dskComplete(){
	data->thread.Complete();
}
VENGINE_UNITY_EXTERN bool dskReadLoadState(DataSpan* sp) {
	return sp->loadState.load();
}
}// namespace toolhub::dsk