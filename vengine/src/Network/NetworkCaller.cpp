
#include <Network/network_config.h>
#include <Network/INetworkService.h>
#include <Network/ISocket.h>
#include <Network/NetworkInclude.h>
#include <Network/FunctionSerializer.h>
#include <Common/DynamicDLL.h>
#include <Common/LockFreeStepQueue.h>
#include <Database/DatabaseInclude.h>
#include <Utility/MD5.h>
namespace toolhub::net {
static db::IJsonDatabase* GenerateDB() {
	static DllFactoryLoader<db::Database> dll("VEngine_Database.dll", "Database_GetFactory");
	auto dbBase = dll();
	return dbBase->CreateDatabase();
}
static db::WriteJsonVariant DeserArg(
	vbyte& var,
	db::IJsonDatabase* dbase,
	vstd::span<vbyte const>& data) {
	var = vstd::SerDe<vbyte>::Get(data);
	switch (var) {
		case db::WriteJsonVariant::IndexOf<int64>:
			return vstd::SerDe<int64>::Get(data);
		case db::WriteJsonVariant::IndexOf<double>:
			return vstd::SerDe<double>::Get(data);
		case db::WriteJsonVariant::IndexOf<vstd::string>:
			return vstd::SerDe<vstd::string>::Get(data);
		case db::WriteJsonVariant::IndexOf<vstd::unique_ptr<db::IJsonDict>>: {
			auto dict = dbase->CreateDict();
			uint byteSize = vstd::SerDe<uint>::Get(data);
			dict->Read({data.data(), byteSize}, false);
			data = {data.data() + byteSize, data.size() - byteSize};
			return dict;
		}
		case db::WriteJsonVariant::IndexOf<vstd::unique_ptr<db::IJsonArray>>: {
			auto arr = dbase->CreateArray();
			uint byteSize = vstd::SerDe<uint>::Get(data);
			arr->Read({data.data(), byteSize}, false);
			data = {data.data() + byteSize, data.size() - byteSize};
			return arr;
		}
		case db::WriteJsonVariant::IndexOf<vstd::Guid>:
			return vstd::SerDe<vstd::Guid>::Get(data);
		case db::WriteJsonVariant::IndexOf<bool>:
			return vstd::SerDe<bool>::Get(data);
		default:
			return {};
	}
}
static void SerArg(
	db::WriteJsonVariant const& var,
	vstd::vector<vbyte>& vec) {
	vstd::SerDe<vbyte>::Set(var.GetType(), vec);
	auto write = [&](auto&& v) {
		vstd::SerDe<std::remove_cvref_t<decltype(v)>>::Set(v, vec);
	};
	auto writeJson = [&](auto&& ptr) {
		size_t vecIndex = vec.size();
		vstd::SerDe<uint>::Set(0, vec);
		ptr->Serialize(vec);
		*reinterpret_cast<uint*>(&vec[vecIndex]) = (vec.size() - (sizeof(uint) + vecIndex));
	};
	var.multi_visit(
		write,
		write,
		write,
		writeJson,
		writeJson,
		write,
		write);
}

class RpcBase {
protected:
	vstd::unique_ptr<db::IJsonDatabase> db;
	vstd::unique_ptr<ISocket> socket;

public:
	RpcBase(vstd::unique_ptr<ISocket>&& socket)
		: socket(std::move(socket)) {
		db = vstd::create_unique(GenerateDB());
	}
};

class RpcClient : public IRpcClient, public RpcBase {
	struct CallCmd {
		vstd::string funcName;
		db::WriteJsonVariant arg;
		Future<db::WriteJsonVariant> retValue;
		template<typename A, typename B, typename C>
		CallCmd(
			A&& a,
			B&& b,
			C&& c)
			: funcName(std::forward<A>(a)),
			  arg(std::forward<B>(b)),
			  retValue(std::forward<C>(c)) {}
	};
	vstd::LockFreeArrayQueue<CallCmd> callCmd;
	vstd::string errMsg;
	vstd::vector<vbyte> netBuffer;
	std::mutex thdMtx;
	std::condition_variable cv;

public:
	void Dispose() override {
		delete this;
	}
	vstd::string GetErrorMessage() override { return errMsg; }
	RpcClient(vstd::unique_ptr<ISocket>&& socket)
		: RpcBase(std::move(socket)) {
	}

