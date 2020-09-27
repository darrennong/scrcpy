#pragma once
#include "ScrPlayer.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <WS2tcpip.h>
#include "common.h"
using namespace std;
#pragma comment(lib,"WS2_32.lib")
class Server
{
private :
	unsigned short port = 6587;
    string serverIP = "10.0.0.246";
    SOCKADDR_IN addrSrv;

	// 【1】初始化WinSock
	bool initWinSockC()
	{
		WORD verision = MAKEWORD(2, 2);
		WSADATA lpData;
		int intEr = WSAStartup(verision, &lpData); // 指定winsock版本并初始化
		if (intEr != 0)
		{
			LOGE("WinSock初始化失败！\n");
			return false;
		}
		LOGI("WinSock初始化成功\r");
		return true;
	}

	// 【2】创建socket
	bool createSocketC(SOCKET& listenScok)
	{
		// 创建侦听socket  
		listenScok = socket(AF_INET, SOCK_STREAM, 0);
		if (listenScok == INVALID_SOCKET)
		{
			LOGE("socket创建失败！\r\n");
			return false;
		}
		LOGI("socket创建成功！\r\n" );
		return true;
	}

	// 【3】连接到服务器
	bool connectSocketC(SOCKET& conSock, const string ip, const unsigned short port)
	{
		// 建立地址结构体
		sockaddr_in hostAddr;
		hostAddr.sin_family = AF_INET;
		hostAddr.sin_port = htons(port);//转换成网络字节序  
										//hostAddr.sin_addr.S_un.S_addr = inet_addr(SERVERIP);//转换成网络字节序  
										//cout << "net IP:" << hostAddr.sin_addr.S_un.S_addr << endl;  
										/*
										inet_addr()版本太低，被弃用使用inet_pton(协议族，字符串IP地址，void目标in_addr*)
										头文件：WS2tcpip.h
										*/
		in_addr addr;
		inet_pton(AF_INET, ip.c_str(), (void*)&addr);
		hostAddr.sin_addr = addr;
		cout << "ip(网络字节序):" << addr.S_un.S_addr << endl;
		cout << "ip(常规形式):" << ip.c_str() << endl;

		// 向服务器提出连接请求
		int err = connect(conSock, (sockaddr*)&hostAddr, sizeof(sockaddr));
		if (err == INVALID_SOCKET)
		{
			cout << "连接服务器失败！" << endl;
			return false;
		}
		return true;
	}

	// 【4】发送数据
	bool sendDataC(SOCKET& clientSock, const string& data)
	{
		int err = send(clientSock, data.c_str(), data.size(), 0);
		if (err == SOCKET_ERROR)
		{
			cout << "发送失败！" << endl;
			return false;
		}
		cout << "发送数据为:\n" << data.c_str() << endl;
		return true;
	}

	// 【5】接收数据
	bool receiveDataC(SOCKET& clientSock, string& data)
	{
		static int cnt = 1; // 接收数据编号-静态
							// 通过已建立连接的套接字，接收数据 设定缓冲1024字节
		char buf[1024] = "\0";
		// flags操作方式（0正常数据，MSG_PEED系统缓冲区的数据复制到所提供的接收缓冲区内，系统缓冲区数据未删除，MSG_OOB处理带外数据，通常用参数0即可）
		int buflen = recv(clientSock, buf, 1024, 0);
		if (buflen == SOCKET_ERROR)
		{
			LOGE("数据接收失败");
			return false;
		}
		// 一切正常则显示接收数据
		data = string(buf);
		LOGI("Packet no.d%",cnt++);
		return true;
	}
public:
	SOCKET videoSocket;
	SOCKET controlSocket;
	bool start() {
		if (initWinSockC()){
			createSocketC(videoSocket);
			connectSocketC(videoSocket, serverIP, port);
			string data;
			receiveDataC(videoSocket, data);
			createSocketC(controlSocket);
			connectSocketC(controlSocket, serverIP, port + 1);
		}
		return true;
	}

	bool device_read_info(char* device_name, size_w* size) {
		unsigned char buf[DEVICE_NAME_FIELD_LENGTH + 4];
		//int r = net_recv_all(videoSocket, buf, sizeof(buf));
		int r = recv(videoSocket, (char *)buf, sizeof(buf), MSG_WAITALL);
		if (r < DEVICE_NAME_FIELD_LENGTH + 4) {
			LOGE("Could not retrieve device information");
			return false;
		}
		// in case the client sends garbage
		buf[DEVICE_NAME_FIELD_LENGTH - 1] = '\0';
		// strcpy is safe here, since name contains at least
		// DEVICE_NAME_FIELD_LENGTH bytes and strlen(buf) < DEVICE_NAME_FIELD_LENGTH
		strcpy_s(device_name, DEVICE_NAME_FIELD_LENGTH, (char*)buf);
		size->width = (buf[DEVICE_NAME_FIELD_LENGTH] << 8)
			| buf[DEVICE_NAME_FIELD_LENGTH + 1];
		size->height = (buf[DEVICE_NAME_FIELD_LENGTH + 2] << 8)
			| buf[DEVICE_NAME_FIELD_LENGTH + 3];
		return true;
	}
};

