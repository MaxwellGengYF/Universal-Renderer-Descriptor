from ctypes import *
from struct import *
import uuid
import os
dll = cdll.LoadLibrary(os.path.dirname(
    os.path.realpath(__file__)) + "\\VEngine_Database.dll")

dll.py_get_str_size.restype = c_uint64
dll.pydb_get_new.restype = c_uint64
dll.db_get_curobj.restype = c_uint64
dll.pydb_get_rootnode.restype = c_void_p
dll.pydb_create_dict.restype = c_void_p
dll.pydb_create_array.restype = c_void_p
dll.pydb_dict_create_dict.restype = c_void_p
dll.pydb_dict_create_array.restype = c_void_p
dll.pydb_arr_create_dict.restype = c_void_p
dll.pydb_arr_create_array.restype = c_void_p
dll.pydb_dict_get_variant.restype = c_uint32
dll.pydb_dict_len.restype = c_uint32
dll.pydb_dict_itebegin.restype = c_uint64
dll.pydb_dict_iteend.restype = c_bool
dll.pydb_dict_ite_next.restype = c_uint64
dll.pydb_dict_ite_get_variant.restype = c_uint32
dll.pydb_dict_ite_getkey_variant.restype = c_uint32
dll.pydb_arr_get_value_variant.restype = c_uint32
dll.pydb_arr_itebegin.restype = c_uint64
dll.pydb_arr_iteend.restype = c_bool
dll.pydb_arr_ite_next.restype = c_uint64
dll.pydb_arr_ite_get_variant.restype = c_uint32
dll.pydb_dict_deser.restype = c_bool
dll.pydb_arr_deser.restype = c_bool
dll.pydb_dict_parse.restype = c_bool
dll.pydb_dict_parse_yaml.restype = c_bool
dll.pydb_arr_parse.restype = c_bool
dll.pydb_get_data_len.restype = c_uint64
gTempBytes = []


b64 = bytes(8)
b128 = bytes(16)


class Null:
    pass

def GetText(path:str)->str:
    f = open(path, 'r')
    s = f.read()
    f.close()
    return s

class Guid:
    data0: int
    data1: int

    def __str__(self):
        h0 = hex(self.data0)
        h1 = hex(self.data1)
        return (h0[2:len(h0)] + h1[2:len(h1)]).upper()

    def __init__(self, tp):
        self.data0 = tp[0]
        self.data1 = tp[1]


def GetGuid():
    id = uuid.uuid4()
    ss = id.hex.upper()
    num0 = int(ss[0:16], base=16)
    num1 = int(ss[16:32], base=16)
    return Guid((num0, num1))


def GetValue(index, parent):
    if index == 0:
        dll.py_bind_64(c_char_p(b64))
        return unpack("q", b64)[0]
    elif index == 1:
        dll.py_bind_64(c_char_p(b64))
        return unpack("d", b64)[0]
    elif index == 2:
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")
    elif index == 3:
        dll.py_bind_64(c_char_p(b64))
        return SerializeDict(unpack("Q", b64)[0], parent)
    elif index == 4:
        dll.py_bind_64(c_char_p(b64))
        return SerializeArray(unpack("Q", b64)[0], parent)
    elif index == 5:
        dll.py_bind_128(c_char_p(b128))
        return Guid(unpack("QQ", b128))
    elif index == 6:
        dll.py_bind_64(c_char_p(b64))
        return unpack("q", b64)[0] != 0
    elif index == 7:
        return Null()
    else:
        return None


def GetKey(index):
    if index == 0:
        dll.py_bind_64(c_char_p(b64))
        return unpack("q", b64)[0]
    elif index == 1:
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")
    elif index == 2:
        dll.py_bind_128(c_char_p(b128))
        return Guid(unpack("QQ", b128))
    else:
        return None


def SetValue(value, gcList: list):
    if type(value) == int:
        return pack("q", value), 0
    elif type(value) == float:
        return pack("d", value), 1
    elif type(value) == str:
        bs = bytes(16)
        p = value.encode("ascii")
        gcList.append(p)
        dll.py_bind_strview(c_char_p(bs), c_char_p(p),
                            c_uint64(len(value)))
        return bs, 2
    elif type(value) == SerializeDict:
        if not value.containPtr:
            raise Exception("Invalid dict!")
        value.containPtr = False
        return pack("q", value.data), 3
    elif type(value) == SerializeArray:
        if not value.containPtr:
            raise Exception("Invalid array!")
        value.containPtr = False
        return pack("q", value.data), 4
    elif type(value) == Guid:
        return pack("QQ", value.data0, value.data1), 5
    elif type(value) == bool:
        if value:
            return pack("q", 1), 6
        else:
            return pack("q", 0), 6
    elif type(value) == Null:
        return pack("q", 0), 7

    return None


