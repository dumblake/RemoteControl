#include <stdio.h>
#include <Windows.h>
#include <atlimage.h>

#pragma comment(lib, "ws2_32.lib")
#define RECV_BUFFER_SIZE 1024*1024*10

// data struct
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

enum Cmd {
    CMD_SCREEN = 1,
    CMD_MOUSE = 2,
    CMD_KEYBOARD = 4,
    CMD_TESTCONNECT = 2026
};

// function prototype
Packet* pack_packet(int cmd, char* buffer, int buffer_len);
Packet* parse_packet(char* buffer, int len);
int init_socket(void);
int get_packet_len(Packet* packet);
int init_window(HINSTANCE handle_instance, int num_cmd_show);

DWORD WINAPI send_screen_callback(LPVOID lpThreadParameter);

// global variables
SOCKET g_client_socket;
SOCKADDR_IN g_server_addr;
HWND g_hwnd = NULL;
CImage g_image;
CRITICAL_SECTION g_critical_section; // used to protect g_image

// typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK winProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // got image
        if (!g_image.IsNull()) {
            RECT client_rect;
            GetClientRect(hwnd, &client_rect);
            int client_width = client_rect.right - client_rect.left;
            int client_height = client_rect.bottom - client_rect.top;

            int old_mode = SetStretchBltMode(hdc, HALFTONE); // HALFTONE 高清
            SetBrushOrgEx(hdc, 0, 0, NULL); // set brush origin location
            
            EnterCriticalSection(&g_critical_section); // enter critical section to protect g_image
            int remote_width = g_image.GetWidth();
            int remote_height = g_image.GetHeight();

            g_image.StretchBlt(hdc, 0, 0, client_width, client_height, 0, 0,
                remote_width, remote_height, SRCCOPY);
            LeaveCriticalSection(&g_critical_section); // leave critical section

            SetStretchBltMode(hdc, old_mode);
        }

        EndPaint(hwnd, &ps);
        break;
    }
    default:
        return DefWindowProc(hwnd, msg, w_param, l_param);
        break;
    }
    return 0;
}

// entry point of windows GUI program
int WINAPI WinMain(HINSTANCE handle_instance,
    HINSTANCE handle_previous_instance, PSTR p_cmd_line,
    int num_cmd_show)
{
    InitializeCriticalSection(&g_critical_section);
    init_window(handle_instance, num_cmd_show);
    init_socket();

    // connect server
    int rc = connect(g_client_socket, (sockaddr*)&g_server_addr,
        sizeof(SOCKADDR_IN));
    if (rc == SOCKET_ERROR) {
        printf("连接服务器失败\r\n");
        return 0;
    }

    // create thread used to loop send screen data
    DWORD send_screen_thread_id = 0;
    HANDLE handle_send_screen = CreateThread(NULL, 0, send_screen_callback, NULL, 0, &send_screen_thread_id);
    OutputDebugString("连接服务器成功\r\n");

    // process message like queue
    MSG message = { 0 };
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

DWORD WINAPI send_screen_callback(LPVOID lpThreadParameter) {
    char* receive_buffer = (char*)malloc(RECV_BUFFER_SIZE);
    if (receive_buffer == NULL) {
        printf("malloc receive_buffer failed\r\n");
        return 0;
    }
    
    while (true) {
        Packet* packet = pack_packet(CMD_SCREEN, NULL, 0); // just send one cammand
        int send_len = send(g_client_socket, (char*)&packet->header.magic, get_packet_len(packet), 0);
        if (send_len > 0) {
            OutputDebugString("成功发送数据");
        }
        free(packet);
        int len = recv(g_client_socket, receive_buffer, RECV_BUFFER_SIZE, 0);
        if (len > 0) {
            Packet* receive_packet = parse_packet(receive_buffer, RECV_BUFFER_SIZE);
            if (receive_packet != NULL) { // parsed packet successful
                // process packet body data
                HGLOBAL heap_memory = GlobalAlloc(GMEM_MOVEABLE, 0);
                if (heap_memory == NULL)
                    continue;

                IStream* p_stream = NULL;
                HRESULT ret = CreateStreamOnHGlobal(heap_memory, true, &p_stream);
                if (ret == S_OK) {
                    ULONG length = 0;
                    p_stream->Write(receive_packet->body, receive_packet->header.body_len, &length);
                    free(receive_packet); // free receive_packet to avoid memory leak

                    LARGE_INTEGER large_integer = { 0 };
                    p_stream->Seek(large_integer, STREAM_SEEK_SET, NULL); // set stream pointer to the beginning of the stream
                    
                    EnterCriticalSection(&g_critical_section); // enter critical section to protect g_image
                    if (g_image.IsNull() == FALSE) {
                        g_image.Destroy(); // destroy the old image to avoid memory leak
                    }
                    g_image.Load(p_stream); // load image from stream
                    LeaveCriticalSection(&g_critical_section); // leave critical section

                    InvalidateRect(g_hwnd, NULL, FALSE); // invalidate the entire client area of the window, causing a WM_PAINT message to be sent to the window
                    UpdateWindow(g_hwnd); // force the window to be repainted immediately, causing a WM_PAINT message to be sent to the window
                }
            }
        }
    }
}

int init_window(HINSTANCE handle_instance, int num_cmd_show) {
    // register window class
    WNDCLASS window_class = { 0 };
    LPCSTR class_name = "MainWindow";
    window_class.lpfnWndProc = winProc;
    window_class.hInstance = handle_instance;
    window_class.lpszClassName = class_name;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    window_class.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClass(&window_class)) {
        MessageBox(NULL, "register window failed!", "error",
            MB_OK | MB_ICONERROR);
        return 0;
    }

    // create window
    g_hwnd = CreateWindow(class_name, "Remote Control", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, NULL, NULL,
        handle_instance, NULL);
    if (g_hwnd == NULL) {
        MessageBox(NULL, "create window failed!", "error", MB_OK | MB_ICONERROR);
        return 0;
    }

    ShowWindow(g_hwnd, num_cmd_show);

    UpdateWindow(g_hwnd);
}

