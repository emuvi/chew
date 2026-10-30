#include <windows.h>
#include <winhttp.h>
#include <stdio.h>

int main(void) {
  int code = 1;
  HINTERNET session = NULL, connect = NULL, request = NULL;

  session = WinHttpOpen(L"call_tel_ping/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    fprintf(stderr, "WinHttpOpen failed (%lu)\n", GetLastError());
    goto done;
  }
  connect = WinHttpConnect(session, L"localhost", 8080, 0);
  if (!connect) {
    fprintf(stderr, "WinHttpConnect failed (%lu)\n", GetLastError());
    goto done;
  }
  request = WinHttpOpenRequest(connect, L"GET", L"/tel/ping", NULL,
                               WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!request) {
    fprintf(stderr, "WinHttpOpenRequest failed (%lu)\n", GetLastError());
    goto done;
  }
  if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request, NULL)) {
    fprintf(stderr, "Request failed (%lu)\n", GetLastError());
    goto done;
  }

  DWORD status = 0, status_size = sizeof(status);
  WinHttpQueryHeaders(request,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                      WINHTTP_NO_HEADER_INDEX);

  char buffer[4096];
  DWORD read = 0;
  while (WinHttpReadData(request, buffer, sizeof(buffer), &read) && read > 0)
    fwrite(buffer, 1, read, stdout);
  printf("\n");

  code = (status >= 200 && status < 300) ? 0 : 1;
  if (code) fprintf(stderr, "HTTP status %lu\n", (unsigned long)status);

done:
  if (request) WinHttpCloseHandle(request);
  if (connect) WinHttpCloseHandle(connect);
  if (session) WinHttpCloseHandle(session);
  return code;
}
