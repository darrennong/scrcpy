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

	// ��1����ʼ��WinSock
	bool initWinSockC()
	{
		WORD verision = MAKEWORD(2, 2);
		WSADATA lpData;
		int intEr = WSAStartup(verision, &lpData); // ָ��winsock�汾����ʼ��
		if (intEr != 0)
		{
			LOGE("WinSock��ʼ��ʧ�ܣ�\n");
			return false;
		}
		LOGI("WinSock��ʼ���ɹ�\r");
		return true;
	}

	// ��2������socket
	bool createSocketC(SOCKET& listenScok)
	{
		// ��������socket  
		listenScok = socket(AF_INET, SOCK_STREAM, 0);
		if (listenScok == INVALID_SOCKET)
		{
			LOGE("socket����ʧ�ܣ�\r\n");
			return false;
		}
		LOGI("socket�����ɹ���\r\n" );
		return true;
	}

	// ��3�����ӵ�������
	bool connectSocketC(SOCKET& conSock, const string ip, const unsigned short port)
	{
		// ������ַ�ṹ��
		sockaddr_in hostAddr;
		hostAddr.sin_family = AF_INET;
		hostAddr.sin_port = htons(port);//ת���������ֽ���  
										//hostAddr.sin_addr.S_un.S_addr = inet_addr(SERVERIP);//ת���������ֽ���  
										//cout << "net IP:" << hostAddr.sin_addr.S_un.S_addr << endl;  
										/*
										inet_addr()�汾̫�ͣ�������ʹ��inet_pton(Э���壬�ַ���IP��ַ��voidĿ��in_addr*)
										ͷ�ļ���WS2tcpip.h
										*/
		in_addr addr;
		inet_pton(AF_INET, ip.c_str(), (void*)&addr);
		hostAddr.sin_addr = addr;
		cout << "ip(�����ֽ���):" << addr.S_un.S_addr << endl;
		cout << "ip(������ʽ):" << ip.c_str() << endl;

		// ������������������
		int err = connect(conSock, (sockaddr*)&hostAddr, sizeof(sockaddr));
		if (err == INVALID_SOCKET)
		{
			cout << "���ӷ�����ʧ�ܣ�" << endl;
			return false;
		}
		return true;
	}

	// ��4����������
	bool sendDataC(SOCKET& clientSock, const string& data)
	{
		int err = send(clientSock, data.c_str(), data.size(), 0);
		if (err == SOCKET_ERROR)
		{
			cout << "����ʧ�ܣ�" << endl;
			return false;
		}
		cout << "��������Ϊ:\n" << data.c_str() << endl;
		return true;
	}

	// ��5����������
	bool receiveDataC(SOCKET& clientSock, string& data)
	{
		static int cnt = 1; // �������ݱ��-��̬
							// ͨ���ѽ������ӵ��׽��֣��������� �趨����1024�ֽ�
		char buf[1024] = "\0";
		// flags������ʽ��0�������ݣ�MSG_PEEDϵͳ�����������ݸ��Ƶ����ṩ�Ľ��ջ������ڣ�ϵͳ����������δɾ����MSG_OOB����������ݣ�ͨ���ò���0���ɣ�
		int buflen = recv(clientSock, buf, 1024, 0);
		if (buflen == SOCKET_ERROR)
		{
			LOGE("���ݽ���ʧ��");
			return false;
		}
		// һ����������ʾ��������
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

