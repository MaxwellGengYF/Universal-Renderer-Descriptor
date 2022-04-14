#pragma once

/*
A json structure database.
A simple example of interface
*/

#include <Database/DatabaseInclude.h>
#include <Database/IJsonDatabase.h>
#include <Database/IJsonObject.h>
#include <Common/VObject.h>
void TestDatabase() {
	using namespace toolhub::db;
	vstd::vector<vbyte> bin;
	{
		BinaryReader reader("test_text.txt");
		bin = reader.Read();
	}
	// ����DLL
	DllFactoryLoader<Database> dll("VEngine_Database.dll", "Database_GetFactory");
	// ��ȡ������
	auto dbBase = dll();
	auto db = vstd::create_unique(dbBase->CreateDatabase());
	// �����ֵ�����
	auto subDict = db->CreateDict();
	if (auto exp = subDict->Parse(vstd::string_view((char*)bin.begin(), (char*)bin.end()), true)) {
		std::cout << exp->message << '\n';
	}
	// ����
	subDict->Set("add by code", nullptr);
	subDict->Set("add by code1", true);
	subDict->Set("add by code2", false);
	subDict->Get("dd"). force_get<IJsonArray*>()->Remove(2);
	subDict->Remove("add by code");
	db->GetRootNode()->Set("root", std::move(subDict));
	auto dbClone = vstd::create_unique(dbBase->CreateDatabase());
	if (dbClone->GetRootNode()->Read(db->GetRootNode()->Serialize(), false)) {
		std::cout << dbClone->GetRootNode()->FormattedPrint();
	}
}