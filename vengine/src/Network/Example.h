#include <Common/DynamicDLL.h>
#include <Network/ISocket.h>
#include <Network/NetworkInclude.h>
#include <Network/INetworkService.h>
namespace toolhub::net {
toolhub::db::WriteJsonVariant GetStringValue(toolhub::db::IJsonDatabase* db, toolhub::db::WriteJsonVariant&& arg) {
	if (!arg.template IsTypeOf<vstd::string>()) return {};
	std::cout << arg.template force_get<vstd::string>() << '\n';
	return arg.template force_get<vstd::string>() + " from server!";
}

void server() {
	using namespace toolhub::net;
	DllFactoryLoader<NetWork> loader("VEngine_Network.dll", "NetWork_GetFactory");
	auto network = loader();
	std::cout << "0: server\n"
			  << "other: client\n";
	vstd::string str;
	std::cin >> str;
	constexpr uint PORT = 35535;
	////////////////////// Server
	if (str == "0") {
		//Set port 35535
		auto acc = network->GenServerAcceptor(PORT);
		//Init socket & connect (thread blocked)
		//Can have multiple socket for multiple client
		vstd::unique_ptr<ISocket> socket(network->GenServerTCPSock(acc));
		if (!socket->Connect()) {
			std::cout << "Connect Failed!\n";
			return;
		} else {
			std::cout << "Connected!\n";
		}
		//Generate rpc from socket
		auto rpc = MakeObjectPtr(network->GetServerRpc(std::move(socket)));
		//Regist function "GetStringValue"
		VE_NET_REGIST_FUNCTOR(*network, GetStringValue);
		//Server keep looping
		while (true) {
			if (!rpc->Block_RecvMsg()) {
				std::cout << "Error: " << rpc->GetErrorMessage() << '\n';
				return;
			}
		}
	}
	////////////////////// Client
	else {
		// Connect to server
		vstd::unique_ptr<ISocket> socket(network->GenClientTCPSock(PORT, "127.0.0.1"));
		if (!socket->Connect()) {
			std::cout << "Connect Failed!\n";
			return;
		} else {
			std::cout << "Connected!\n";
		}
		auto rpc = MakeObjectPtr(network->GetClientRpc(std::move(socket)));
		std::cout << "Input String:\n";
		// Start a looping thread
		std::thread tt([rpcWeak = vstd::ObjWeakPtr(rpc)]() {
			while (true) {
				auto rpc = vstd::ObjectPtr(rpcWeak);
				if (!rpc) {
					std::cout << "RPC Closed!\n";
					return;
				} else if (!rpc->Block_SendMsg()) {
					std::cout << "Error: " << rpc->GetErrorMessage() << '\n';
					return;
				}
			}
		});
		while (true) {
			vstd::string value;
			std::cin >> value;
			// Call server function & get return value
			auto retValue = rpc->CallFunctor("GetStringValue", std::move(value)).Get();
			if (retValue.template IsTypeOf<vstd::string>()) {
				std::cout << retValue.template force_get<vstd::string>() << '\n';
			}
		}
		tt.join();
	}
}
}// namespace toolhub::net