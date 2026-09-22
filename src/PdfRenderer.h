#pragma once
#include <cstdint>
#include <string>
#include <vector>

class PdfRenderer {
public:
  PdfRenderer();
  ~PdfRenderer();

  bool Open(const std::wstring& path);
  void Close();
  bool IsOpen() const;
  unsigned PageCount() const;
  const std::wstring& Path() const;

  // Returns decoded BGRA8 pixels for the requested page.
  bool RenderPage(unsigned pageIndex,
                  unsigned targetHeight,
                  std::vector<std::uint8_t>& bgra,
                  int& width,
                  int& height);

private:
  struct Impl;
  Impl* impl_;
  std::wstring path_;
  unsigned pageCount_ = 0;
};
