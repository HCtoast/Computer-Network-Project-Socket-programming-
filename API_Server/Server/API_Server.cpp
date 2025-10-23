#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#define BUF 8192  // 단일 요청을 수용할 임시 버퍼 크기(학습용)

static int sendall(SOCKET s, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(s, buf + total, len - total, 0);
        if (n == SOCKET_ERROR) return SOCKET_ERROR;
        total += n;
    }
    return total;
}

// 간단 HTTP 응답 헬퍼: 상태줄 + 헤더 + 본문 순서로 전송
static void http_send(SOCKET c, int code, const char* status, const char* ct, const char* body) {
    char hdr[1024];
    const int bl = (int)strlen(body);

    // HTTP/1.1 상태줄과 필수 헤더(여기선 Content-Type, Content-Length, Connection) 작성
    _snprintf_s(hdr, sizeof(hdr), _TRUNCATE,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status, ct, bl);

    // 헤더와 바디 순서대로 송신(부분 전송 대비 sendall)
    sendall(c, hdr, (int)strlen(hdr));
    sendall(c, body, bl);
}

int main(void) {
    WSADATA wsa;
    SOCKET s = INVALID_SOCKET;

    // 1) Winsock 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup 실패: %d\n", WSAGetLastError());
        return 1;
    }

    // 2) 서버 리스닝 소켓 생성
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        printf("socket 실패: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // 3) 재시작 시 bind 충돌 방지
    BOOL yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    // 4) 바인드 주소/포트 설정 (127.0.0.1:8080)
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;     // 모든 인터페이스
    a.sin_port = htons(8080);           // HTTP 개발용 포트

    if (bind(s, (struct sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        printf("bind 실패: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen 실패: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }

    printf("HTTP API server on http://127.0.0.1:8080\n");

    // 5) 메인 accept 루프
    for (;;) {
        SOCKET c = accept(s, NULL, NULL);
        if (c == INVALID_SOCKET) {
            printf("accept 실패: %d\n", WSAGetLastError());
            continue; // 서버는 계속 유지
        }

        // 6) 단일 요청 수신 (학습용: 한번의 recv로 요청을 모두 받는다고 가정)
        char buf[BUF] = { 0 };
        int n = recv(c, buf, BUF - 1, 0);
        if (n <= 0) { // 0=정상 종료, <0=에러
            closesocket(c);
            continue;
        }
        buf[n] = '\0'; // 안전한 문자열 처리를 위해 널 종료

        // 7) 요청 라인 파싱: "METHOD SP PATH SP HTTP/1.1"
        char method[8] = { 0 };
        char path[256] = { 0 };
        sscanf_s(buf, "%7s %255s", method, (unsigned)_countof(method), path, (unsigned)_countof(path));

        // 8) 간단 라우팅: /api/* 만 처리, 그 외는 안내 페이지
        if (strcmp(method, "GET") == 0 && strcmp(path, "/api/inbox") == 0) {
            // TODO: 실제로는 storage_list_json() 결과 반환
            const char* json = "{\"items\":[]}";
            http_send(c, 200, "OK", "application/json", json);

        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/send") == 0) {
            // POST의 경우 바디가 헤더 다음에 오므로, 헤더/바디 구분자 "\r\n\r\n"을 찾는다.
            char* body = strstr(buf, "\r\n\r\n");
            body = body ? body + 4 : (char*)""; // 바디 시작 지점

            // TODO: body(JSON)에서 from/to/subject/body 파싱 → smtp_send(host,port,...) 호출
            // TODO: 성공 시 storage_save_eml(), index.json 갱신
            (void)body; // 컴파일 경고 회피(미사용 변수)

            // 응답: 데모용 고정 ID
            http_send(c, 200, "OK", "application/json",
                "{\"ok\":true,\"messageId\":\"demo-0001\"}");

        }
        else {
            // 정적 페이지(가이드). 실사용에선 실제 index.html을 읽어 서빙하거나
            // web 서버를 분리할 수 있다.
            http_send(c, 200, "OK", "text/html; charset=utf-8",
                "<!doctype html><html><body>"
                "<h1>Mail API (demo)</h1>"
                "<ul>"
                "<li><a href=\"/api/inbox\">GET /api/inbox</a></li>"
                "<li>POST /api/send (body: {from,to,subject,body})</li>"
                "</ul>"
                "</body></html>");
        }

        // 이 연결은 keep-alive를 쓰지 않으므로 여기서 종료
        closesocket(c);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}