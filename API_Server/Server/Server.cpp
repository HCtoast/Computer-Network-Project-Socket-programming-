#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#include <time.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)
#define PORT 8080

#define RECV_BUF 65536

#define SERVER_ID "MAILAPI" // 서버 식별자
#define EXPECT_HOST ""      // 기대하는 Host 헤더 값 (빈값이면 검사 안함)

#define MAIL_INDEX_INCLUDES_CURRENT 1
// 메일 인덱스 계산 시, 이번에 저장하는 메일도 포함할지 여부

// =================== 유틸 ===================

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
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcv_ms, sizeof(rcv_ms));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&snd_ms, sizeof(snd_ms));
}

// 간단 HTTP 응답 헬퍼 (상태줄/헤더/바디 전송)
static void http_send(SOCKET c, int code, const char *status,
                      const char *content_type, const char *body) {
  char hdr[1024];
  int bl = (int)strlen(body);
  snprintf(hdr, sizeof(hdr),
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %d\r\n"
           "X-Server-Id: %s\r\n"
           "Connection: close\r\n"
           "\r\n",
           code, status, content_type, bl, SERVER_ID);
  sendall(c, hdr, (int)strlen(hdr));
  sendall(c, body, bl);
}

// 데이터 저장 폴더 생성(없으면 생성)
static void ensure_dirs() {
  CreateDirectoryA("data", NULL);
  CreateDirectoryA("data\\mailbox", NULL);
}

// 텍스트 파일 전체 쓰기
static int write_text(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;
  fwrite(text, 1, strlen(text), f);
  fclose(f);
  return 0;
}

// 텍스트 파일 전체 읽기
static int read_text(const char *path, char *out, int outsz) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  int n = (int)fread(out, 1, outsz - 1, f);
  out[n] = 0;
  fclose(f);
  return n;
}

// JSON 문자열 이스케이프
static void json_escape(const char *src, char *dst, int dstsz) {
  int i = 0;
  for (; *src && i < dstsz - 1; src++) {
    char c = *src;
    if (c == '\"' || c == '\\') {
      if (i + 2 >= dstsz)
        break;
      dst[i++] = '\\';
      dst[i++] = c;
    } else if (c == '\n') {
      if (i + 2 >= dstsz)
        break;
      dst[i++] = '\\';
      dst[i++] = 'n';
    } else if (c == '\r') {
      if (i + 2 >= dstsz)
        break;
      dst[i++] = '\\';
      dst[i++] = 'r';
    } else if (c == '\t') {
      if (i + 2 >= dstsz)
        break;
      dst[i++] = '\\';
      dst[i++] = 't';
    } else
      dst[i++] = c;
  }
  dst[i] = 0;
}

// 현재 저장된 메일 개수 세기
static int get_total_mail_count(void) {
    char buf[RECV_BUF];
    if (read_text("data\\mailbox\\index.json", buf, sizeof(buf)) < 0) return 0;
    int total = 0;
    char* p = strstr(buf, "\"total\"");
    if (p && sscanf(p, "\"total\"%*[^0-9]%d", &total) == 1 && total >= 0) return total;
    return 0;
}

// 이번 저장에서 쓸 mail_index 계산
static int compute_mail_index() {
  int prev = get_total_mail_count();
  return MAIL_INDEX_INCLUDES_CURRENT ? (prev + 1) : prev;
}

// 파일명에 쓸 user를 안전하게 변환(공백/슬래시 등 제거)
static void sanitize_id(const char *src, char *dst, int dstsz) {
  int i = 0;
  for (; *src && i < dstsz - 1; src++) {
    char c = *src;
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
        c == '\"' || c == '<' || c == '>' || c == '|' || c == ' ')
      c = '_';
    dst[i++] = c;
  }
  dst[i] = 0;
}

// =================== HTTP 요청 읽기/헤더 파싱 ===================

// 헤더와 바디 경계("\r\n\r\n") 위치 반환
static const char *find_header_end(const char *buf) {
  return strstr(buf, "\r\n\r\n");
}

