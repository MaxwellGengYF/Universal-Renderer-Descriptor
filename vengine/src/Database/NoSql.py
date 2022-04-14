from ctypes import *
import enum
from struct import *
from enum import IntEnum
import uuid
import os
import Database as db
dll = cdll.LoadLibrary(os.path.dirname(os.path.realpath(__file__)) + "\\VEngine_Database.dll")
dll.pynosql_create_db.restype = c_uint64
dll.pynosql_get_int_value.restype = c_int64
dll.pynosql_get_double_value.restype = c_double
dll.pynosql_get_value_type.restype = c_uint8
dll.pynosql_map_end.restype = c_bool
dll.pynosql_ele_end.restype = c_bool
dll.pynosql_findone.restype = c_uint64
dll.pynosql_findall.restype = c_uint64
dll.pynosql_get_key_type.restype = c_uint8
dll.pynosql_get_int_key.restype = c_int64

class CompareFlag(IntEnum):
    Less = 0
    LessEqual = 1
    Equal = 2
    GreaterEqual = 3
    Greater = 4
class SelectLogic(IntEnum):
    Or = 0
    And = 1

class SearchResultHandle:
    ptr:int
    def __init__(self, ptr:int):
        self.ptr = ptr
    def __del__(self):
        dll.pynosql_dispose_handle(c_uint64(self.ptr))
    def __iter__(self):
        ptr = c_uint64(self.ptr)
        dll.pynosql_begin_map(ptr)
        return self
    def __next__(self):
        ptr = c_uint64(self.ptr)
        if dll.pynosql_map_end(ptr):
            raise StopIteration
        result = {}
        dll.pynosql_begin_ele(ptr)
        while(not dll.pynosql_ele_end(ptr)):
            result[_getKey(ptr)] = _getResult(ptr)
            dll.pynosql_next_ele(ptr)
        dll.pynosql_next_map(ptr)
        return result

def _addKey(key):
    if type(key) == int:
        dll.pynosql_add_key_int(c_int64(key))
    elif type(key) == db.Guid:
        byts = pack("QQ", key.data0, key.data1)
        dll.pynosql_add_key_guid(c_char_p(byts))
    elif type(key) == str:
        strBytes = key.encode("ascii")
        dll.pynosql_add_key_str(c_char_p(strBytes), c_uint64(len(strBytes)))
    else:
        raise Exception("Illegal type")

def _addValue(key):
    if type(key) == int:
        dll.pynosql_add_value_int(c_int64(key))
    elif type(key) == float:
        dll.pynosql_add_value_double(c_double(key))
    elif type(key) == bool:
        dll.pynosql_add_value_bool(c_bool(key))
    elif type(key) == db.Guid:
        byts = pack("QQ", key.data0, key.data1)
        dll.pynosql_add_value_guid(c_char_p(byts))
    elif type(key) == str:
        strBytes = key.encode("ascii")
        dll.pynosql_add_value_str(c_char_p(strBytes), c_uint64(len(strBytes)))
    elif key == None:
        dll.pynosql_add_value_null()
    else:
        raise Exception("Illegal type")

    
def _getResult(ptr:c_uint64):
    valueType = int(dll.pynosql_get_value_type(ptr))
    if valueType == 0:
        return int(dll.pynosql_get_int_value(ptr))
    elif valueType == 1:
        return float(dll.pynosql_get_double_value(ptr))
    elif valueType == 2:
        resBytes = bytes(dll.pynosql_get_int_value(ptr))
        dll.pynosql_get_str(ptr, c_char_p(resBytes))
        return resBytes.decode("ascii")
    elif valueType == 3:
        bs = bytes(16)
        dll.pynosql_get_guid(ptr, c_char_p(bs))
        return db.Guid(unpack("QQ", bs))
    elif valueType == 4:
        if int(dll.pynosql_get_int_value(ptr)) == 0:
            return False
        return True
    return None
def _getKey(ptr:c_uint64):
    keyType = int(dll.pynosql_get_key_type(ptr))
    if keyType == 0:
        return int(dll.pynosql_get_int_key(ptr))
    elif keyType == 1:
        resBytes = bytes(dll.pynosql_get_int_key(ptr))
        dll.pynosql_get_str_key(ptr, c_char_p(resBytes))
        return resBytes.decode("ascii")
    elif keyType == 2:
        bs = bytes(16)
        dll.pynosql_get_guid_key(ptr, c_char_p(bs))
        return db.Guid(unpack("QQ", bs))
def _addCondition(key, value):
    _addKey(key)
    _addValue(value[0])
    dll.pynosql_add_flag(c_uint8(int(value[1])))
class NoSqlDatabase:
    dbInst:int
    def __init__(self, name:str):
        nBytes = name.encode("ascii")
        self.dbInst = dll.pynosql_create_db(c_char_p(nBytes), c_uint64(len(nBytes)))
    def __del__(self):
        dll.pynosql_dispose_db(c_uint64(self.dbInst))


    def FindOne(self, conditions:dict, deleteAfterFind:bool = False, logic:SelectLogic = SelectLogic.And):
        dll.pynosql_clear_condition()
        for i in conditions:
            _addCondition(i, conditions[i])
        return SearchResultHandle(dll.pynosql_findone(c_uint64(self.dbInst), c_bool(deleteAfterFind), c_uint8(logic)))
        
    def FindAll(self, conditions:dict, deleteAfterFind:bool = False, logic:SelectLogic = SelectLogic.And):
        dll.pynosql_clear_condition()
        for i in conditions:
            _addCondition(i, conditions[i])
        return SearchResultHandle(dll.pynosql_findall(c_uint64(self.dbInst), c_bool(deleteAfterFind), c_uint8(logic)))
            
    def DeleteAll(self, conditions:dict, logic:SelectLogic = SelectLogic.And):
        dll.pynosql_clear_condition()
        for i in conditions:
            _addCondition(i, conditions[i])
        dll.pynosql_deleteall(c_uint64(self.dbInst), c_uint8(logic))

    def AddNode(self, keyValues:dict):
        dll.pynosql_clear_condition()
        for i in keyValues:
            _addKey(i)
            _addValue(keyValues[i])
        dll.pynosql_addall(c_uint64(self.dbInst))
    def Clear(self):
        dll.pynosql_clear_db(c_uint64(self.dbInst))
    def Print(self):
        dll.pynosql_print(c_uint64(self.dbInst))