def SetKey(value, gcList: list):
    if type(value) == int:
        return pack("q", value), 0
    elif type(value) == str:
        bs = bytes(16)
        p = value.encode("ascii")
        gcList.append(p)
        dll.py_bind_strview(c_char_p(bs), c_char_p(p),
                            c_uint64(len(value)))
        return bs, 1
    elif type(value) == Guid:
        return pack("QQ", value.data0, value.data1), 2
    return None


class SerializeDict:
    parent = None
    data: int
    ite: int
    containPtr: bool

    def __init__(self, data, parent, contain=False):
        self.data = data
        self.parent = parent
        self.containPtr = contain

    def __del__(self):
        if self.containPtr and self.data != 0:
            dll.pydb_dict_dispose(c_uint64(self.data))

    def Serialize(self):
        dll.pydb_dict_ser(c_uint64(self.data))
        serBytes = bytes(dll.pydb_get_data_len())
        dll.pydb_cpy_data(c_char_p(serBytes))
        return serBytes

    def DeSerialize(self, bytesValue: bytes, clearLast: bool):
        dll.pydb_dict_deser(c_uint64(self.data), c_char_p(
            bytesValue), c_uint64(len(bytesValue)), c_bool(clearLast))

    def Parse(self, sentence: str, clearLast: bool = True):
        p = sentence.encode("ascii")
        return dll.pydb_dict_parse(c_uint64(self.data),  c_char_p(p),
                                   c_uint64(len(sentence)), c_bool(clearLast))
    def ParseYaml(self, sentence: str, clearLast: bool = True):
        p = sentence.encode("ascii")
        return dll.pydb_dict_parse_yaml(c_uint64(self.data),  c_char_p(p),
                                   c_uint64(len(sentence)), c_bool(clearLast))

    def __str__(self):
        dll.pydb_dict_print(c_uint64(self.data))
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")

    def Print(self):
        dll.pydb_dict_print_formatted(c_uint64(self.data))
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")
    def PrintYaml(self):
        dll.pydb_dict_print_yaml(c_uint64(self.data))
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")

    def __add__(self, map):
        if type(map) == dict:
            for k in map:
                self[k] = map[k]
        elif type(map) == SerializeDict:
            for k, v in map:
                self[k] = v
        return self

    def __delitem__(self, key):
        keyResult = SetKey(key, gTempBytes)
        dll.pydb_dict_remove(c_uint64(self.data), keyResult[0], keyResult[1])
        gTempBytes.clear()

    def __getitem__(self, key):
        keyResult = SetKey(key, gTempBytes)
        v = GetValue(dll.pydb_dict_get_variant(
            c_uint64(self.data), keyResult[0], c_uint32(keyResult[1])), self.parent)
        gTempBytes.clear()
        return v

    def __getattr__(self, key):
        return self[key]

    def __setitem__(self, key, value):
        if type(value) == dict:
            newDict = SerializeDict(dll.pydb_dict_create_dict(
                c_uint64(self.data)), self.parent, True)
            for k in value:
                newDict[k] = value[k]
            self[key] = newDict
        elif type(value) == list:
            newArr = SerializeArray(dll.pydb_dict_create_array(
                c_uint64(self.data)), self.parent, True)
            for i in value:
                if (type(i) == dict):
                    newDict = SerializeDict(dll.pydb_arr_create_dict(
                        c_uint64(self.data)), self.parent, True)
                    for k in i:
                        newDict[k] = i[k]
                    newArr += newDict
                else:
                    newArr += i
            self[key] = newArr
        else:
            keyResult = SetKey(key, gTempBytes)
            valueResult = SetValue(value, gTempBytes)
            dll.pydb_dict_set(c_uint64(self.data), keyResult[0], c_uint32(
                keyResult[1]), valueResult[0], c_uint32(valueResult[1]))
            gTempBytes.clear()

    def __len__(self):
        return dll.pydb_dict_len(c_uint64(self.data))

    def Reset(self):
        dll.pydb_dict_reset(c_uint64(self.data))

    def __iter__(self):
        self.ite = dll.pydb_dict_itebegin(c_uint64(self.data))
        return self

    def __next__(self):
        if dll.pydb_dict_iteend(c_uint64(self.data), c_uint64(self.ite)):
            raise StopIteration
        value = GetValue(dll.pydb_dict_ite_get_variant(c_uint64(self.ite)), self.parent)
        key = GetKey(dll.pydb_dict_ite_getkey_variant(c_uint64(self.ite)))
        self.ite = dll.pydb_dict_ite_next(c_uint64(self.ite))
        return key, value


