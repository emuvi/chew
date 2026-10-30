#include <windows.h>
#include <rpc.h>
#include <stdio.h>
#include <string.h>

#define UID_FILE "uid"

static int print_existing(void) {
  FILE *file = fopen(UID_FILE, "r");
  if (!file) return -1;
  char line[256];
  if (!fgets(line, sizeof(line), file)) {
    fclose(file);
    fprintf(stderr, "Could not read %s\n", UID_FILE);
    return 1;
  }
  fclose(file);
  line[strcspn(line, "\r\n")] = '\0';
  printf("%s\n", line);
  return 0;
}

int main(void) {
  int existing = print_existing();
  if (existing >= 0) return existing;

  UUID uuid;
  RPC_CSTR text = NULL;
  RPC_STATUS status = UuidCreate(&uuid);
  if ((status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) ||
      UuidToStringA(&uuid, &text) != RPC_S_OK) {
    fprintf(stderr, "Could not generate a unique id\n");
    return 1;
  }

  // CREATE_NEW avoids overwriting a uid created by a concurrent run.
  HANDLE file = CreateFileA(UID_FILE, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_EXISTS) {
      RpcStringFreeA(&text);
      return print_existing() == 0 ? 0 : 1;
    }
    fprintf(stderr, "Could not create %s (%lu)\n", UID_FILE, GetLastError());
    RpcStringFreeA(&text);
    return 1;
  }

  char line[64];
  int length = snprintf(line, sizeof(line), "%s\n", (const char *)text);
  DWORD written = 0;
  int ok = WriteFile(file, line, (DWORD)length, &written, NULL) &&
           written == (DWORD)length;
  CloseHandle(file);
  if (!ok) {
    fprintf(stderr, "Could not write %s (%lu)\n", UID_FILE, GetLastError());
    DeleteFileA(UID_FILE);
    RpcStringFreeA(&text);
    return 1;
  }

  printf("Created %s: %s\n", UID_FILE, (const char *)text);
  RpcStringFreeA(&text);
  return 0;
}
