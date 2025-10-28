#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#define PORT 8080

#define RECV_BUF 65536

// 모든 데이터 전송 
static int sendall(SOCKET s, const char *buf, int len) {
  int total = 0;
  while (total < len) {
    int n = send(s, buf + total, len - total, 0);
    if (n == SOCKET_ERROR)
      return SOCKET_ERROR;
    total += n;
  }
  return total;
}

// 소켓 타임아웃 설정 (ms)
static void set_timeouts(SOCKET s, int rcv_ms, int snd_ms) {
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv_ms, sizeof(rcv_ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&snd_ms, sizeof(snd_ms));
}

// 간단 HTTP 응답 헬퍼 (상태줄/헤더/바디 전송)
static void http_send(SOCKET c, int code, const char* status,
    const char* content_type, const char* body) {
    char hdr[1024];
    int bl = (int)strlen(body);
    printf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status, content_type, bl);
    sendall(c, hdr, (int)strlen(hdr));
    sendall(c, body, bl);
}

// =================== HTTP 요청 읽기/헤더 파싱 ===================

// 헤더와 바디 경계("\r\n\r\n") 위치 반환
static const char* find_header_end(const char* buf) {
    return strstr(buf, "\r\n\r\n");
}

// 대소문자 무시 헤더 검색
static int get_header_ci(const char* headers, const char* key, char* out, int outsz) {
    const char* p = headers; size_t klen = strlen(key);
    while (1) {
        const char* e = strstr(p, "\r\n");
        size_t linelen = e ? (size_t)(e - p) : strlen(p);
        if (linelen == 0) break;
        if (linelen > klen && _strnicmp(p, key, klen) == 0 && (p[klen] == ':' || p[klen] == ' ')) {
            const char* v = p + klen; while (*v == ':' || *v == ' ' || *v == '\t') v++;
            int i = 0; while (i < outsz - 1 && (v < p + linelen)) out[i++] = *v++;
            out[i] = 0; return 0;
        }
        if (!e) break; p = e + 2;
    }
    return -1;
}

// Content-Length 정수값 얻기(없으면 0)
static int get_content_length(const char* headers) {
    char cl[32] = { 0 };
    if (get_header_ci(headers, "Content-Length", cl, sizeof(cl)) != 0) return 0;
    return atoi(cl);
}

// 요청 1건을 끝까지 수신: 헤더 경계까지 루프 → Content-Length만큼 바디 추가 수신
// buf에는 "요청라인 + 헤더 + \r\n\r\n + 바디"가 연속 저장됨
static int read_http_request(SOCKET c, char* buf, int bufsz, int* out_total_len) {
    int n = 0; buf[0] = 0;

    // 헤더 경계(\r\n\r\n)까지 반복 수신 (TCP는 스트림이므로 쪼개져 올 수 있음)
    while(1) {
        if (n >= bufsz - 1) return -1;          // 버퍼 초과 보호
        int r = recv(c, buf + n, bufsz - 1 - n, 0);
        if (r <= 0) return -2;                  // 종료/에러
        n += r; buf[n] = 0;
        if (find_header_end(buf)) break;        // 헤더 끝 발견
    }
    // Content-Length만큼 바디 추가 수신 (없으면 안함)
    const char* header_end = find_header_end(buf);
    int header_len = (int)((header_end + 4) - buf);
    int cl = get_content_length(buf);
    int have = n - header_len;
    while (have < cl) {
        if (n >= bufsz - 1) return -1;
        int r = recv(c, buf + n, bufsz - 1 - n, 0);
        if (r <= 0) return -2;
        n += r; have += r; buf[n] = 0;
    }
    *out_total_len = n;
    return 0;
}

// json 추출기 (에러 시 -1, 성공 시 0)
static int json_extract(const char* body, const char* key, char* out, int outsz) {
    char pat[128]; printf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(body, pat); if (!p) return -1;
    p = strchr(p, ':'); if (!p) return -1; p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1; p++;
    int i = 0; while (*p && *p != '"') { if (i < outsz - 1) out[i++] = *p; p++; }
    out[i] = 0; return 0;
}

// =================== 라우트 핸들러 ===================

// GET /api/list  (헤더: X-User)
static void handle_get_list(SOCKET c, const char* req) {
    char user[256] = { 0 };
    if (get_header_ci(req, "X-User", user, sizeof(user)) != 0) {
        http_send(c, 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"missing X-User\"}");
        return;
    }
    // 더미 목록
    const char* json =
        "{"
        " \"ok\": true,"
        " \"total\": 2,"
        " \"items\": ["
        "   {\"id\":\"demo-001\",\"user\":\"%s\",\"date\":\"2025-10-28T00:00:00Z\"},"
        "   {\"id\":\"demo-002\",\"user\":\"%s\",\"date\":\"2025-10-28T01:00:00Z\"}"
        " ]"
        "}";
    char body[1024]; printf(body, sizeof(body), json, user, user);
    http_send(c, 200, "OK", "application/json", body);
}

