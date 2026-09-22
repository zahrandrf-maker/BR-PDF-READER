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
  char* GetParameterDisplay(unsigned int index) override;

private:
  enum Param : unsigned int {
    PT_BRANDING = 0,
    PT_CHOOSE_PDF,
    PT_PREVIOUS,
    PT_NEXT,
    PT_MANUAL_TRANSITION,
    PT_MANUAL_SPEED,
    PT_AUTO_SLIDE,
    PT_INTERVAL,
    PT_END_MODE,
    PT_AUTO_TRANSITION,
    PT_AUTO_SPEED,
    PT_DISPLAY_MODE,
    PT_ZOOM,
    PT_POS_X,
    PT_POS_Y,
    PT_COUNT
  };

  enum TransitionMode {
    TM_NORMAL = 0,
    TM_FADE,
    TM_SLIDE_UP,
    TM_SLIDE_DOWN,
    TM_SLIDE_LEFT,
    TM_SLIDE_RIGHT
  };

  void ChoosePdf();
  void Previous();
  void Next();
  void AdvanceAuto();
  void RenderCurrentPage();
  void MarkPageChanged(int transitionMode, int speedMode);
  float TransitionProgress() const;
  float TransitionDuration() const;

  std::array<float, PT_COUNT> params_{};
  PdfRenderer pdf_;
  unsigned currentPage_ = 0;
  bool pageDirty_ = false;

  int pendingTransitionMode_ = TM_NORMAL;
  int pendingSpeedMode_ = 0;
  int activeTransitionMode_ = TM_NORMAL;
  int activeSpeedMode_ = 0;

  std::chrono::steady_clock::time_point lastAdvance_;
  std::chrono::steady_clock::time_point transitionStarted_;
  bool transitionActive_ = false;

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
