import os
f = open("builtin_func.txt", "r")
lines = f.readlines()
f.close()
results = []
for ss in lines:
	if len(ss) <= 2:
		continue
	curS = ""
	for c in ss:
		if c == '\t' or c == ' ':
			continue
		if c == ',':
			break
		curS += c
	results.append(curS.lower())
#for i in results:
#	print("static bool " + i + "(StatementName& s, StatementName::FuncCall const& funcPack){ return s.UnaryOpCall(UnaryOp::" + i.upper() + ", funcPack); }")
print("")
print("")
print("")
for i in results:
	print("REGIST_NAME(binaryMap, BinaryOp::" + i.upper() + ", " + i + ");")
# print func
os.system("pause")