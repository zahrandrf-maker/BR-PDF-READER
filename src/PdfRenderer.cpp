#include "PdfRenderer.h"

#include <windows.h>
#include <wincodec.h>
#include <objidl.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Data.Pdf.h>

#include <algorithm>
#include <cstring>

using namespace winrt;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Data::Pdf;

namespace {

bool DecodePngBytes(const std::vector<std::uint8_t>& png,
                    std::vector<std::uint8_t>& bgra,
                    int& width,
                    int& height) {
  if (png.empty()) return false;

  IStream* rawStream = nullptr;
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, png.size());
  if (!memory) return false;

  void* ptr = GlobalLock(memory);
  if (!ptr) {
    GlobalFree(memory);
    return false;
  }
  std::memcpy(ptr, png.data(), png.size());
  GlobalUnlock(memory);

  if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &rawStream)) || !rawStream) {
    GlobalFree(memory);
    return false;
  }

  winrt::com_ptr<IStream> stream;
  stream.attach(rawStream);

  winrt::com_ptr<IWICImagingFactory> factory;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(factory.put()))))
    return false;

  winrt::com_ptr<IWICBitmapDecoder> decoder;
  if (FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr,
                                              WICDecodeMetadataCacheOnLoad,
                                              decoder.put())))
    return false;

  winrt::com_ptr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, frame.put())))
    return false;

  UINT w = 0, h = 0;
  if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0)
    return false;

  winrt::com_ptr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(converter.put())))
    return false;

  if (FAILED(converter->Initialize(frame.get(),
                                   GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0,
                                   WICBitmapPaletteTypeCustom)))
    return false;

  const UINT stride = w * 4;
  bgra.resize(static_cast<size_t>(stride) * h);

  if (FAILED(converter->CopyPixels(nullptr,
                                   stride,
                                   static_cast<UINT>(bgra.size()),
                                   bgra.data())))
    return false;

  width = static_cast<int>(w);
  height = static_cast<int>(h);
  return true;
}

}

struct PdfRenderer::Impl {
  PdfDocument document{nullptr};
};

PdfRenderer::PdfRenderer() : impl_(new Impl) {
  // C++/WinRT gracefully handles the case where the host already initialized COM.
  try {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
  } catch (...) {
  }
}

PdfRenderer::~PdfRenderer() {
  Close();
  delete impl_;
}

bool PdfRenderer::Open(const std::wstring& path) {
  try {
    Close();

    StorageFile file = StorageFile::GetFileFromPathAsync(path).get();
    PdfDocument document = PdfDocument::LoadFromFileAsync(file).get();

    const unsigned count = document.PageCount();
    if (count == 0)
      return false;

    impl_->document = document;
    path_ = path;
    pageCount_ = count;
    return true;
  } catch (...) {
    Close();
    return false;
  }
}

void PdfRenderer::Close() {
  if (impl_)
    impl_->document = nullptr;
  path_.clear();
  pageCount_ = 0;
}

bool PdfRenderer::IsOpen() const {
  return impl_ && impl_->document && pageCount_ > 0;
}

unsigned PdfRenderer::PageCount() const {
  return pageCount_;
}

const std::wstring& PdfRenderer::Path() const {
  return path_;
}

bool PdfRenderer::RenderPage(unsigned pageIndex,
                             unsigned targetHeight,
                             std::vector<std::uint8_t>& bgra,
                             int& width,
                             int& height) {
  if (!IsOpen() || pageIndex >= pageCount_)
    return false;

  try {
    PdfPage page = impl_->document.GetPage(pageIndex);
    const auto nativeSize = page.Size();

    if (nativeSize.Width <= 0.0f || nativeSize.Height <= 0.0f)
      return false;

    const unsigned outH = std::max(64u, targetHeight);
    const unsigned outW = std::max(
      64u,
      static_cast<unsigned>(
        nativeSize.Width * (static_cast<float>(outH) / nativeSize.Height)
      )
    );

    InMemoryRandomAccessStream stream;
    PdfPageRenderOptions options;
    options.DestinationWidth(outW);
    options.DestinationHeight(outH);

    page.RenderToStreamAsync(stream, options).get();

    const std::uint64_t size64 = stream.Size();
    if (size64 == 0 || size64 > 0x7fffffffULL)
      return false;

    const std::uint32_t size = static_cast<std::uint32_t>(size64);
    IInputStream input = stream.GetInputStreamAt(0);

    DataReader reader(input);
    const std::uint32_t loaded = reader.LoadAsync(size).get();
    if (loaded == 0)
      return false;

    std::vector<std::uint8_t> png(loaded);
    reader.ReadBytes(png);

    return DecodePngBytes(png, bgra, width, height);
  } catch (...) {
    return false;
  }
}
