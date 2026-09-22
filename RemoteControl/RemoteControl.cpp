#include <stdio.h>
#include <Windows.h>
#include <atlimage.h>
#include <ShellScalingAPI.h>

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

enum Mouse_Enum {
    MOUSE_MOVE = 1,
    MOUSE_LEFT_DOWN = 2,
    MOUSE_LEFT_UP = 3,
    MOUSE_RIGHT_DOWN = 4,
    MOUSE_RIGHT_UP = 5,
    MOUSE_MIDDLE_DOWN = 6,
    MOUSE_MIDDLE_UP = 7,
    MOUSE_LEFT_CLICK = 8,
    MOUSE_RIGHT_CLICK = 9,
    MOUSE_MIDDLE_CLICK = 10,
    MOUSE_LEFT_DOUBLE_CLICK = 11,
    MOUSE_RIGHT_DOUBLE_CLICK = 12,
    MOUSE_MIDDLE_DOUBLE_CLICK = 13,
};

struct Mouse {
    int action;
    POINT point;
};

struct Key_Board {
    int virtual_code;
    int key_status;
};

enum Cmd {
    CMD_SCREEN = 1,
    CMD_MOUSE = 2,
    CMD_KEYBOARD = 4,
    CMD_TESTCONNECT = 2026
};

#define RECV_BUFFER_SIZE 1024*1024*10

int get_packet_len(Packet* packet);
Packet* pack_packet(int cmd, char* buffer, int buffer_len);
Packet* parse_packet(char* buffer, int len);
int init_server(void);

int handle_command(Packet* packet);
int handle_screen(Packet* packet);
int handle_mouse(Packet* packet);
int handle_keyboard(Packet* packet);
int handle_testconnect(Packet* packet);

SOCKET g_server_socket = INVALID_SOCKET;
SOCKET g_client_socket = INVALID_SOCKET;

DWORD g_screen_thread_id = 0;
DWORD g_mouse_thread_id = 0;
DWORD g_keyboard_thread_id = 0;

#define WM_HANDLE_SCREEN (WM_USER + 1)
#define WM_HANDLE_MOUSE (WM_USER + 2)
#define WM_HANDLE_KEYBOARD (WM_USER + 3)
#define WM_HANDLE_INVOKE_MSG_LOOP (WM_USER + 4)

DWORD WINAPI handle_screen_thread_func(LPVOID lpParameter) {
    // Thread code here
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        if (msg.message == WM_HANDLE_SCREEN) {
            Packet *packet = (Packet *)msg.lParam;
            handle_screen(packet);
            free(packet);
        }
    }
    return 0;
}

DWORD WINAPI handle_mouse_thread_func(LPVOID lpParameter) {
    // Thread code here
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        if (msg.message == WM_HANDLE_MOUSE) {
            Packet* packet = (Packet*)msg.lParam;
            handle_mouse(packet);
            free(packet);
        }
    }
    return 0;
}

DWORD WINAPI handle_keyboard_thread_func(LPVOID lpParameter) {
    // Thread code here
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        if (msg.message == WM_HANDLE_KEYBOARD) {
            Packet* packet = (Packet*)msg.lParam;
            handle_keyboard(packet);
            free(packet);
        }
    }
    return 0;
}


