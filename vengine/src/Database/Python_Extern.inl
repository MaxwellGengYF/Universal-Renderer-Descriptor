#pragma once
#include <Database/SimpleBinaryJson.h>
#include <Database/SimpleJsonValue.h>
#include <Utility/BinaryReader.h>
#include <Graphics/Struct.h>
namespace toolhub::db {
struct PyStruct {
	union {
		vstd::StackObject<int64> intValue;
		vstd::StackObject<double> floatValue;
		vstd::StackObject<Span<char>> strv;
		vstd::StackObject<vstd::Guid> guid;
		vstd::StackObject<IJsonDict*> dict;
		vstd::StackObject<IJsonArray*> arr;
	};
	PyStruct() {}
};
static PyStruct pyStruct;
static vstd::vector<vbyte> pyDataCache;
VENGINE_UNITY_EXTERN uint64 pydb_get_data_len() {

	return pyDataCache.size();
}
VENGINE_UNITY_EXTERN void pydb_cpy_data(vbyte* ptr) {
	memcpy(ptr, pyDataCache.data(), pyDataCache.size());
}
VENGINE_UNITY_EXTERN IJsonDatabase* pydb_get_new() {
	return new SimpleBinaryJson();
}
VENGINE_UNITY_EXTERN void pydb_dispose(IJsonDatabase* p) {
	delete static_cast<SimpleBinaryJson*>(p);
}
VENGINE_UNITY_EXTERN void pydb_dict_dispose(SimpleJsonValueDict* p) {
	p->Dispose();
}
VENGINE_UNITY_EXTERN void pydb_arr_dispose(SimpleJsonValueArray* p) {
	p->Dispose();
}
VENGINE_UNITY_EXTERN void* pydb_get_rootnode(IJsonDatabase* db) {
	return db->GetRootNode();
}
VENGINE_UNITY_EXTERN void* pydb_create_dict(IJsonDatabase* db) {
	return static_cast<SimpleBinaryJson*>(db)->CreateDict_Nake();
}
VENGINE_UNITY_EXTERN void* pydb_create_array(IJsonDatabase* db) {
	return static_cast<SimpleBinaryJson*>(db)->CreateArray_Nake();
}
VENGINE_UNITY_EXTERN void* pydb_dict_create_dict(SimpleJsonValueDict* db) {
	return db->MGetDB()->CreateDict_Nake();
}
VENGINE_UNITY_EXTERN void* pydb_dict_create_array(SimpleJsonValueDict* db) {
	return db->MGetDB()->CreateArray_Nake();
}
VENGINE_UNITY_EXTERN void* pydb_arr_create_dict(SimpleJsonValueArray* db) {
	return db->MGetDB()->CreateDict_Nake();
}
VENGINE_UNITY_EXTERN void* pydb_arr_create_array(SimpleJsonValueArray* db) {
	return db->MGetDB()->CreateArray_Nake();
}
template<typename T>
void TSer(T* v) {
	pyDataCache = v->Serialize();
}
template<typename T>
bool TDeser(T* v, vstd::span<vbyte const> data, bool clearLast) {
	return v->Read(data, clearLast);
}
static vstd::optional<vstd::string> printableStr;
template<typename T>
void TFormattedPrint(T* v) {
	printableStr = v->FormattedPrint();
	pyStruct.strv.New(*printableStr);
}
template<typename T>
void TPrint(T* v) {
	printableStr = v->Print();
	pyStruct.strv.New(*printableStr);
}
template<typename T>
bool TParse(T* v, char const* str, uint64 strLen, bool clearLast) {
	auto error = v->Parse(Span<char>(str, strLen), clearLast);
	if (error) {
		std::cout << error->message;
	}
	return !error;
}
template<typename T>
bool TParseYaml(T* v, char const* str, uint64 strLen, bool clearLast) {
	auto error = v->ParseYaml(Span<char>(str, strLen), clearLast);
	if (error) {
		std::cout << error->message;
	}
	return !error;
}
VENGINE_UNITY_EXTERN void py_clear_printstr() {
	printableStr.Delete();
}
VENGINE_UNITY_EXTERN void pydb_dict_ser(SimpleJsonValueDict* v) {
	TSer(v);
}
VENGINE_UNITY_EXTERN void pydb_arr_ser(SimpleJsonValueArray* v) {
	TSer(v);
}
VENGINE_UNITY_EXTERN bool pydb_dict_deser(SimpleJsonValueDict* v, vbyte const* ptr, uint64 size, bool clearLast) {
	return TDeser(v, vstd::span<vbyte const>(ptr, size), clearLast);
}
VENGINE_UNITY_EXTERN bool pydb_arr_deser(SimpleJsonValueArray* v, vbyte const* ptr, uint64 size, bool clearLast) {
	return TDeser(v, vstd::span<vbyte const>(ptr, size), clearLast);
}
VENGINE_UNITY_EXTERN void pydb_dict_print(SimpleJsonValueDict* v) {
	TPrint(v);
}
VENGINE_UNITY_EXTERN void pydb_arr_print(SimpleJsonValueArray* v) {
	TPrint(v);
}
VENGINE_UNITY_EXTERN void pydb_dict_print_formatted(SimpleJsonValueDict* v) {
	TFormattedPrint(v);
}
VENGINE_UNITY_EXTERN void pydb_arr_print_formatted(SimpleJsonValueArray* v) {
	TFormattedPrint(v);
}
VENGINE_UNITY_EXTERN bool pydb_dict_parse(SimpleJsonValueDict* v, char const* str, uint64 strLen, bool clearLast) {
	return TParse(v, str, strLen, clearLast);
}
VENGINE_UNITY_EXTERN bool pydb_dict_parse_yaml(SimpleJsonValueDict* v, char const* str, uint64 strLen, bool clearLast) {
	return TParseYaml(v, str, strLen, clearLast);
}
VENGINE_UNITY_EXTERN void pydb_dict_print_yaml(SimpleJsonValueDict* v) {
	printableStr = v->PrintYaml();
	pyStruct.strv.New(*printableStr);
}
VENGINE_UNITY_EXTERN bool pydb_arr_parse(SimpleJsonValueArray* v, char const* str, uint64 strLen, bool clearLast) {
	return TParse(v, str, strLen, clearLast);
}

VENGINE_UNITY_EXTERN void pydb_dict_set(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType,
	void* valuePtr,
	CSharpValueType valueType) {
	dict->Set(GetCSharpKey(keyPtr, keyType), GetCSharpWriteValue(valuePtr, valueType));
}

VENGINE_UNITY_EXTERN CSharpValueType pydb_dict_get_variant(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType) {
	auto value = dict->Get(GetCSharpKey(keyPtr, keyType));
	return SetCSharpReadValue(&pyStruct, value);
}
VENGINE_UNITY_EXTERN void pydb_dict_remove(SimpleJsonValueDict* dict, void* keyPtr, CSharpKeyType keyType) {
	dict->Remove(GetCSharpKey(keyPtr, keyType));
}

VENGINE_UNITY_EXTERN uint pydb_dict_len(SimpleJsonValueDict* dict) { return dict->Length(); }
VENGINE_UNITY_EXTERN uint64 pydb_dict_itebegin(SimpleJsonValueDict* dict) {
	auto ite = dict->vars.begin();
	return *reinterpret_cast<uint64 const*>(&ite);
}
VENGINE_UNITY_EXTERN bool pydb_dict_iteend(SimpleJsonValueDict* dict, DictIterator end) { return (end == dict->vars.end()); }
VENGINE_UNITY_EXTERN uint64 pydb_dict_ite_next(DictIterator end) {
	end++;
	return *reinterpret_cast<uint64 const*>(&end);
}
VENGINE_UNITY_EXTERN CSharpValueType pydb_dict_ite_get_variant(DictIterator ite) {
	return SetCSharpReadValue(&pyStruct, ite->second.GetVariant());
}
VENGINE_UNITY_EXTERN CSharpKeyType pydb_dict_ite_getkey_variant(DictIterator ite) {
	return SetCSharpKey(&pyStruct, ite->first.GetKey());
}
VENGINE_UNITY_EXTERN void pydb_dict_reset(
	SimpleJsonValueDict* dict) {
	dict->Reset();
}
////////////////// Array Area
VENGINE_UNITY_EXTERN void pydb_arr_reset(
	SimpleJsonValueArray* arr) {
	arr->Reset();
}
VENGINE_UNITY_EXTERN uint pydb_arr_len(SimpleJsonValueArray* arr) {
	return arr->Length();
}
VENGINE_UNITY_EXTERN CSharpValueType pydb_arr_get_value_variant(SimpleJsonValueArray* arr, uint index) {
	return SetCSharpReadValue(&pyStruct, arr->Get(index));
}
VENGINE_UNITY_EXTERN void pydb_arr_set_value(SimpleJsonValueArray* arr, uint index, void* valuePtr, CSharpValueType valueType) {
	arr->Set(index, GetCSharpWriteValue(valuePtr, valueType));
}
VENGINE_UNITY_EXTERN void pydb_arr_add_value(SimpleJsonValueArray* arr, void* valuePtr, CSharpValueType valueType) {
	arr->Add(GetCSharpWriteValue(valuePtr, valueType));
}
VENGINE_UNITY_EXTERN void pydb_arr_remove(SimpleJsonValueArray* arr, uint index) {
	arr->Remove(index);
}

VENGINE_UNITY_EXTERN uint64 pydb_arr_itebegin(SimpleJsonValueArray* arr) {
	auto a = arr->arr.begin();
	return *reinterpret_cast<uint64 const*>(&a);
}
VENGINE_UNITY_EXTERN bool pydb_arr_iteend(SimpleJsonValueArray* arr, ArrayIterator ptr) {
	return (ptr == arr->arr.end());
}
VENGINE_UNITY_EXTERN uint64 pydb_arr_ite_next(ArrayIterator ptr) {
	(ptr)++;
	return *reinterpret_cast<uint64 const*>(&ptr);
}
VENGINE_UNITY_EXTERN CSharpValueType pydb_arr_ite_get_variant(ArrayIterator ite) {
	return SetCSharpReadValue(&pyStruct, ite->GetVariant());
}
VENGINE_UNITY_EXTERN uint64 py_get_str_size() {
	return pyStruct.strv->count;
}
VENGINE_UNITY_EXTERN void py_get_chars(char* ptr) {
	memcpy(ptr, pyStruct.strv->ptr, pyStruct.strv->count);
}
VENGINE_UNITY_EXTERN void py_bind_64(int64* ptr) {
	*ptr = *pyStruct.intValue;
}
VENGINE_UNITY_EXTERN void py_bind_128(int64* ptr) {
	int64* otherPtr = reinterpret_cast<int64*>(&pyStruct);
	ptr[0] = otherPtr[0];
	ptr[1] = otherPtr[1];
}

VENGINE_UNITY_EXTERN void py_bind_strview(
	Span<char>* strv,
	char* ptr,
	uint64 len) { *strv = Span<char>(ptr, len); }
}// namespace toolhub::db