// 대소문자 무시 헤더 검색
static int get_header_ci(const char *headers, const char *key, char *out,
                         int outsz) {
  const char *p = headers;
  size_t klen = strlen(key);
  while (1) {
    const char *e = strstr(p, "\r\n");
    size_t linelen = e ? (size_t)(e - p) : strlen(p);
    if (linelen == 0)
      break;
    if (linelen > klen && _strnicmp(p, key, klen) == 0 &&
        (p[klen] == ':' || p[klen] == ' ')) {
      const char *v = p + klen;
      while (*v == ':' || *v == ' ' || *v == '\t')
        v++;
      int i = 0;
      while (i < outsz - 1 && (v < p + linelen))
        out[i++] = *v++;
      out[i] = 0;
      return 0;
    }
    if (!e)
      break;
    p = e + 2;
  }
  return -1;
}

// 요청이 "맞는 서버"로 왔는지 검증. (에러 시 -1, 정상 시 0)
static int verify_server_identity(const char *req) {
  char expect[128] = {0};
  if (get_header_ci(req, "X-Expect-Server", expect, sizeof(expect)) == 0) {
    if (strcmp(expect, SERVER_ID) != 0)
      return -1;
  }
  if (EXPECT_HOST[0]) {
    char host[256] = {0};
    if (get_header_ci(req, "Host", host, sizeof(host)) == 0) {
      if (_stricmp(host, EXPECT_HOST) != 0)
        return -1;
    }
  }
  return 0;
}

// Content-Length 정수값 얻기(없으면 0)
static int get_content_length(const char *headers) {
  char cl[32] = {0};
  if (get_header_ci(headers, "Content-Length", cl, sizeof(cl)) != 0)
    return 0;
  return atoi(cl);
}

// 요청 1건을 끝까지 수신: 헤더 경계까지 루프 → Content-Length만큼 바디 추가
// 수신 buf에는 "요청라인 + 헤더 + \r\n\r\n + 바디"가 연속 저장됨
static int read_http_request(SOCKET c, char *buf, int bufsz,
                             int *out_total_len) {
  int n = 0;
  buf[0] = 0;

  // 헤더 경계(\r\n\r\n)까지 반복 수신 (TCP는 스트림이므로 쪼개져 올 수 있음)
  while (1) {
    if (n >= bufsz - 1)
      return -1; // 버퍼 초과 보호
    int r = recv(c, buf + n, bufsz - 1 - n, 0);
    if (r <= 0)
      return -2; // 종료/에러
    n += r;
    buf[n] = 0;
    if (find_header_end(buf))
      break; // 헤더 끝 발견
  }
  // Content-Length만큼 바디 추가 수신 (없으면 안함)
  const char *header_end = find_header_end(buf);
  int header_len = (int)((header_end + 4) - buf);
  int cl = get_content_length(buf);
  int have = n - header_len;
  while (have < cl) {
    if (n >= bufsz - 1)
      return -1;
    int r = recv(c, buf + n, bufsz - 1 - n, 0);
    if (r <= 0)
      return -2;
    n += r;
    have += r;
    buf[n] = 0;
  }
  *out_total_len = n;
  return 0;
}

// json 추출기 (에러 시 -1, 성공 시 0)
static int json_extract(const char *body, const char *key, char *out,
                        int outsz) {
  char pat[128];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(body, pat);
  if (!p)
    return -1;
  p = strchr(p, ':');
  if (!p)
    return -1;
  p++;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p != '"')
    return -1;
  p++;
  int i = 0;
  while (*p && *p != '"') {
    if (i < outsz - 1)
      out[i++] = *p;
    p++;
  }
  out[i] = 0;
  return 0;
}

// =================== 저장소(index.json 초기화/추가) ===================

// index.json이 없으면 초기화
static void init_index_if_missing(void) {
    if (GetFileAttributesA("data\\mailbox\\index.json") == INVALID_FILE_ATTRIBUTES) {
        write_text("data\\mailbox\\index.json",
            "{ \"total\": 0, \"items\": [] }");
    }
}