class SerializeArray:
    parent = None
    data: int
    ite: int
    containPtr: bool

    def __init__(self, data, parent, contain=False):
        self.data = data
        self.parent = parent
        self.containPtr = contain

    def __del__(self):
        if self.containPtr and self.data != 0:
            dll.pydb_arr_dispose(c_uint64(self.data))

    def Serialize(self):
        dll.pydb_arr_ser(c_uint64(self.data))
        serBytes = bytes(dll.pydb_get_data_len())
        dll.pydb_cpy_data(c_char_p(serBytes))
        return serBytes

    def DeSerialize(self, bytesValue: bytes, clearLast: bool):
        dll.pydb_arr_deser(c_uint64(self.data), c_char_p(
            bytesValue), c_uint64(len(bytesValue)), c_bool(clearLast))

    def Parse(self, sentence: str, clearLast: bool = True):
        p = sentence.encode("ascii")
        return dll.pydb_arr_parse(c_uint64(self.data),  c_char_p(p),
                                  c_uint64(len(sentence)), c_bool(clearLast))

    def __str__(self):
        dll.pydb_arr_print(c_uint64(self.data))
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")

    def Print(self):
        dll.pydb_arr_print_formatted(c_uint64(self.data))
        bs = bytes(dll.py_get_str_size())
        dll.py_get_chars(c_char_p(bs))
        return bs.decode("ascii")

    def __len__(self):
        return dll.pydb_arr_len(c_uint64(self.data))

    def Reset(self):
        dll.pydb_arr_reset(c_uint64(self.data))

    def __getitem__(self, index: int):
        return GetValue(dll.pydb_arr_get_value_variant(c_uint64(self.data), c_uint32(index)), self.parent)

    def __add__(self, value):
        if type(value) == list:
            for i in value:
                self += i
        elif type(value) == dict:
            subDict = self.parent.CreateDict()
            subDict += value
            self += subDict
        else:
            valueResult = SetValue(value, gTempBytes)
            dll.pydb_arr_add_value(c_uint64(self.data),
                                   valueResult[0], valueResult[1])
            gTempBytes.clear()
        return self

    def __setitem__(self, index: int, value):
        if type(value) == list:
            newArr = SerializeArray(dll.pydb_arr_create_array(
                c_uint64(self.data)), self.parent, True)
            for i in value:
                if (type(i) == dict):
                    newDict = SerializeDict(dll.pydb_arr_create_dict(
                        c_uint64(self.data)), self.parent, True)
                    for k in i:
                        newDict[k] = i[k]
                    newArr += newDict
                else:
                    newArr += i
            self[index] = newArr
        elif type(value) == dict:
            newDict = SerializeDict(dll.pydb_arr_create_dict(
                c_uint64(self.data)), self.parent, True)
            for k in value:
                newDict[k] = value[k]
            self[index] = newDict
        else:
            valueResult = SetValue(value, gTempBytes)
            dll.pydb_arr_set_value(c_uint64(self.data), c_uint32(
                index), valueResult[0], valueResult[1])
            gTempBytes.clear()

    def __delitem__(self, index: int):
        dll.pydb_arr_remove(c_uint64(self.data), c_uint32(index))

    def __iter__(self):
        self.ite = dll.pydb_arr_itebegin(c_uint64(self.data))
        return self

    def __next__(self):
        if dll.pydb_arr_iteend(c_uint64(self.data), c_uint64(self.ite)):
            raise StopIteration
        value = GetValue(dll.pydb_arr_ite_get_variant(c_uint64(self.ite)), self.parent)
        self.ite = dll.pydb_arr_ite_next(c_uint64(self.ite))
        return value


class SerializeObject:
    data: int
    isCreated: bool

    def __init__(self, createNew=True):
        self.isCreated = createNew
        if createNew:
            self.data = dll.pydb_get_new()
        else:
            self.data = dll.db_get_curobj()

    def __del__(self):
        if self.data != 0 and self.isCreated:
            dll.pydb_dispose(c_uint64(self.data))

    def GetRootNode(self):
        return SerializeDict(dll.pydb_get_rootnode(c_uint64(self.data)), self, False)

    def CreateDict(self):
        return SerializeDict(dll.pydb_create_dict(c_uint64(self.data)), self, True)

    def CreateArray(self):
        return SerializeArray(dll.pydb_create_array(c_uint64(self.data)), self, True)


def GetSerObj():
    return SerializeObject(False)
