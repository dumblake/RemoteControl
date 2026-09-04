#include <stdio.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1)
struct Packet_Header {
    int magic;
    int cmd;
    int body_len; // by byte
};
#pragma pack(pop)

struct Packet {
    Packet_Header header;
    char body[];
};

#define RECV_BUFFER_SIZE 1024*1024*1

Packet* pack_packet(int cmd, char* buffer, int buffer_len);
Packet* parse_packet(char* buffer, int len);
int init_scoket(void);

SOCKET g_client_socket;
SOCKADDR_IN g_server_addr;

int main (void) {
    printf("CLIENT\n");
    int rc;

    init_scoket();

    // 3）连接服务器
    rc = connect(g_client_socket, (sockaddr *)&g_server_addr, sizeof(SOCKADDR_IN));
    if (rc == SOCKET_ERROR) {
	printf("连接服务器失败\r\n");
	return 0;
    }
    printf("连接服务器成功\r\n");

    // 4）向服务端发送数据
    char buffer[1024] = {0};

    char *recv_buffer = (char *)malloc(RECV_BUFFER_SIZE);
    int count = 0;

    while (true) {
	count++;
	printf("请输入要发送的数据: ");	// 接受用户输入 stdin
	fgets(buffer, 1024, stdin);
		
	//snprintf(buffer, 1024, "packet:%d", count);

	//Packet* packet = (Packet*)malloc(sizeof(Packet) + 10);
	//packet->header.magic = 0x55AA77CC;
	//packet->header.cmd = 2000;
	//packet->header.body_len = 10;
	//memcpy(packet->body, buffer, 10);
		
	Packet *send_packet = pack_packet(2000, buffer, 10);
	// start address and data size : "<header> + packet:1"
	send(g_client_socket,
	     (char*)&send_packet->header.magic, 
	     send_packet->header.body_len + sizeof(send_packet->header), 
	     0); // Can (char *)packet?
	free(send_packet);
	printf("client send data: %s\r\n", buffer);

	// 5）客户端等待接收服务端数据
	int len = recv(g_client_socket, recv_buffer, RECV_BUFFER_SIZE, 0);
	if (len > 0) {
	    Packet* recv_packet = parse_packet(recv_buffer, len);
	    printf("client recv data : %s\r\n", recv_packet->body);
	    printf("client recv packet->header.magic : %x\r\n", recv_packet->header.magic);
	    printf("client recv packet->header.cmd : %d\r\n", recv_packet->header.cmd);
	    printf("client recv packet->header.body_len : %d\r\n", recv_packet->header.body_len);
	    free(recv_packet);
	}
		
	// Sleep(10);
    }


    // 6）程序结束关闭套接字
    closesocket(g_client_socket);
    // 清除
    WSACleanup();
    return 0;
}

Packet* pack_packet(int cmd, char* buffer, int buffer_len) {
    Packet* packet = (Packet*)malloc(buffer_len + sizeof(packet->header));
    packet->header.magic = 0x55AA77CC;
    packet->header.cmd = cmd;
    packet->header.body_len = buffer_len;
    if (buffer && buffer_len > 0) {
	memcpy(packet->body, buffer, buffer_len);
    }
    else return NULL;
    return packet;
}

Packet* parse_packet(char* buffer, int len) {
    // check header
    Packet packet;
    Packet* ppacket = NULL;
    int i = 0;
    for (; i < len; i += 4) {
	if (*(int*)(buffer + i) == 0x55AA77CC) {
	    packet.header.magic = *(int*)(buffer + i);
	    i += 4;
	    break;
	}
    }
    /*
      if ( *(int *)(buffer + i) == 0x55AA77CC) {
      packet.header.magic = 0x55AA77CC;
      i+=4;
      } else return NULL;
    */
    packet.header.cmd = *(int*)(buffer + i); i += 4;
    packet.header.body_len = *(int*)(buffer + i); i += 4; // follow body data
    if (packet.header.body_len > 0) {
	ppacket = (Packet*)malloc(sizeof(Packet) + packet.header.body_len);
	memcpy(ppacket->body, (char*)(buffer + i), packet.header.body_len);
	memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
	return ppacket;
    }
    return NULL;
}

int init_scoket(void) {
    // 客户端网络编程步骤
    // 1）初始化网络环境
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    // 2）创建socket连接
    g_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (g_client_socket == INVALID_SOCKET) {
        printf("创建socket失败\r\n");
        return 0;
    }
    g_server_addr;
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_port = htons(9999);	// 转为网络字节序
    g_server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.1.4");
}