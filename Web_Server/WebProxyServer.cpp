#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define WEB_PORT 8081       // 웹서버 포트: 클라이언트가 접속
#define API_PORT 8080       // API 서버 포트: 백엔드 API
#define API_IP "127.0.0.1"  // API 서버 IP
#define BUFSIZE 8192

#define WWW_ROOT "../client/mail-client"

// MIME 타입 맵핑
static const char* get_mime_type(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    return "application/octet-stream";
}

// 파일 내용을 읽어 HTTP 응답으로 전송
static void serve_static_file(SOCKET c, const char* filepath) {
    FILE* fp;
    char fullpath[256];
    char header[1024];
    long file_size;

    _snprintf_s(fullpath, sizeof(fullpath), _TRUNCATE, "%s%s", WWW_ROOT, filepath);
    
    if (fopen_s(&fp, fullpath, "rb") != 0 || !fp) {
        _snprintf_s(header, sizeof(header), _TRUNCATE, "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        send(c, header, (int)strlen(header), 0);
        printf("[INFO] 404 Not Found: %s\n", fullpath);
        return;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // HTTP 헤더 구성
    _snprintf_s(header, sizeof(header), _TRUNCATE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        get_mime_type(fullpath), file_size);

    send(c, header, (int)strlen(header), 0);
    
    // 파일 내용 전송
    char buffer[BUFSIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFSIZE, fp)) > 0) {
        send(c, buffer, (int)bytes_read, 0);
    }

    fclose(fp);
    printf("[INFO] 200 OK: %s (Size: %ld)\n", fullpath, file_size);
}

// API 서버로 요청을 전달하고 응답을 클라이언트에게 반환 (프록시)
static void proxy_to_api_server(SOCKET client_socket, const char* request, int request_len) {
    SOCKET api_s;
    struct sockaddr_in api_server_addr;
    char buffer[BUFSIZE];
    int recv_size;
    
    // 1. API 서버 소켓 생성 및 연결
    api_s = socket(AF_INET, SOCK_STREAM, 0);
    if (api_s == INVALID_SOCKET) {
        printf("[ERROR] API socket creation failed: %d\n", WSAGetLastError());
        const char* error_resp = "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, error_resp, (int)strlen(error_resp), 0);
        return;
    }

    memset(&api_server_addr, 0, sizeof(api_server_addr));
    api_server_addr.sin_family = AF_INET;
    api_server_addr.sin_port = htons(API_PORT);
    api_server_addr.sin_addr.s_addr = inet_addr(API_IP);

    if (connect(api_s, (struct sockaddr*)&api_server_addr, sizeof(api_server_addr)) == SOCKET_ERROR) {
        printf("[ERROR] API connection failed: %d\n", WSAGetLastError());
        closesocket(api_s);
        const char* error_resp = "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, error_resp, (int)strlen(error_resp), 0);
        return;
    }

    // 2. API 서버로 요청 전달
    send(api_s, request, request_len, 0);

    // 3. API 서버 응답 수신 및 클라이언트에게 전달
    printf("[INFO] Proxying response from API server.\n");
    do {
        recv_size = recv(api_s, buffer, BUFSIZE, 0);
        if (recv_size > 0) {
            send(client_socket, buffer, recv_size, 0);
        }
    } while (recv_size > 0);

    closesocket(api_s);
}

// 클라이언트 요청 처리 핸들러 (스레드 함수)
unsigned int __stdcall handle_client(void* arg) {
    SOCKET c = (SOCKET)arg;
    char buf[BUFSIZE] = { 0 };
    int n;

    // 1. 요청 수신
    n = recv(c, buf, BUFSIZE - 1, 0);
    if (n <= 0) {
        closesocket(c);
        return 0;
    }
    buf[n] = '\0';
    
    // 2. 요청 라인 파싱: "METHOD SP PATH SP HTTP/1.1"
    char method[16] = { 0 };
    char path[256] = { 0 };
    sscanf_s(buf, "%15s %255s", method, (unsigned)_countof(method), path, (unsigned)_countof(path));
    printf("[REQUEST] %s %s\n", method, path);

    // 3. 라우팅 및 처리
    if (strcmp(path, "/") == 0) {
        serve_static_file(c, "/index.html");
    } else if (strstr(path, "/api/")) {
        proxy_to_api_server(c, buf, n);
    } else {
        serve_static_file(c, path);
    }

    closesocket(c);
    return 0;
}


int main(void) {
    WSADATA wsa;
    SOCKET server_socket = INVALID_SOCKET;
    struct sockaddr_in server;

    // 1. Winsock 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup 실패: %d\n", WSAGetLastError());
        return 1;
    }

    // 2. 서버 리스닝 소켓 생성 및 바인딩
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    BOOL yes = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(WEB_PORT);

    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("바인딩 실패: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("리스닝 실패: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("웹서버 대기 중 (클라이언트 포트: %d, API 프록시 포트: %d)\n", WEB_PORT, API_PORT);

    // 3. 메인 accept 루프 (멀티스레딩 적용)
    while (1) {
        SOCKET client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            printf("accept 실패: %d\n", WSAGetLastError());
            continue;
        }
        unsigned int threadId;
        _beginthreadex(NULL, 0, handle_client, (void*)client_socket, 0, &threadId);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}