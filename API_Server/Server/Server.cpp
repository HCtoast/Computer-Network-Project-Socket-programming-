#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define PORT 8080

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

int main(void) {
  WSADATA wsa;
  SOCKET server_socket = INVALID_SOCKET, client_socket = INVALID_SOCKET;
  struct sockaddr_in server, client;
  int client_size = 0;

  char buffer[1024];

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
    printf("클라이언트 연결됨\n", inet_ntoa(client.sin_addr),
           ntohs(client.sin_port));

    while (1) {
      int recv_size = recv(client_socket, buffer, (int)sizeof(buffer) - 1, 0);
      if (recv_size == 0) { // 정상 종료
        printf("클라이언트 정상 종료\n");
        break;
      }
      if (recv_size == SOCKET_ERROR) {
        printf("recv 에러: %d\n", WSAGetLastError());
        break;
      }
      buffer[recv_size] = '\0';
      printf("클라이언트: %s\n", buffer);

      if (sendall(client_socket, buffer, recv_size) == SOCKET_ERROR) {
        printf("send 에러: %d\n", WSAGetLastError());
        break;
      }
    }
    closesocket(client_socket);
  }
  closesocket(server_socket);
  WSACleanup();
  return 0;
}
