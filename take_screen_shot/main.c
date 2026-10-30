#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int save_png(const char *path, const BYTE *pixels, int width, int height) {
  if (width <= 0 || height <= 0 ||
      (size_t)width > UINT_MAX / 4 / (size_t)height) {
    fprintf(stderr, "Invalid screenshot dimensions\n");
    return 0;
  }

  UINT stride = (UINT)width * 4;
  UINT image_size = stride * (UINT)height;
  BYTE *top_down = malloc(image_size);
  int path_length = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
  WCHAR *wide_path = path_length ? malloc((size_t)path_length * sizeof(WCHAR)) : NULL;
  if (!top_down || !wide_path) {
    fprintf(stderr, "Could not allocate memory to save screenshot\n");
    free(top_down);
    free(wide_path);
    return 0;
  }
  if (!MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, path_length)) {
    fprintf(stderr, "Could not convert output path: %s\n", path);
    free(top_down);
    free(wide_path);
    return 0;
  }

  for (int row = 0; row < height; row++) {
    memcpy(top_down + (size_t)row * stride,
           pixels + (size_t)(height - row - 1) * stride, stride);
    for (int column = 0; column < width; column++)
      top_down[(size_t)row * stride + (size_t)column * 4 + 3] = 255;
  }

  HRESULT hr = CoInitialize(NULL);
  if (FAILED(hr)) {
    fprintf(stderr, "COM initialization failed (0x%08lx)\n", (unsigned long)hr);
    free(top_down);
    free(wide_path);
    return 0;
  }

  IWICImagingFactory *factory = NULL;
  IWICStream *stream = NULL;
  IWICBitmapEncoder *encoder = NULL;
  IWICBitmapFrameEncode *frame = NULL;
  IPropertyBag2 *properties = NULL;
  int ok = 0;

  hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IWICImagingFactory, (void **)&factory);
  if (SUCCEEDED(hr))
    hr = IWICImagingFactory_CreateStream(factory, &stream);
  if (SUCCEEDED(hr))
    hr = IWICStream_InitializeFromFilename(stream, wide_path, GENERIC_WRITE);
  if (SUCCEEDED(hr))
    hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng,
                                          NULL, &encoder);
  if (SUCCEEDED(hr))
    hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream,
                                      WICBitmapEncoderNoCache);
  if (SUCCEEDED(hr))
    hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &properties);
  if (SUCCEEDED(hr))
    hr = IWICBitmapFrameEncode_Initialize(frame, properties);
  if (SUCCEEDED(hr))
    hr = IWICBitmapFrameEncode_SetSize(frame, (UINT)width, (UINT)height);
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  if (SUCCEEDED(hr))
    hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
  if (SUCCEEDED(hr) && !IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGRA))
    hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
  if (SUCCEEDED(hr))
    hr = IWICBitmapFrameEncode_WritePixels(frame, (UINT)height, stride,
                                           image_size, top_down);
  if (SUCCEEDED(hr))
    hr = IWICBitmapFrameEncode_Commit(frame);
  if (SUCCEEDED(hr))
    hr = IWICBitmapEncoder_Commit(encoder);
  if (FAILED(hr))
    fprintf(stderr, "PNG encoding failed (0x%08lx)\n", (unsigned long)hr);
  else
    ok = 1;

  if (properties) IPropertyBag2_Release(properties);
  if (frame) IWICBitmapFrameEncode_Release(frame);
  if (encoder) IWICBitmapEncoder_Release(encoder);
  if (stream) IWICStream_Release(stream);
  if (factory) IWICImagingFactory_Release(factory);
  CoUninitialize();
  free(top_down);
  free(wide_path);
  return ok;
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "screenshot.png";

  SetProcessDPIAware();
  int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  HDC screen = GetDC(NULL);
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *bits = NULL;
  HBITMAP dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!dib) {
    fprintf(stderr, "CreateDIBSection failed (%lu)\n", GetLastError());
    return 1;
  }
  HGDIOBJ old = SelectObject(mem, dib);

  int code = 0;
  if (!BitBlt(mem, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT)) {
    fprintf(stderr, "BitBlt failed (%lu)\n", GetLastError());
    code = 1;
  } else if (!save_png(path, (const BYTE *)bits, width, height)) {
    fprintf(stderr, "Could not write %s\n", path);
    code = 1;
  } else {
    printf("Saved %dx%d screenshot to %s\n", width, height, path);
  }

  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(NULL, screen);
  return code;
}