int main() {
#if 1    
    if (init_server() != 0) {
        printf("启动服务失败\r\n");
        return 0;
    }


    CreateThread(NULL, 0, handle_screen_thread_func, NULL, 0, &g_screen_thread_id);
    CreateThread(NULL, 0, handle_mouse_thread_func, NULL, 0, &g_mouse_thread_id);
    CreateThread(NULL, 0, handle_keyboard_thread_func, NULL, 0, &g_keyboard_thread_id);

    PostThreadMessage(g_screen_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);
    PostThreadMessage(g_mouse_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);
    PostThreadMessage(g_keyboard_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);

    Sleep(100);

    // 5）等待客户端连接
    SOCKADDR_IN client_addr;
    int client_addr_len = sizeof(SOCKADDR_IN);
    printf("SERVER\n");
    printf("等待客户端连接\r\n");
    g_client_socket = accept(g_server_socket, (sockaddr*)&client_addr, &client_addr_len);    // 阻塞
    printf("客户端连接成功\r\n");

    // 6）等待接受客户端socket发送的数据

    char* buffer = (char*)malloc(RECV_BUFFER_SIZE);
    if (buffer == NULL) {
        printf("malloc buffer failed\r\n");
        return 0;
    }

    static int index = 0;
    while (true) {
        printf("等待接收数据\r\n");
        int len = recv(g_client_socket, buffer + index, RECV_BUFFER_SIZE - index, 0); // index : rest data size
        if (len > 0) {
            index += len;                           // buffer[packet1packet2pack0000000000000..00]  !!packet1 : packet:1\0
            printf("接受数据成功：%d\r\n", len);
        }
        else {
            printf("接受数据失败\r\n");
        }

        if (index > 0) {
            printf("开始解析数据\r\n");
            Packet* packet = parse_packet(buffer, index); // parse one packet, rest move to buffer head
            while (packet != NULL && index > 0) {
                if (packet == NULL) {
                    printf("解析数据失败\r\n");
                }
                printf("解析数据成功\r\n");
                if (packet != NULL) {
                    index -= get_packet_len(packet);                          // how many char(byte) rest data
                    memmove(buffer, buffer + get_packet_len(packet), index);  // rest data move to buffer head
                    handle_command(packet);
                    //free(packet);
                }
                // parse all data in buffer, if not enough data, wait for next recv
                packet = parse_packet(buffer, index); // parse next packet
            }
        }
    }

    // 8）程序结束关闭套接字
    closesocket(g_client_socket);
    closesocket(g_server_socket);
    // 清除掉
    WSACleanup();
#endif
    // handle_screen(NULL);
    return 0;
}

int handle_command(Packet* packet) {
    printf("handle_command: cmd = %d\r\n", packet->header.cmd);
    int ret = 0;
    switch (packet->header.cmd) {
    case CMD_SCREEN: {
        PostThreadMessage(g_screen_thread_id, WM_HANDLE_SCREEN, NULL, (LPARAM)packet);
        //ret = handle_screen(packet);
        break;
    }
    case CMD_MOUSE: {
        PostThreadMessage(g_mouse_thread_id, WM_HANDLE_MOUSE, NULL, (LPARAM)packet);
        //ret = handle_mouse(packet);
        break;
    }
    case CMD_KEYBOARD: {
        PostThreadMessage(g_keyboard_thread_id, WM_HANDLE_KEYBOARD, NULL, (LPARAM)packet);
        //ret = handle_keyboard(packet);
        break;
    }
    case CMD_TESTCONNECT: {
        ret = handle_testconnect(packet);
        break;
    }
    default:
        break;
    }
    return ret;
}


int handle_screen(Packet* packet) {
    CImage image; //  1）创建image对象
    HDC h_screen = GetDC(NULL); // 2）拿到屏幕上下文
    int bit_width = GetDeviceCaps(h_screen, BITSPIXEL); // 3）拿到屏幕像素位宽
    printf("bit_width : %d\r\n", bit_width); // 32

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); // 让程序感知DPI缩放，获取真实屏幕分辨率

    // 4）拿到屏幕宽高（以像素为单位）
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    printf("width : %d, height : %d\r\n", screen_width, screen_height);
    image.Create(screen_width, screen_height, bit_width);

    // 5）把屏幕数据复制到image的HDC里
    BitBlt(image.GetDC(), 0, 0, screen_width, screen_height, h_screen, 0, 0,
        SRCCOPY);
    ReleaseDC(NULL, h_screen);
    // image.Save("test.png", ::Gdiplus::ImageFormatPNG);

    HGLOBAL heap_memory = GlobalAlloc(GMEM_MOVEABLE, 0);
    if (heap_memory == NULL) return -1;

    IStream* p_stream = NULL;
    HRESULT ret = CreateStreamOnHGlobal(heap_memory, true, &p_stream);
    if (ret == S_OK) {
        // PNG -> [memory]
        image.Save(p_stream, ::Gdiplus::ImageFormatPNG);
        LARGE_INTEGER large_integer = { 0 };
        p_stream->Seek(large_integer, STREAM_SEEK_SET, NULL); // p_stream -> heap_memory
        char* pdata = (char*)GlobalLock(heap_memory);
        int len = GlobalSize(heap_memory);

        // 发送数据
        Packet* packet = pack_packet(CMD_SCREEN, pdata, len);
        int send_len = send(g_client_socket, (char*)&packet->header.magic, sizeof(Packet_Header) + len, 0);
        if (send_len > 0) {
            printf("发送数据成功 : %d\r\n", send_len);
        }
        else if (send_len < 0) {
            printf("发送数据失败 : %d\r\n", send_len);
        }
        free(packet);
        GlobalUnlock(heap_memory);
    }

    // cleanup
    p_stream->Release();
    image.ReleaseDC();
    // 6）通过网络发送数据

    return 0;
}