// index.json 에 items 뒤에 1건 추가(간단 문자열 조작; 원자적 갱신)
static int index_append(const char *user, const char *iso, int* out_index,
                        const char *to, const char *title) {
  char buf[RECV_BUF];
  if (read_text("data\\mailbox\\index.json", buf, sizeof(buf)) < 0)
    return -1;

  // total++
  int total = 0;
  char *totp = strstr(buf, "\"total\":");
  if (!totp || sscanf(totp, "\"total\":%d", &total) != 1) 
      return -1;
  int new_index = total + 1;
  char tmp[RECV_BUF];
  char* after = strchr(totp, ','); if (!after) return -1;
  int head = (int)(totp - buf);
  _snprintf(tmp, sizeof(tmp), "%.*s\"total\":%d%s", head, buf, new_index, after);
  strcpy(buf, tmp);

  // items 배열 뒤에 추가
  char *end = strrchr(buf, ']');
  if (!end)
    return -1;
  int comma = (end - 1 >= buf && *(end - 1) != '[');

  char escTitle[RECV_BUF];
  json_escape(title, escTitle, sizeof(escTitle));

  char add[1024];
  snprintf(add, sizeof(add),
           "%s{\"user\":\"%s\",\"date\":\"%s\",\"mail_index\":%d, \"to\":\"%s\", "
           "\"title\":\"%s\"}]}",
           comma ? "," : "", user, iso, new_index, to, escTitle);

  int prefix_len = (int)(end - buf);
  char out[RECV_BUF];
  snprintf(out, sizeof(out), "%.*s%s", prefix_len, buf, add);

  if (write_text("data\\mailbox\\index.json.tmp", out) != 0)
    return -1;
  MoveFileExA("data\\mailbox\\index.json.tmp", "data\\mailbox\\index.json",
              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (out_index) *out_index = new_index;
  return 0;
}

// path가 "/api/mail?index=N" 형태일 때 index를 파싱 (성공:0, 실패:-1)
static int parse_query_index(const char* path, int* out_index) {
    const char* q = strchr(path, '?'); 
    if (!q) 
        return -1; 
    q++;
    const char* key = "index=";
    const char* p = strstr(q, key); 
    if (!p) 
        return -1; 
    p += (int)strlen(key);
    int v = 0; 
    if (!(*p >= '0' && *p <= '9')) 
        return -1;
    while (*p >= '0' && *p <= '9') { 
        v = v * 10 + (*p - '0'); 
        p++; 
    }
    if (v <= 0) 
        return -1; 
    *out_index = v; 
    return 0;
}

// =================== 라우트 핸들러 ===================

// GET /api/resp  → 서버 이름과 연결 상태 반환
static void handle_get_resp(SOCKET c, const char *req) {

  // 응답 JSON
  char body[1024];
  snprintf(body, sizeof(body),
            "{"
            "\"ok\":true,"
            "\"server\":\"%s\""
            "}",
            SERVER_ID);

  http_send(c, 200, "OK", "application/json", body);
}

// GET /api/list
static void handle_get_list(SOCKET c, const char *req) {
  char user[256] = {0};
  /*if (get_header_ci(req, "X-User", user, sizeof(user)) != 0) {
    http_send(c, 400, "Bad Request", "application/json",
              "{\"ok\":false,\"error\":\"missing X-User\"}");
    return;
  }*/
  char json[RECV_BUF];
  if (read_text("data\\mailbox\\index.json", json, sizeof(json)) < 0) {
    http_send(c, 500, "Internal Server Error", "application/json",
              "{\"ok\":false,\"error\":\"index read\"}");
    return;
  }
  http_send(c, 200, "OK", "application/json", json);
}

// GET /api/mail
static void handle_get_mail(SOCKET c, const char *req, const char* path) {
  char id[256] = { 0 };
  int mail_index = 0;
  if (parse_query_index(path, &mail_index) != 0) {
      http_send(c, 400, "Bad Request", "application/json",
          "{\"ok\":false,\"error\":\"query ?index=N required\"}");
      return;
  }

  char pathfile[512], content[RECV_BUF];
  snprintf(pathfile, sizeof(pathfile), "data\\mailbox\\%d.json",
      mail_index);
  if (read_text(pathfile, content, sizeof(content)) < 0) {
    http_send(c, 404, "Not Found", "application/json",
              "{\"ok\":false,\"error\":\"not found\"}");
    return;
  }
  char resp[RECV_BUF];
  snprintf(resp, sizeof(resp), "{\"ok\":true,\"raw\":\"%s\"}",
           content);
  http_send(c, 200, "OK", "application/json", resp);
}

// POST /api/send  (JSON: {"user":"...","to":"...","title":"...","body":"..."})
static void handle_post_send(SOCKET c, const char *req) {
  const char *he = find_header_end(req);
  const char *body = he ? he + 4 : "";
  char user[256] = {0}, to[256] = {0}, title[256] = {0}, text[1024] = {0};

  if (json_extract(body, "user", user, sizeof(user)) != 0 ||
      json_extract(body, "to", to, sizeof(to)) != 0 ||
      json_extract(body, "title", title, sizeof(title)) != 0 ||
      json_extract(body, "body", text, sizeof(text)) != 0) {
    http_send(c, 400, "Bad Request", "application/json",
              "{\"ok\":false,\"error\":\"invalid json fields\"}");
    return;
  }
  if (strstr(title, "..") || strchr(title, '/') || strchr(title, '\\')) {
    http_send(c, 400, "Bad Request", "application/json",
              "{\"ok\":false,\"error\":\"invalid title\"}");
    return;
  }
  ensure_dirs();
  init_index_if_missing();

  // mail_index 계산
  int mail_index = compute_mail_index();

  // UTC 시각
  char iso[64];
  {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_s(&tm, &t);
    snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec);
  }

  //// 원문(.msg) 구성
  // char msg[RECV_BUF];
  // snprintf(msg, sizeof(msg),
  //     "User: %s\r\nMail-Id: %s\r\nDate: %s\r\n\r\n%s\r\n",
  //     user, title, iso, text);

  //// 파일 저장
  // char path[512]; snprintf(path, sizeof(path), "data\\mailbox\\%s.msg",
  // title); if (write_text(path, msg) != 0) {
  //     http_send(c, 500, "Internal Server Error", "application/json",
  //     "{\"ok\":false,\"error\":\"write msg\"}"); return;
  // }

  // 원문 이스케이프 --> JSON 문서 생성
  char escBody[RECV_BUF];
  json_escape(text, escBody, sizeof(escBody));
  char escTitle[RECV_BUF];
  json_escape(title, escTitle, sizeof(escTitle));

  char doc[RECV_BUF];
  snprintf(doc, sizeof(doc),
           "{ \"user\":\"%s\", \"mail_index\":%d, \"to\":\"%s\", "
           "\"title\":\"%s\", \"date\":\"%s\", \"body\":\"%s\" }",
           user, mail_index, to, escTitle, iso, escBody);

  // 파일 저장: <mail_index>.json 형태
  char path[512];
  snprintf(path, sizeof(path), "data\\mailbox\\%d.json", mail_index);
  if (write_text(path, doc) != 0) {
    http_send(c, 500, "Internal Server Error", "application/json",
              "{\"ok\":false,\"error\":\"write json\"}");
    return;
  }

  // index.json 반영
  if (index_append(user, iso, &mail_index, to, title) != 0) {
    http_send(c, 500, "Internal Server Error", "application/json",
              "{\"ok\":false,\"error\":\"index append\"}");
    return;
  }

  char resp[256];
  snprintf(resp, sizeof(resp), "{ \"ok\": true, \"mail_index\": %d }",
            mail_index);
  http_send(c, 200, "OK", "application/json", "{\"ok\":true}");
}

