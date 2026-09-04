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

int get_packet_len(Packet* packet);
Packet* pack_packet(int cmd, char* buffer, int buffer_len);
Packet* parse_packet(char* buffer, int len);
int init_server(void);

SOCKET g_server_socket = INVALID_SOCKET;

int main() {
    init_server();
    // 5）等待客户端连接
    SOCKADDR_IN client_addr;
    int client_addr_len = sizeof(SOCKADDR_IN);
    printf("SERVER\n");
    printf("等待客户端连接\r\n");
    SOCKET client_socket = accept(g_server_socket, (sockaddr *)&client_addr, &client_addr_len);	// 阻塞
    printf("客户端连接成功\r\n");

    // 6）等待接受客户端socket发送的数据

    char* buffer = (char*)malloc(RECV_BUFFER_SIZE);

    static int index = 0;
    while (true) {
	    int len = recv(client_socket, buffer + index, RECV_BUFFER_SIZE - index, 0); // index : rest data size
	    index += len; // buffer[packet1packet2pack0000000000000..00]  !!packet1 : packet:1\0
	    Packet* packet = parse_packet(buffer, len); // parse one packet, rest move to buffer head
	    index -= get_packet_len(packet); // how many char(byte) rest data
	    memmove(buffer, buffer + get_packet_len(packet), index); // rest data move to buffer head
    #if 0
	    fwrite(buffer, sizeof(char), len, stdout);	// 换种打印方式
	    fwrite("\r\n----\r\n", 1, 8, stdout);
    #endif
	    printf("server recv packet->body : %s\r\n", packet->body);
	    printf("server recv packet->header.magic : %x\r\n", packet->header.magic);
	    printf("server recv packet->header.cmd : %d\r\n", packet->header.cmd);
	    printf("server recv packet->header.body_len : %d\r\n", packet->header.body_len);

	    // 7）给客户端发送数据
	    Packet *send_packet = pack_packet(packet->header.cmd, packet->body, packet->header.body_len);
	    send(client_socket, (char *)&packet->header.magic, get_packet_len(send_packet), 0);
	    printf("server send data : %s\r\n", &buffer[12]);

	    free(packet);
	    // Sleep(500);	// 模拟服务器处理命令（耗时1s）
    }
	
    // 8）程序结束关闭套接字
    closesocket(client_socket);
    closesocket(g_server_socket);
    // 清除掉
    WSACleanup();
    return 0;
}

int get_packet_len(Packet* packet) {
    if (packet != NULL) {
	    return packet->header.body_len + sizeof(packet->header);
    }
    else return 0;
}

// parse data : "<header> + packet:1"
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
	    memcpy(ppacket->body, (char *)(buffer + i), packet.header.body_len);
	    memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
	    return ppacket;
    }
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

int init_server(void) {
    int rc;
    // 服务器网络编程步骤
    // 1）初始化网络环境
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    // 2）创建服务端socket
    g_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (g_server_socket == INVALID_SOCKET) {
        printf("创建服务器socket失败\r\n");
        return 0;
    }

    // 3）给服务器绑定地址
    SOCKADDR_IN server_addr;				// 准备一个地址(ip+port)
    server_addr.sin_family = AF_INET;		// ipv4协议
    server_addr.sin_port = htons(9999);		// 监听端口
    server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.1.4");	// 监听服务器所有ip，因为电脑不止一个ip地址
    rc = bind(g_server_socket, (sockaddr*)&server_addr, sizeof(SOCKADDR_IN));
    if (rc == SOCKET_ERROR) {
        printf("Failed to bind server_addr!\r\n");
        return 0;
    }

    // 4）开启服务器socket监听, backlog：允许完成三次握手的客户端数量
    rc = listen(g_server_socket, 1);
    if (rc == SOCKET_ERROR) {
        printf("开启监听失败\r\n");
        return 0;
    }
}