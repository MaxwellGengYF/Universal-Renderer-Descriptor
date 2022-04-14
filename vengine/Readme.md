# Tool Hub

## VSTL API:
## 使用VSTL的原因：
* 方便统一的内存管理
* 定制实现高性能基础库
* 实现更多动态抽象
### VEngine_AllocType: 
* 在AllocateType.h中定义，决定内存分配方法，Default为默认malloc, free，VEngine为自定义分配方法，目前使用mimalloc作为分配库。
### DynamicDLL.h:
* 动态加载DLL的方法
### functional.h: 
* 代替std::function，功能基本一致
### vector.h:
* 代替std::vector 功能基本一致
### vstd::string, std::string_view：
* 代替std::string, std::string_view，功能基本一致，string_view实现operator ""sv
### DynamicLink.h:
* 全局动态加载可闭包的函数类，在任意.cpp文件空白处使用VENGINE_LINK_CLASS定义Runnable，或VENGINE_LINK_FUNC定义函数，在其他任意动态库或编译单元中，均可以使用TryGetFunction获取，Debug宏启动状态下，有类型对比检测。
### HashMap.h:
* 代替std::unordered_map，拥有更高的添加，删除，查找性能，且遍历性能为O(n)。关键API：
* Emplace: 若已存在，则不做任何操作，若不存在，则添加新元素，返回索引器
* ForceEmplace：若已存在，覆盖，若不存在，添加新元素，返回索引器
* Remove：删除元素
* Find：查找元素，返回索引器
### LockFreeArrayQueue.h：Ring Queue， Push Pop
### Pool.h: 
* 对象池，New, Delete分配回收对象，New_Lock, Delete_Lock加入锁
### VObject.h: 
* 可实现自定义caster，在cpp文件空白处以以下格式定义REGIST_VOBJ_CLASS(当前类名，接口类1，接口类2)，在构建函数中调用SET_VOBJ_CLASS(当前类名)，当前类即可使用GetInterface动态Cast到已注册的类。
* ObjectPtr代替std::shared_ptr，用法无太大区别，通过MakeObjectPtr实例化变量。

## Database:
* Example: Database/DatabaseExample.h