// =================== 라우팅 ===================

static void route_and_respond(SOCKET c, const char *req) {
  // 서버 식별 검증
  if (verify_server_identity(req) != 0) {
    http_send(c, 421, "Misdirected Request", "application/json",
              "{\"ok\":false,\"error\":\"wrong server\",\"server\":\"" SERVER_ID
              "\"}");
    return;
  }
  char method[8] = {0}, path[256] = {0}, ver[16] = {0};
  sscanf(req, "%7s %255s %15s", method, path, ver);

  if (strcmp(method, "GET") == 0 && strcmp(path, "/api/resp") == 0) {
    handle_get_resp(c, req);
    return;
  }
  if (strcmp(method, "GET") == 0 && strcmp(path, "/api/list") == 0) {
    handle_get_list(c, req);
    return;
  }
  if (strcmp(method, "GET") == 0 && strncmp(path, "/api/mail", 9) == 0) {
    handle_get_mail(c, req, path);
    return;
  }
  if (strcmp(method, "POST") == 0 && strcmp(path, "/api/send") == 0) {
    handle_post_send(c, req);
    return;
  }

  http_send(c, 404, "Not Found", "application/json",
            "{\"ok\":false,\"error\":\"not found\"}");
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
    printf("클라이언트 연결됨\nServer : %s\n",
           SERVER_ID); // 클라이언트가 요청할 때 마다 출력 (디버깅용)

    set_timeouts(client_socket, 15000, 15000);

    char req[RECV_BUF];
    int total = 0;
    int r = read_http_request(client_socket, req, sizeof(req), &total);
    if (r == 0)
      route_and_respond(client_socket, req);
    else
      http_send(client_socket, 400, "Bad Request", "application/json",
                "{\"ok\":false}");

    closesocket(client_socket);
  }
  closesocket(server_socket);
  WSACleanup();
  return 0;
}
