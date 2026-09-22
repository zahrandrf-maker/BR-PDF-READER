#include "PdfRenderer.h"

#include <windows.h>
#include <roapi.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.storage.h>
#include <windows.data.pdf.h>
#include <windows.storage.streams.h>
#include <wincodec.h>

#include <algorithm>
#include <chrono>
#include <thread>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;

using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageFileStatics;
using ABI::Windows::Storage::FileAccessMode;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::Streams::IInMemoryRandomAccessStream;
using ABI::Windows::Data::Pdf::IPdfDocument;
using ABI::Windows::Data::Pdf::IPdfDocumentStatics;
using ABI::Windows::Data::Pdf::IPdfPage;
using ABI::Windows::Data::Pdf::IPdfPageRenderOptions;

namespace {
template <typename TAsync>
bool WaitForAsync(TAsync* async) {
  if (!async) return false;
  AsyncStatus status = AsyncStatus::Started;
  while (status == AsyncStatus::Started) {
    if (FAILED(async->get_Status(&status))) return false;
    if (status == AsyncStatus::Started)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return status == AsyncStatus::Completed;
}

bool OpenDocument(const std::wstring& path, ComPtr<IPdfDocument>& document) {
  HString storageClass;
  if (FAILED(storageClass.Set(RuntimeClass_Windows_Storage_StorageFile))) return false;

  ComPtr<IStorageFileStatics> storageStatics;
  if (FAILED(RoGetActivationFactory(storageClass.Get(), IID_PPV_ARGS(&storageStatics)))) return false;

  HString filePath;
  if (FAILED(filePath.Set(path.c_str()))) return false;

  ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Storage::StorageFile*>> fileOp;
  if (FAILED(storageStatics->GetFileFromPathAsync(filePath.Get(), &fileOp)) || !WaitForAsync(fileOp.Get()))
    return false;

  ComPtr<IStorageFile> file;
  IStorageFile* fileRaw = nullptr;
  if (FAILED(fileOp->GetResults(&fileRaw)) || !fileRaw) return false;
  file.Attach(fileRaw);

  ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Storage::Streams::IRandomAccessStream*>> streamOp;
  if (FAILED(file->OpenAsync(FileAccessMode_Read, &streamOp)) || !WaitForAsync(streamOp.Get()))
    return false;

  ComPtr<IRandomAccessStream> stream;
  IRandomAccessStream* streamRaw = nullptr;
  if (FAILED(streamOp->GetResults(&streamRaw)) || !streamRaw) return false;
  stream.Attach(streamRaw);

  HString pdfClass;
  if (FAILED(pdfClass.Set(RuntimeClass_Windows_Data_Pdf_PdfDocument))) return false;

  ComPtr<IPdfDocumentStatics> pdfStatics;
  if (FAILED(RoGetActivationFactory(pdfClass.Get(), IID_PPV_ARGS(&pdfStatics)))) return false;

  ComPtr<ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Data::Pdf::PdfDocument*>> docOp;
  if (FAILED(pdfStatics->LoadFromStreamAsync(stream.Get(), &docOp)) || !WaitForAsync(docOp.Get()))
    return false;

  IPdfDocument* docRaw = nullptr;
  if (FAILED(docOp->GetResults(&docRaw)) || !docRaw) return false;
  document.Attach(docRaw);
  return true;
}

bool DecodePngStream(IStream* stream, std::vector<std::uint8_t>& bgra, int& width, int& height) {
  ComPtr<IWICImagingFactory> factory;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory)))) return false;

  LARGE_INTEGER zero{};
  stream->Seek(zero, STREAM_SEEK_SET, nullptr);

  ComPtr<IWICBitmapDecoder> decoder;
  if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
    return false;

  ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, &frame))) return false;

  UINT w = 0, h = 0;
  if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) return false;

  ComPtr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(&converter))) return false;

  if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom))) return false;

  const UINT stride = w * 4;
  bgra.resize(static_cast<size_t>(stride) * h);
  if (FAILED(converter->CopyPixels(nullptr, stride,
                                   static_cast<UINT>(bgra.size()), bgra.data())))
    return false;

  width = static_cast<int>(w);
  height = static_cast<int>(h);
  return true;
}
}

struct PdfRenderer::Impl {
  ComPtr<IPdfDocument> document;
};

PdfRenderer::PdfRenderer() : impl_(new Impl) {
  RoInitialize(RO_INIT_MULTITHREADED);
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

PdfRenderer::~PdfRenderer() {
  Close();
  delete impl_;
}

bool PdfRenderer::Open(const std::wstring& path) {
  Close();
  ComPtr<IPdfDocument> document;
  if (!OpenDocument(path, document)) return false;

  UINT32 count = 0;
  if (FAILED(document->get_PageCount(&count)) || count == 0) return false;

  impl_->document = document;
  path_ = path;
  pageCount_ = count;
  return true;
}

void PdfRenderer::Close() {
  if (impl_) impl_->document.Reset();
  path_.clear();
  pageCount_ = 0;
}

bool PdfRenderer::IsOpen() const { return impl_ && impl_->document && pageCount_ > 0; }
unsigned PdfRenderer::PageCount() const { return pageCount_; }
const std::wstring& PdfRenderer::Path() const { return path_; }

bool PdfRenderer::RenderPage(unsigned pageIndex,
                             unsigned targetHeight,
                             std::vector<std::uint8_t>& bgra,
                             int& width,
                             int& height) {
  if (!IsOpen() || pageIndex >= pageCount_) return false;

  ComPtr<IPdfPage> page;
  if (FAILED(impl_->document->GetPage(pageIndex, &page)) || !page) return false;

  ABI::Windows::Foundation::Size nativeSize{};
  if (FAILED(page->get_Size(&nativeSize)) || nativeSize.Width <= 0 || nativeSize.Height <= 0)
    return false;

  const unsigned outH = std::max(64u, targetHeight);
  const unsigned outW = std::max(64u, static_cast<unsigned>(
      nativeSize.Width * (static_cast<float>(outH) / nativeSize.Height)));

  HString memClass;
  if (FAILED(memClass.Set(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream)))
    return false;

  ComPtr<IInspectable> memInspectable;
  if (FAILED(RoActivateInstance(memClass.Get(), &memInspectable))) return false;

  ComPtr<IInMemoryRandomAccessStream> mem;
  if (FAILED(memInspectable.As(&mem))) return false;

  HString optionsClass;
  if (FAILED(optionsClass.Set(RuntimeClass_Windows_Data_Pdf_PdfPageRenderOptions)))
    return false;

  ComPtr<IInspectable> optionsInspectable;
  if (FAILED(RoActivateInstance(optionsClass.Get(), &optionsInspectable))) return false;

  ComPtr<IPdfPageRenderOptions> options;
  if (FAILED(optionsInspectable.As(&options))) return false;
  options->put_DestinationWidth(outW);
  options->put_DestinationHeight(outH);

  ComPtr<ABI::Windows::Foundation::IAsyncAction> renderAction;
  if (FAILED(page->RenderWithOptionsToStreamAsync(mem.Get(), options.Get(), &renderAction)) ||
      !WaitForAsync(renderAction.Get()))
    return false;

  // Windows.Data.Pdf writes PNG into the random-access stream.
  ComPtr<IStream> comStream;
  if (FAILED(CreateStreamOverRandomAccessStream(mem.Get(), IID_PPV_ARGS(&comStream))))
    return false;

  return DecodePngStream(comStream.Get(), bgra, width, height);
}
