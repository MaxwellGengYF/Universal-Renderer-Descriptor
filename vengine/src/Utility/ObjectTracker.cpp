

#include <Utility/ObjectTracker.h>

namespace vstd {
static vstd::Pool<ObjectTrackFlag_Impl, VEngine_AllocType::VEngine, true> objTrackerAllocType(256, false);
static vstd::spin_mutex objTrackerMtx;
ObjectTrackFlag_Impl::ObjectTrackFlag_Impl() {
	refCount = 1;
}

ObjectTrackFlag_Impl* ObjectTrackFlag_Impl::Allocate() {
	return objTrackerAllocType.New_Lock(objTrackerMtx);
}
void ObjectTrackFlag_Impl::DeAllocate(ObjectTrackFlag_Impl* ptr) {
	objTrackerAllocType.Delete_Lock(objTrackerMtx, ptr);
}
}// namespace vstd