// GET /api/mail  (헤더: X-Mail-Id)
static void handle_get_mail(SOCKET c, const char* req) {
    char id[256] = { 0 };
    if (get_header_ci(req, "X-Mail-Id", id, sizeof(id)) != 0) {
        http_send(c, 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"missing X-Mail-Id\"}");
        return;
    }
    // 더미 본문
    char body[1024];
    printf(body, sizeof(body),
        "{ \"ok\": true, \"id\": \"%s\", \"raw\": \"This is a dummy mail body for %s\" }",
        id, id);
    http_send(c, 200, "OK", "application/json", body);
}

// POST /api/send  (JSON: {"user":"...","mailId":"...","body":"..."})
static void handle_post_send(SOCKET c, const char* req) {
    const char* he = find_header_end(req);
    const char* body = he ? he + 4 : "";
    char user[256] = { 0 }, mailId[256] = { 0 }, text[1024] = { 0 };

    if (json_extract(body, "user", user, sizeof(user)) != 0 ||
        json_extract(body, "mailId", mailId, sizeof(mailId)) != 0 ||
        json_extract(body, "body", text, sizeof(text)) != 0) {
        http_send(c, 400, "Bad Request", "application/json", "{\"ok\":false,\"error\":\"invalid json fields\"}");
        return;
    }
    // 저장 없이 OK만 리턴
    char resp[1024];
    printf(resp, sizeof(resp),
        "{ \"ok\": true, \"echo\": {\"user\":\"%s\",\"mailId\":\"%s\",\"len\":%d} }",
        user, mailId, (int)strlen(text));
    http_send(c, 200, "OK", "application/json", resp);
}

// =================== 라우팅 ===================

static void route_and_respond(SOCKET c, const char* req) {
    char method[8] = { 0 }, path[256] = { 0 }, ver[16] = { 0 };
    scanf_s(req, "%7s %255s %15s", method, path, ver);

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/list") == 0) { handle_get_list(c, req); return; }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/mail") == 0) { handle_get_mail(c, req); return; }
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/send") == 0) { handle_post_send(c, req); return; }

    http_send(c, 404, "Not Found", "application/json", "{\"ok\":false,\"error\":\"not found\"}");
}

// =================== 메인 서버 루프 ===================

int main(void) {
  WSADATA wsa;
  SOCKET server_socket = INVALID_SOCKET, client_socket = INVALID_SOCKET;
  struct sockaddr_in server, client;
  int client_size = 0;

  printf("Winsock 초기화 중...\n");
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    printf("초기화 실패. 에러 코드: %d\n", WSAGetLastError());
    return 1;
  }
  printf("초기화 완료!\n");

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == INVALID_SOCKET) {
    printf("소켓 생성 실패: %d\n", WSAGetLastError());
    WSACleanup();
    return 1;
  }
  printf("소켓 생성 완료!\n");

  // 빠른 재시작을 위한 옵션 (디버깅 편의)
  BOOL yes = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes,
             sizeof(yes));

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(PORT);

  if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) ==
      SOCKET_ERROR) {
    printf("바인딩 실패. 에러 코드: %d\n", WSAGetLastError());
    closesocket(server_socket);
    WSACleanup();
    return 1;
  }
  printf("바인딩 완료!\n");

  if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
    printf("리스닝 실패. 에러 코드: %d\n", WSAGetLastError());
    closesocket(server_socket);
    WSACleanup();
    return 1;
  }

  printf("API server on http://127.0.0.1:%d\n", PORT);
  printf("서버 대기 중...\n");

  while (1) {
    memset(&client, 0, sizeof(client));
    client_size = sizeof(client);
    client_socket =
        accept(server_socket, (struct sockaddr *)&client, &client_size);
    if (client_socket == INVALID_SOCKET) {
      printf("연결 수락 실패. 에러 코드: %d\n", WSAGetLastError());
      continue; // 서버는 계속 유지
    }
    printf("클라이언트 연결됨\n");

    set_timeouts(client_socket, 15000, 15000);

    char req[RECV_BUF]; int total = 0;
    int r = read_http_request(client_socket, req, sizeof(req), &total);
    if (r == 0) route_and_respond(client_socket, req);
    else     http_send(client_socket, 400, "Bad Request", "application/json", "{\"ok\":false}");

    closesocket(client_socket);

    //while (1) {
    //  int recv_size = recv(client_socket, buffer, (int)sizeof(buffer) - 1, 0);
    //  if (recv_size == 0) { // 정상 종료
    //    printf("클라이언트 정상 종료\n");
    //    break;
    //  }
    //  if (recv_size == SOCKET_ERROR) {
    //    printf("recv 에러: %d\n", WSAGetLastError());
    //    break;
    //  }
    //  buffer[recv_size] = '\0';
    //  printf("클라이언트: %s\n", buffer);

    //  if (sendall(client_socket, buffer, recv_size) == SOCKET_ERROR) {
    //    printf("send 에러: %d\n", WSAGetLastError());
    //    break;
    //  }
    //}
    //closesocket(client_socket);
  }
  closesocket(server_socket);
  WSACleanup();
  return 0;
}