int handle_mouse(Packet* packet) {
    Mouse mouse;
    memcpy(&mouse.action, packet->body, packet->header.body_len);
    printf("x : %d, y : %d\r\n", mouse.point.x, mouse.point.y);

    SetCursorPos(mouse.point.x, mouse.point.y);
    switch (mouse.action)
    {
    case MOUSE_MOVE:
        SetCursorPos(mouse.point.x, mouse.point.y);
        break;
    case MOUSE_LEFT_DOWN:
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_LEFT_UP:
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_RIGHT_DOWN:
        mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_RIGHT_UP:
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_MIDDLE_DOWN:
        mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_MIDDLE_UP:
        mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_LEFT_CLICK:
        mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_RIGHT_CLICK:
        mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_MIDDLE_CLICK:
        mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_LEFT_DOUBLE_CLICK:
        mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_RIGHT_DOUBLE_CLICK:
        mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    case MOUSE_MIDDLE_DOUBLE_CLICK:
        mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        break;
    default:
        printf("未知鼠标操作: %d\r\n", mouse.action);
        break;
    }

    return 0;
}

int handle_keyboard(Packet* packet) {
    Key_Board key_board = {0};
    memcpy(&key_board.virtual_code, packet->body, packet->header.body_len);
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key_board.virtual_code;
    input.ki.wScan = 0;
    input.ki.dwFlags = key_board.key_status; // 0：pressed, 1: released
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;

    int ret = SendInput(1, &input, sizeof(INPUT));
    if (ret > 0) {
        printf("成功执行键盘事件 ：%d\r\n", key_board.virtual_code);
    }
    return 0;
}

int handle_testconnect(Packet* packet) {
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
    for (; i < len; i++) {
        if (*(int*)(buffer + i) == 0x55AA77CC) {
            packet.header.magic = *(int*)(buffer + i);
            i += 4;
            break;
        }
    }

    packet.header.cmd = *(int*)(buffer + i); i += 4;
    packet.header.body_len = *(int*)(buffer + i); i += 4; // follow body data
    if (packet.header.body_len == 0) {
        ppacket = (Packet*)malloc(sizeof(Packet));
        if (ppacket == NULL)
            return NULL;

        memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
        return ppacket;
    }
    if (packet.header.body_len > 0) {
        ppacket = (Packet*)malloc(sizeof(Packet) + packet.header.body_len);
        if (ppacket == NULL)
            return NULL;

        memcpy(ppacket->body, (char*)(buffer + i), packet.header.body_len);
        memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
        return ppacket;
    }
    return NULL;
}

Packet* pack_packet(int cmd, char* buffer, int buffer_len) {
    Packet* packet = (Packet*)malloc(buffer_len + sizeof(Packet_Header));
    if (packet == NULL)
        return NULL;

    packet->header.magic = 0x55AA77CC;
    packet->header.cmd = cmd;
    packet->header.body_len = buffer_len;
    if (buffer && buffer_len > 0) {
        memcpy(packet->body, buffer, buffer_len);
    }
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
        return -1;
    }

    // 3）给服务器绑定地址
    SOCKADDR_IN server_addr;                // 准备一个地址(ip+port)
    server_addr.sin_family = AF_INET;        // ipv4协议
    server_addr.sin_port = htons(9999);        // 监听端口
    //server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.1.4");    // 监听服务器所有ip，因为电脑不止一个ip地址
    server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.37.129");    // 监听服务器所有ip，因为电脑不止一个ip地址
    rc = bind(g_server_socket, (sockaddr*)&server_addr, sizeof(SOCKADDR_IN));
    if (rc == SOCKET_ERROR) {
        printf("Failed to bind server_addr!\r\n");
        return -2;
    }

    // 4）开启服务器socket监听, backlog：允许完成三次握手的客户端数量
    rc = listen(g_server_socket, 1);
    if (rc == SOCKET_ERROR) {
        printf("开启监听失败\r\n");
        return -3;
    }

    return 0;
}
