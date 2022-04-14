#pragma once
#include <Database/SimpleBinaryJson.h>
#include <Database/SimpleJsonValue.h>
#include <Utility/BinaryReader.h>
#include <Graphics/Struct.h>
namespace toolhub::db {
template<typename T>
void DBParse(T* db, Span<char> strv, vstd::funcPtr_t<void(Span<char>)> errorCallback, bool clearLast) {
	auto errorMsg = db->Parse(strv, clearLast);
	if (errorMsg) {
		errorCallback(errorMsg->message);
	} else {
		errorCallback(Span<char>(nullptr, 0));
	}
}

VENGINE_UNITY_EXTERN void db_get_new(SimpleBinaryJson** pp) {
	*pp = new SimpleBinaryJson();
}
VENGINE_UNITY_EXTERN void db_dispose(SimpleBinaryJson* p) {
	delete p;
}
VENGINE_UNITY_EXTERN void db_compile_from_py(SimpleBinaryJson* p, wchar_t const* datas, int64 sz, bool* result) {
	vstd::string str;
	str.resize(sz);
	for (auto i : vstd::range(sz)) {
		str[i] = datas[i];
	}
	*result = p->CompileFromPython(str.data());
}
VENGINE_UNITY_EXTERN void db_get_rootnode(SimpleBinaryJson* db, SimpleJsonValueDict** pp) {
	*pp = static_cast<SimpleJsonValueDict*>(db->GetRootNode());
}
VENGINE_UNITY_EXTERN void db_dict_print(SimpleJsonValueDict* db, vstd::funcPtr_t<void(Span<char>)> callback) {
	auto str = db->Print();
	callback(str);
}
VENGINE_UNITY_EXTERN void db_dict_print_yaml(SimpleJsonValueDict* db, vstd::funcPtr_t<void(Span<char>)> callback) {
	auto str = db->PrintYaml();
	callback(str);
}
VENGINE_UNITY_EXTERN void db_arr_print(SimpleJsonValueArray* db, vstd::funcPtr_t<void(char const*, uint64)> callback) {
	auto str = db->Print();
	callback(str.data(), str.size());
}
VENGINE_UNITY_EXTERN void db_dict_format_print(SimpleJsonValueDict* db, vstd::funcPtr_t<void(Span<char>)> callback) {
	auto str = db->FormattedPrint();
	callback(str);
}
VENGINE_UNITY_EXTERN void db_arr_format_print(SimpleJsonValueArray* db, vstd::funcPtr_t<void(char const*, uint64)> callback) {
	auto str = db->FormattedPrint();
	callback(str.data(), str.size());
}
VENGINE_UNITY_EXTERN void db_create_dict(SimpleBinaryJson* db, SimpleJsonValueDict** pp) {
	*pp = db->CreateDict_Nake();
}
VENGINE_UNITY_EXTERN void db_create_array(SimpleBinaryJson* db, SimpleJsonValueArray** pp) {
	*pp = db->CreateArray_Nake();
}

VENGINE_UNITY_EXTERN void db_arr_ser(SimpleJsonValueArray* db, vstd::funcPtr_t<void(vbyte*, uint64)> callback) {
	auto vec = db->Serialize();
	callback(vec.data(), vec.size());
}
VENGINE_UNITY_EXTERN void db_dict_ser(SimpleJsonValueDict* db, vstd::funcPtr_t<void(vbyte*, uint64)> callback) {
	auto vec = db->Serialize();
	callback(vec.data(), vec.size());
}
VENGINE_UNITY_EXTERN void db_arr_deser(SimpleJsonValueArray* db, vbyte* ptr, uint64 len, bool* success, bool clearLast) {
	*success = db->Read(vstd::span<vbyte const>(ptr, len), clearLast);
}
VENGINE_UNITY_EXTERN void db_dict_deser(SimpleJsonValueDict* db, vbyte* ptr, uint64 len, bool* success, bool clearLast) {
	*success = db->Read(vstd::span<vbyte const>(ptr, len), clearLast);
}

VENGINE_UNITY_EXTERN void db_dispose_arr(SimpleJsonValueArray* p) {
	p->Dispose();
}

////////////////// Dict Area

VENGINE_UNITY_EXTERN void db_dispose_dict(SimpleJsonValueDict* ptr) {
	ptr->Dispose();
}
VENGINE_UNITY_EXTERN void db_dict_set(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType,
	void* valuePtr,
	CSharpValueType valueType) {
	dict->Set(GetCSharpKey(keyPtr, keyType), GetCSharpWriteValue(valuePtr, valueType));
}
VENGINE_UNITY_EXTERN void db_dict_tryset(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType,
	void* valuePtr,
	CSharpValueType valueType,
	bool* isTry) {
	Key key;
	WriteJsonVariant value;
	*isTry = false;
	dict->TrySet(GetCSharpKey(keyPtr, keyType), [&]() {
		*isTry = true;
		return GetCSharpWriteValue(valuePtr, valueType);
	});
}
VENGINE_UNITY_EXTERN void db_dict_get(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType,
	CSharpValueType targetValueType,
	void* valuePtr) {
	SetCSharpReadValue(valuePtr, targetValueType, dict->Get(GetCSharpKey(keyPtr, keyType)));
}
VENGINE_UNITY_EXTERN void db_dict_get_variant(
	SimpleJsonValueDict* dict,
	void* keyPtr,
	CSharpKeyType keyType,
	CSharpValueType* targetValueType,
	void* valuePtr) {
	auto value = dict->Get(GetCSharpKey(keyPtr, keyType));
	*targetValueType = SetCSharpReadValue(valuePtr, value);
}
VENGINE_UNITY_EXTERN void db_dict_remove(SimpleJsonValueDict* dict, void* keyPtr, CSharpKeyType keyType) {
	dict->Remove(GetCSharpKey(keyPtr, keyType));
}
VENGINE_UNITY_EXTERN void db_dict_len(SimpleJsonValueDict* dict, int32* sz) { *sz = dict->Length(); }
VENGINE_UNITY_EXTERN void db_dict_itebegin(SimpleJsonValueDict* dict, DictIterator* ptr) { *ptr = dict->vars.begin(); }
VENGINE_UNITY_EXTERN void db_dict_iteend(SimpleJsonValueDict* dict, DictIterator* end, bool* result) { *result = (*end == dict->vars.end()); }
VENGINE_UNITY_EXTERN void db_dict_ite_next(DictIterator* end) { (*end)++; }
VENGINE_UNITY_EXTERN void db_dict_ite_get(DictIterator ite, void* valuePtr, CSharpValueType valueType) {
	SetCSharpReadValue(valuePtr, valueType, ite->second.GetVariant());
}
VENGINE_UNITY_EXTERN void db_dict_ite_get_variant(DictIterator ite, void* valuePtr, CSharpValueType* valueType) {
	*valueType = SetCSharpReadValue(valuePtr, ite->second.GetVariant());
}

VENGINE_UNITY_EXTERN void db_dict_ite_getkey(DictIterator ite, void* keyPtr, CSharpKeyType keyType) {
	SetCSharpKey(keyPtr, keyType, ite->first.GetKey());
}
VENGINE_UNITY_EXTERN void db_dict_ite_getkey_variant(DictIterator ite, void* keyPtr, CSharpKeyType* keyType) {
	*keyType = SetCSharpKey(keyPtr, ite->first.GetKey());
}
VENGINE_UNITY_EXTERN void db_dict_reset(
	SimpleJsonValueDict* dict) {
	dict->Reset();
}
VENGINE_UNITY_EXTERN void db_dict_parse(SimpleJsonValueDict* db, Span<char> strv, vstd::funcPtr_t<void(Span<char>)> errorCallback, bool clearLast) {
	DBParse(db, strv, errorCallback, clearLast);
}
VENGINE_UNITY_EXTERN void db_dict_parse_yaml(SimpleJsonValueDict* db, Span<char> strv, vstd::funcPtr_t<void(Span<char>)> errorCallback, bool clearLast) {
	auto errorMsg = db->ParseYaml(strv, clearLast);
	if (errorMsg) {
		errorCallback(errorMsg->message);
	} else {
		errorCallback(Span<char>(nullptr, 0));
	}
}
////////////////// Array Area
VENGINE_UNITY_EXTERN void db_arr_reset(
	SimpleJsonValueArray* arr) {
	arr->Reset();
}
VENGINE_UNITY_EXTERN void db_arr_len(SimpleJsonValueArray* arr, int32* sz) {
	*sz = arr->Length();
}
VENGINE_UNITY_EXTERN void db_arr_get_value(SimpleJsonValueArray* arr, int32 index, void* valuePtr, CSharpValueType valueType) {
	SetCSharpReadValue(valuePtr, valueType, arr->Get(index));
}
VENGINE_UNITY_EXTERN void db_arr_get_value_variant(SimpleJsonValueArray* arr, int32 index, void* valuePtr, CSharpValueType* valueType) {
	*valueType = SetCSharpReadValue(valuePtr, arr->Get(index));
}
VENGINE_UNITY_EXTERN void db_arr_set_value(SimpleJsonValueArray* arr, int32 index, void* valuePtr, CSharpValueType valueType) {
	arr->Set(index, GetCSharpWriteValue(valuePtr, valueType));
}
VENGINE_UNITY_EXTERN void db_arr_add_value(SimpleJsonValueArray* arr, void* valuePtr, CSharpValueType valueType) {
	arr->Add(GetCSharpWriteValue(valuePtr, valueType));
}
VENGINE_UNITY_EXTERN void db_arr_remove(SimpleJsonValueArray* arr, int32 index) {
	arr->Remove(index);
}
VENGINE_UNITY_EXTERN void db_arr_itebegin(SimpleJsonValueArray* arr, ArrayIterator* ptr) {
	*ptr = arr->arr.begin();
}
VENGINE_UNITY_EXTERN void db_arr_iteend(SimpleJsonValueArray* arr, ArrayIterator* ptr, bool* result) {
	*result = (*ptr == arr->arr.end());
}
VENGINE_UNITY_EXTERN void db_arr_ite_next(SimpleJsonValueArray* arr, ArrayIterator* ptr) {
	(*ptr)++;
}
VENGINE_UNITY_EXTERN void db_arr_ite_get(ArrayIterator ite, void* valuePtr, CSharpValueType valueType) {
	SetCSharpReadValue(valuePtr, valueType, ite->GetVariant());
}
VENGINE_UNITY_EXTERN void db_arr_ite_get_variant(ArrayIterator ite, void* valuePtr, CSharpValueType* valueType) {
	*valueType = SetCSharpReadValue(valuePtr, ite->GetVariant());
}
VENGINE_UNITY_EXTERN void db_arr_parse(SimpleJsonValueArray* db, Span<char> strv, vstd::funcPtr_t<void(Span<char>)> errorCallback, bool clearLast) {
	DBParse(db, strv, errorCallback, clearLast);
}
}// namespace toolhub::db