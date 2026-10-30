#include <windows.h>
#include <rpc.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>

#define UID_FILE "uid"
#define UID_SIZE 64
#define HOST L"localhost"
#define PORT 8080
#define LIVE_PATH L"/tel/live"

static int read_uid(char *uid, size_t size) {
  FILE *file = fopen(UID_FILE, "r");
  if (!file) return -1;
  char *line = fgets(uid, (int)size, file);
  fclose(file);
  if (!line) {
    fprintf(stderr, "Could not read %s\n", UID_FILE);
    return 1;
  }
  uid[strcspn(uid, "\r\n")] = '\0';
  return uid[0] ? 0 : 1;
}

static int generate_uid(char *uid, size_t size) {
  UUID uuid;
  RPC_CSTR text = NULL;
  RPC_STATUS status = UuidCreate(&uuid);
  if ((status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) ||
      UuidToStringA(&uuid, &text) != RPC_S_OK) {
    fprintf(stderr, "Could not generate a unique id\n");
    return 1;
  }
  snprintf(uid, size, "%s", (const char *)text);
  RpcStringFreeA(&text);
  return 0;
}

static int write_uid(const char *uid) {
  // CREATE_NEW avoids overwriting a uid created by a concurrent run.
  HANDLE file = CreateFileA(UID_FILE, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_EXISTS) return -1;
    fprintf(stderr, "Could not create %s (%lu)\n", UID_FILE, GetLastError());
    return 1;
  }
  char line[UID_SIZE + 2];
  int length = snprintf(line, sizeof(line), "%s\n", uid);
  DWORD written = 0;
  int ok = WriteFile(file, line, (DWORD)length, &written, NULL) &&
           written == (DWORD)length;
  CloseHandle(file);
  if (!ok) {
    fprintf(stderr, "Could not write %s (%lu)\n", UID_FILE, GetLastError());
    DeleteFileA(UID_FILE);
    return 1;
  }
  return 0;
}

static int load_or_create_uid(char *uid, size_t size) {
  int result = read_uid(uid, size);
  if (result >= 0) return result;
  if (generate_uid(uid, size)) return 1;
  result = write_uid(uid);
  if (result == -1) return read_uid(uid, size) == 0 ? 0 : 1;
  return result;
}

static int build_live_path(const char *uid, wchar_t *path, size_t count) {
  int length = swprintf(path, count, L"%ls?uid=%hs", LIVE_PATH, uid);
  return length > 0 && (size_t)length < count ? 0 : 1;
}

static DWORD query_status(HINTERNET request) {
  DWORD status = 0, size = sizeof(status);
  WinHttpQueryHeaders(request,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                      WINHTTP_NO_HEADER_INDEX);
  return status;
}

static void print_body(HINTERNET request) {
  char buffer[4096];
  DWORD read = 0;
  while (WinHttpReadData(request, buffer, sizeof(buffer), &read) && read > 0)
    fwrite(buffer, 1, read, stdout);
  printf("\n");
}

static int call_live(const char *uid) {
  int code = 1;
  HINTERNET session = NULL, connect = NULL, request = NULL;
  wchar_t path[256];

  if (build_live_path(uid, path, sizeof(path) / sizeof(path[0]))) {
    fprintf(stderr, "Request path too long\n");
    return 1;
  }
  session = WinHttpOpen(L"id_call_tel_live/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    fprintf(stderr, "WinHttpOpen failed (%lu)\n", GetLastError());
    goto done;
  }
  connect = WinHttpConnect(session, HOST, PORT, 0);
  if (!connect) {
    fprintf(stderr, "WinHttpConnect failed (%lu)\n", GetLastError());
    goto done;
  }
  request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER,
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

  DWORD status = query_status(request);
  print_body(request);
  code = (status >= 200 && status < 300) ? 0 : 1;
  if (code) fprintf(stderr, "HTTP status %lu\n", (unsigned long)status);

done:
  if (request) WinHttpCloseHandle(request);
  if (connect) WinHttpCloseHandle(connect);
  if (session) WinHttpCloseHandle(session);
  return code;
}

int main(void) {
  char uid[UID_SIZE];
  if (load_or_create_uid(uid, sizeof(uid))) return 1;
  return call_live(uid);
}