	db::IJsonDatabase* GetDB() override {
		return db.get();
	}
	bool Block_SendMsg() override {
		netBuffer.clear();
		netBuffer.reserve(65536);
		while (auto md = callCmd.Pop()) {
			netBuffer.clear();
			netBuffer.resize(sizeof(uint64));
			vstd::SerDe<vstd::string>::Set(md->funcName, netBuffer);
			SerArg(md->arg, netBuffer);
			*reinterpret_cast<uint64*>(netBuffer.data()) = netBuffer.size() - sizeof(uint64);
			if (!socket->Write(netBuffer)) {
				errMsg = socket->ErrorMessage();
				return false;
			}
			if (!socket->Read(netBuffer, sizeof(uint64))) {
				errMsg = socket->ErrorMessage();
				return false;
			}
			uint64 bufferSize = *reinterpret_cast<uint64*>(netBuffer.data());
			if (!socket->Read(netBuffer, bufferSize)) {
				errMsg = socket->ErrorMessage();
				return false;
			}
			if (bufferSize != netBuffer.size()) {
				errMsg = "Illegal socket buffer!";
				return false;
			}
			vstd::span<vbyte const> sp = netBuffer;
			vbyte tarType;
			md->retValue.Set(DeserArg(tarType, db.get(), sp));
			if (tarType == 255) {
				errMsg.clear();
				errMsg << "Function not found!";
				return false;
			}
		}
		{
			std::unique_lock lck(thdMtx);
			if (callCmd.Length() == 0)
				cv.wait(lck);
		}
		return true;
	}

	Future<db::WriteJsonVariant> CallFunctor(
		vstd::string_view name,
		db::WriteJsonVariant&& arg) override {
		Future<db::WriteJsonVariant> result;
		callCmd.Push(name, std::move(arg), result);
		cv.notify_all();
		return result;
	}
	void CallFunctors(
		vstd::IEnumerable<std::pair<vstd::string_view, db::WriteJsonVariant>>& nameAndArg,
		vstd::vector<Future<db::WriteJsonVariant>>& retValues) override {
		for (; !nameAndArg.End(); nameAndArg.GetNext()) {
			Future<db::WriteJsonVariant> result;
			auto pair = nameAndArg.GetValue();
			callCmd.Push(pair.first, std::move(pair.second), result);
			retValues.emplace_back(std::move(result));
		}
		cv.notify_all();
	}
	~RpcClient() {
		cv.notify_all();
	}
};
class RpcServer : public IRpcServer, public RpcBase {
private:
	vstd::string errMsg;
	vstd::vector<vbyte> netBuffer;

public:
	bool Block_RecvMsg() override {
		errMsg.clear();
		netBuffer.reserve(65536);
		if (!socket->Read(netBuffer, sizeof(uint64))) {
			errMsg = socket->ErrorMessage();
			return false;
		}
		uint64 buffSize = *reinterpret_cast<uint64*>(netBuffer.data());
		if (!socket->Read(netBuffer, buffSize)) {
			errMsg = socket->ErrorMessage();
			return false;
		}
		vstd::span<vbyte const> sp = netBuffer;
		auto funcName = vstd::SerDe<vstd::string>::Get(sp);
		vbyte argType;
		auto arg = DeserArg(argType, db.get(), sp);
		netBuffer.clear();
		netBuffer.resize(sizeof(uint64));
		auto ite = NetWorkImpl::current->funcMap.Find(funcName);
		if (!ite) {
			vstd::SerDe<vbyte>::Set(255, netBuffer);
		} else {
			SerArg(ite.Value()(db.get(), std::move(arg)), netBuffer);
		}
		*reinterpret_cast<uint64*>(netBuffer.data()) = netBuffer.size() - sizeof(uint64);
		socket->Write(netBuffer);
		return true;
	}
	db::IJsonDatabase* GetDB() override {
		return db.get();
	}
	vstd::string GetErrorMessage() override { return errMsg; }
	void Dispose() override {
		delete this;
	}
	RpcServer(vstd::unique_ptr<ISocket>&& socket)
		: RpcBase(std::move(socket)) {
	}
	~RpcServer() {
	}
};
IRpcClient* NetWorkImpl::GetClientRpc(vstd::unique_ptr<ISocket>&& socket) const {
	return new RpcClient(std::move(socket));
}
IRpcServer* NetWorkImpl::GetServerRpc(vstd::unique_ptr<ISocket>&& socket) const {
	return new RpcServer(std::move(socket));
}
void NetWorkImpl::RegistFunctor(
	vstd::string_view name,
	RpcFunc func) {
	funcMap.Emplace(name, std::move(func));
}
}// namespace toolhub::net