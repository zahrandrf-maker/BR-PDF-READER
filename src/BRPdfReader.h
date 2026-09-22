#pragma once

#include "FFGLSDK.h"
#include "PdfRenderer.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

class BRPdfReader : public CFFGLPlugin {
public:
  BRPdfReader();
  ~BRPdfReader() override;

  FFResult InitGL(const FFGLViewportStruct* vp) override;
  FFResult ProcessOpenGL(ProcessOpenGLStruct* pGL) override;
  FFResult DeInitGL() override;

  FFResult SetFloatParameter(unsigned int index, float value) override;
  float GetFloatParameter(unsigned int index) override;

  FFResult SetTextParameter(unsigned int index, const char* value) override;
  char* GetTextParameter(unsigned int index) override;

  char* GetParameterDisplay(unsigned int index) override;

private:
  enum Param : unsigned int {
    PT_WATERMARK = 0,
    PT_CHOOSE_PDF,
    PT_PREVIOUS,
    PT_NEXT,
    PT_AUTO_SLIDE,
    PT_INTERVAL,
    PT_END_MODE,
    PT_TRANSITION,
    PT_DISPLAY_MODE,
    PT_COUNT
  };

  void ChoosePdf();
  void Previous();
  void Next();
  void AdvanceAuto();
  void RenderCurrentPage();
  void MarkPageChanged(bool useFade);
  float TransitionProgress() const;
  float TransitionDuration() const;

  std::array<float, PT_COUNT> params_{};
  PdfRenderer pdf_;

  unsigned currentPage_ = 0;
  bool pageDirty_ = false;
  bool pendingFade_ = false;

  std::chrono::steady_clock::time_point lastAdvance_;
  std::chrono::steady_clock::time_point fadeStarted_;
  bool fadeActive_ = false;

  GLuint texture_ = 0;
  GLuint previousTexture_ = 0;
  GLuint vao_ = 0;
  GLuint vbo_ = 0;
  GLuint program_ = 0;

  int textureWidth_ = 0;
  int textureHeight_ = 0;
  int previousWidth_ = 0;
  int previousHeight_ = 0;

  FFGLViewportStruct viewport_{};
  std::vector<std::uint8_t> pixels_;
};