/* main just a console program, not a windows GUI program, so no need to use WinMain */
//int main(void) {
//    printf("CLIENT\n");
//    init_scoket();
//
//    // 3）连接服务器
//    if (connect(g_client_socket, (sockaddr*)&g_server_addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
//        printf("连接服务器失败\r\n");
//        return 0;
//    }
//    printf("连接服务器成功\r\n");
//
//    // 4）向服务端发送数据
//    char buffer[1024] = { 0 };
//
//    char* recv_buffer = (char*)malloc(RECV_BUFFER_SIZE);
//    if (recv_buffer == NULL) {
//        printf("malloc recv_buffer failed\r\n");
//        return 0;
//    }
//    int count = 0;
//
//    while (true) {
//        count++;
//        printf("请输入要发送的数据: ");    // 接受用户输入 stdin
//        fgets(buffer, 1024, stdin);
//
//        //snprintf(buffer, 1024, "packet:%d", count);
//
//        //Packet* packet = (Packet*)malloc(sizeof(Packet) + 10);
//        //packet->header.magic = 0x55AA77CC;
//        //packet->header.cmd = 2000;
//        //packet->header.body_len = 10;
//        //memcpy(packet->body, buffer, 10);
//
//        Packet* send_packet = pack_packet(1, buffer, 10);
//        // start address and data size : "<header> + packet:1"
//        send(g_client_socket,
//            (char*)&send_packet->header.magic,
//            send_packet->header.body_len + sizeof(send_packet->header),
//            0); // Can (char *)packet?
//        free(send_packet);
//        printf("client send data: %s\r\n", buffer);
//
//        // 5）客户端等待接收服务端数据
//        int len = recv(g_client_socket, recv_buffer, RECV_BUFFER_SIZE, 0);
//        if (len > 0) {
//            Packet* recv_packet = parse_packet(recv_buffer, len);
//            printf("client recv data : %s\r\n", recv_packet->body);
//            printf("client recv packet->header.magic : %x\r\n", recv_packet->header.magic);
//            printf("client recv packet->header.cmd : %d\r\n", recv_packet->header.cmd);
//            printf("client recv packet->header.body_len : %d\r\n", recv_packet->header.body_len);
//            if (recv_packet->header.cmd == 1) {
//                // 服务器返回了屏幕数据
//                // 解析屏幕数据
//            }
//            free(recv_packet);
//        }
//
//        // Sleep(10);
//    }
//
//
//    // 6）程序结束关闭套接字
//    closesocket(g_client_socket);
//    // 清除
//    WSACleanup();
//    return 0;
//}

Packet* pack_packet(int cmd, char* buffer, int buffer_len) {
    Packet* packet = (Packet*)malloc(buffer_len + sizeof(Packet_Header));
    if (packet == NULL) {
        return NULL;
    }
    packet->header.magic = 0x55AA77CC;
    packet->header.cmd = cmd;
    packet->header.body_len = buffer_len;
    if (buffer && buffer_len > 0) {
        memcpy(packet->body, buffer, buffer_len);
    }
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
        if (ppacket == NULL)
            return NULL;

        memcpy(ppacket->body, (char*)(buffer + i), packet.header.body_len);
        memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
        return ppacket;
    }
    if (packet.header.body_len == 0) {
        ppacket = (Packet*)malloc(sizeof(Packet));
        if (ppacket == NULL)
            return NULL;

        memcpy(&ppacket->header, &packet.header, sizeof(ppacket->header));
        return ppacket;
    }
    return NULL;
}

int init_socket(void) {
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
    g_server_addr.sin_port = htons(9999);    // 转为网络字节序
    g_server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.1.4");
}

int get_packet_len(Packet* packet) {
    if (packet != NULL) {
        return packet->header.body_len + sizeof(packet->header);
    }
    else
        return 0;
}
