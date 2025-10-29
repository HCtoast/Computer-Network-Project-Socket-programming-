#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUF_SIZE 8192 // 응답 수신을 위한 버퍼 크기


// 전송 함수
static int sendall(SOCKET s, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(s, buf + total, len - total, 0);
        if (n == SOCKET_ERROR) return SOCKET_ERROR;
        total += n;
    }
    return total;
}


// TCP 소켓 생성, 지정된 서버에 연결
// 실패 시 INVALID_SOCKET
SOCKET connect_to_server() {
    SOCKET client_socket = INVALID_SOCKET;
    struct sockaddr_in server_addr;

    // 1) 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        printf("ERROR: 소켓 생성 실패: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // 문자열 IP를 네트워크 주소로 변환

    printf("INFO: 서버 (%s:%d)에 연결 시도 중...\n", SERVER_IP, SERVER_PORT);

    // 2) 서버 연결
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("ERROR: 서버 연결 실패: %d\n", WSAGetLastError());
        closesocket(client_socket);
        return INVALID_SOCKET;
    }

    printf("SUCCESS: 서버 연결 성공! 소켓 핸들: %llu\n", (unsigned long long)client_socket);
    return client_socket;
}


// 헤더와 바디의 경계("\r\n\r\n") 위치 반환
static const char* find_header_end(const char* buf) {
    return strstr(buf, "\r\n\r\n");
}


// 응답 수신, 상태 코드와 바디 추출
static int receive_and_parse_http(SOCKET s, char* out_body, int out_body_size) {
    char response[BUF_SIZE] = { 0 };
    int total_received = 0;
    int r;
    const char* header_end = NULL;
    int status_code = 0;
    
    printf("INFO: 응답 수신 대기 중...\n");

    // 1. 헤더 경계(\r\n\r\n)까지 반복 수신
    while ((r = recv(s, response + total_received, BUF_SIZE - 1 - total_received, 0)) > 0) {
        total_received += r;
        response[total_received] = '\0';

        header_end = find_header_end(response);
        if (header_end) {
            break; // 헤더 끝 발견
        }
        if (total_received >= BUF_SIZE - 1) {
            printf("ERROR: 응답 헤더 버퍼 초과.\n");
            return 500; // 임의의 오류 코드로 처리
        }
    }

    if (total_received == 0) {
        printf("ERROR: 서버가 데이터를 보내지 않고 연결을 종료했습니다.\n");
        return 500;
    } else if (r == SOCKET_ERROR) {
        printf("ERROR: 응답 수신 실패: %d\n", WSAGetLastError());
        return 500;
    }

    // 2. 상태 코드 추출
    if (sscanf_s(response, "HTTP/1.1 %d", &status_code) != 1) {
        printf("ERROR: 상태 코드 파싱 실패.\n");
        return 500;
    }

    // 3. 바디 추출
    const char* body_start = header_end + 4; // "\r\n\r\n" 다음
    int body_len = total_received - (int)(body_start - response);

    // 응답 바디 복사
    if (body_len > 0) {
        if (body_len >= out_body_size) {
            body_len = out_body_size - 1;
        }
        memcpy(out_body, body_start, body_len);
    }
    out_body[body_len] = '\0';

    printf("\n--- Received Response ---\n");
    printf("Status: %d\n", status_code);
    printf("Body: %s\n", out_body);
    printf("-------------------------\n");

    return status_code;
}


// HTTP 메시지 작성
void handle_http_request(SOCKET s, const char* method, const char* path, const char* headers, const char* body) {
    char request[BUF_SIZE];
    int body_len = (int)strlen(body);
    
    // HTTP 요청
    if (strcmp(method, "POST") == 0) {
        // POST 요청인 경우
        _snprintf_s(request, BUF_SIZE, _TRUNCATE,
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "Content-Type: application/json\r\n" // 서버의 POST /api/send 스펙에 맞춤
            "Content-Length: %d\r\n"
            "%s\r\n" // 커스텀 헤더
            "\r\n"
            "%s",  // 바디
            method, path, SERVER_IP, SERVER_PORT, body_len, headers, body);
    } else {
        // GET 요청인 경우
        _snprintf_s(request, BUF_SIZE, _TRUNCATE,
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "%s\r\n" // 커스텀 헤더
            "\r\n",
            method, path, SERVER_IP, SERVER_PORT, headers);
    }

    // 요청 전송
    if (sendall(s, request, (int)strlen(request)) == SOCKET_ERROR) {
        printf("ERROR: 요청 전송 실패: %d\n", WSAGetLastError());
        return;
    }
    printf("INFO: 요청 (%s %s) 전송 완료.\n", method, path);

    // HTTP 응답
    char response[BUF_SIZE] = { 0 };
    int status = receive_and_parse_http(s, response_body, sizeof(response_body));

    if (status == 200) {
        // TODO: 여기서 response_body (JSON)를 실제로 파싱하여
        // 메일 리스트, 상세 내용 등을 클라이언트 UI에 반영하는 로직을 추가합니다.
        printf("SUCCESS: %s %s API 호출 성공!\n", method, path);
    } else {
        printf("FAILURE: API 호출 실패! 상태 코드: %d\n", status);
    }
}


int main(void) {
    WSADATA wsa;
    SOCKET connected_socket = INVALID_SOCKET;
    
    // 1. Winsock 초기화
    printf("INFO: Winsock 초기화 중...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("FATAL: WSAStartup 실패: %d\n", WSAGetLastError());
        return 1;
    }
    printf("INFO: Winsock 초기화 완료.\n");

    // 2. 소켓 생성 및 서버 연결

    // 2-1. 메일 리스트 요청
    connected_socket = connect_to_server();
    if (connected_socket != INVALID_SOCKET) {
        const char* user = ""; // 유저
        char list_header[128];
        _snprintf_s(list_header, sizeof(list_header), _TRUNCATE, "X-User: %s", user);
        
        handle_http_request(connected_socket, "GET", "/api/list", list_header, "");
        closesocket(connected_socket);
    }

    // 2-2. 메일 상세 요청
    connected_socket = connect_to_server();
    if (connected_socket != INVALID_SOCKET) {
        const char* mail_id = ""; // 메일 아이디
        char detail_header[128];
        _snprintf_s(detail_header, sizeof(detail_header), _TRUNCATE, "X-Mail-Id: %s", mail_id);
        
        handle_http_request(connected_socket, "GET", "/api/mail", detail_header, "");
        closesocket(connected_socket);
    }

    // 2-3. 메일 전송 요청
    connected_socket = connect_to_server();
    if (connected_socket != INVALID_SOCKET) {
        const char* post_body = 
            "{\"user\":\"유저\","
            "\"mailId\":\"메일 아이디\","
            "\"body\":\"Hello, this is a test email body.\"}";
        
        handle_http_request(connected_socket, "POST", "/api/send", "", post_body);
        closesocket(connected_socket);
    }

    WSACleanup();
    
    return 0;
}