// Windows shell headers must be included before FFGL/GLEW headers.
#include <windows.h>
#include <shobjidl.h>

#include "BRPdfReader.h"
#include "BRThumbnail.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ffglex;


// Static embedded thumbnail exposed to FFGL hosts such as Resolume.
// It is available before an instance is created, so the Source/clip can show
// the BR PDF Reader artwork immediately when dragged into the composition.
static CFFGLThumbnailInfo ThumbnailInfo(
  BR_THUMBNAIL_WIDTH,
  BR_THUMBNAIL_HEIGHT,
  BR_THUMBNAIL_PIXELS
);

static CFFGLPluginInfo PluginInfo(
  PluginFactory<BRPdfReader>,
  "BRP2",
  "BR PDF Reader",
  2, 1,
  1, 0,
  FF_SOURCE,
  "PDF reader and slideshow source for Resolume",
  "BR PDF Reader V1.4.4"
);

static const char* kVertexShader = R"(#version 410 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 texCoord;
out vec2 uv;
void main() {
  gl_Position = vec4(position, 0.0, 1.0);
  uv = texCoord;
}
)";

static const char* kFragmentShader = R"(#version 410 core
uniform sampler2D currentTexture;
uniform sampler2D previousTexture;

uniform float currentAspect;
uniform float previousAspect;
uniform float outputAspect;

uniform int displayMode;
uniform int hasPrevious;

uniform int transitionMode;
uniform float transitionProgress;

uniform float zoomValue;
uniform float positionX;
uniform float positionY;

in vec2 uv;
out vec4 fragColor;

vec2 transformedUV(vec2 p) {
  float z = max(zoomValue, 0.05);
  vec2 center = vec2(
    0.5 + positionX * 0.5,
    0.5 + positionY * 0.5
  );

  return (p - center) / z + vec2(0.5);
}

vec4 samplePage(sampler2D tex, float sourceAspect, vec2 p) {
  vec2 q = transformedUV(p);
  bool outside = false;

  // 0 = FIT
  if (displayMode == 0) {
    if (outputAspect > sourceAspect) {
      float width = sourceAspect / outputAspect;
      outside = outside || abs(q.x - 0.5) > 0.5 * width;
      q.x = (q.x - (0.5 - 0.5 * width)) / max(width, 0.0001);
    } else {
      float height = outputAspect / sourceAspect;
      outside = outside || abs(q.y - 0.5) > 0.5 * height;
      q.y = (q.y - (0.5 - 0.5 * height)) / max(height, 0.0001);
    }
  }

  // 1 = FILL
  else if (displayMode == 1) {
    if (outputAspect > sourceAspect) {
      q.y = (q.y - 0.5) * (sourceAspect / outputAspect) + 0.5;
    } else {
      q.x = (q.x - 0.5) * (outputAspect / sourceAspect) + 0.5;
    }
  }

  // 2 = STRETCH: use transformed UV directly.

  outside = outside ||
            q.x < 0.0 || q.x > 1.0 ||
            q.y < 0.0 || q.y > 1.0;

  if (outside)
    return vec4(0.0);

  return texture(tex, vec2(q.x, 1.0 - q.y));
}

void main() {
  float t = smoothstep(
    0.0,
    1.0,
    clamp(transitionProgress, 0.0, 1.0)
  );

  if (hasPrevious == 0 || transitionMode == 0 || t >= 1.0) {
    fragColor = samplePage(
      currentTexture,
      currentAspect,
      uv
    );
    return;
  }

  if (transitionMode == 1) {
    vec4 oldColor = samplePage(
      previousTexture,
      previousAspect,
      uv
    );

    vec4 newColor = samplePage(
      currentTexture,
      currentAspect,
      uv
    );

    fragColor = mix(oldColor, newColor, t);
    return;
  }

  vec2 oldOffset = vec2(0.0);
  vec2 newOffset = vec2(0.0);

  if (transitionMode == 2) {
    oldOffset = vec2(0.0, t);
    newOffset = vec2(0.0, t - 1.0);
  }

  if (transitionMode == 3) {
    oldOffset = vec2(0.0, -t);
    newOffset = vec2(0.0, 1.0 - t);
  }

  if (transitionMode == 4) {
    oldOffset = vec2(-t, 0.0);
    newOffset = vec2(1.0 - t, 0.0);
  }

  if (transitionMode == 5) {
    oldOffset = vec2(t, 0.0);
    newOffset = vec2(t - 1.0, 0.0);
  }

  vec4 oldColor = samplePage(
    previousTexture,
    previousAspect,
    uv - oldOffset
  );

  vec4 newColor = samplePage(
    currentTexture,
    currentAspect,
    uv - newOffset
  );

  fragColor = oldColor + newColor * (1.0 - oldColor.a);
}
)";

static GLuint CompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

BRPdfReader::BRPdfReader() {
  SetMinInputs(0);
  SetMaxInputs(0);

  params_.fill(0.0f);
  params_[PT_INTERVAL] = 5.0f;
  params_[PT_ZOOM] = 1.0f;

  SetParamInfo(PT_CHOOSE_PDF, "Choose PDF", FF_TYPE_EVENT, false);

  SetParamInfo(PT_PREVIOUS, "Previous", FF_TYPE_EVENT, false);
  SetParamInfo(PT_NEXT, "Next", FF_TYPE_EVENT, false);

  SetOptionParamInfo(PT_MANUAL_TRANSITION, "Manual Mode", 6, 0.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 0, "NORMAL", 0.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 1, "FADE", 1.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 2, "SLIDE UP", 2.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 3, "SLIDE DOWN", 3.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 4, "SLIDE LEFT", 4.0f);
  SetParamElementInfo(PT_MANUAL_TRANSITION, 5, "SLIDE RIGHT", 5.0f);

  SetOptionParamInfo(PT_MANUAL_SPEED, "Manual Speed", 2, 0.0f);
  SetParamElementInfo(PT_MANUAL_SPEED, 0, "FAST", 0.0f);
  SetParamElementInfo(PT_MANUAL_SPEED, 1, "SLOW", 1.0f);

  SetParamInfo(PT_AUTO_SLIDE, "Auto Slide", FF_TYPE_BOOLEAN, false);
  SetParamInfo(PT_INTERVAL, "Interval", FF_TYPE_STANDARD, 5.0f);
  SetParamRange(PT_INTERVAL, 1.0f, 60.0f);

  SetOptionParamInfo(PT_END_MODE, "When Finished", 2, 0.0f);
  SetParamElementInfo(PT_END_MODE, 0, "Stop at Last Page", 0.0f);
  SetParamElementInfo(PT_END_MODE, 1, "Loop to First Page", 1.0f);

  SetOptionParamInfo(PT_AUTO_TRANSITION, "Auto Mode", 6, 0.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 0, "NORMAL", 0.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 1, "FADE", 1.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 2, "SLIDE UP", 2.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 3, "SLIDE DOWN", 3.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 4, "SLIDE LEFT", 4.0f);
  SetParamElementInfo(PT_AUTO_TRANSITION, 5, "SLIDE RIGHT", 5.0f);

  SetOptionParamInfo(PT_AUTO_SPEED, "Auto Speed", 2, 0.0f);
  SetParamElementInfo(PT_AUTO_SPEED, 0, "FAST", 0.0f);
  SetParamElementInfo(PT_AUTO_SPEED, 1, "SLOW", 1.0f);

  // Keep internal parameter names unique for Resolume serialization,
  // but show the compact labels requested in the UI.
  SetParamDisplayName(PT_MANUAL_TRANSITION, "Mode", false);
  SetParamDisplayName(PT_MANUAL_SPEED, "Speed", false);
  SetParamDisplayName(PT_AUTO_TRANSITION, "Mode", false);
  SetParamDisplayName(PT_AUTO_SPEED, "Speed", false);

  SetOptionParamInfo(PT_DISPLAY_MODE, "Display Mode", 3, 0.0f);
  SetParamElementInfo(PT_DISPLAY_MODE, 0, "FIT", 0.0f);
  SetParamElementInfo(PT_DISPLAY_MODE, 1, "FILL", 1.0f);
  SetParamElementInfo(PT_DISPLAY_MODE, 2, "STRETCH", 2.0f);

  SetParamInfo(PT_ZOOM, "Zoom", FF_TYPE_STANDARD, 1.0f);
  SetParamRange(PT_ZOOM, 0.25f, 4.0f);

  SetParamInfo(PT_POS_X, "Position X", FF_TYPE_STANDARD, 0.0f);
  SetParamRange(PT_POS_X, -1.0f, 1.0f);

  SetParamInfo(PT_POS_Y, "Position Y", FF_TYPE_STANDARD, 0.0f);
  SetParamRange(PT_POS_Y, -1.0f, 1.0f);

  SetParamGroup(PT_CHOOSE_PDF, "by Belajar Resolume | PDF FILE");

  SetParamGroup(PT_PREVIOUS, "SLIDE CONTROL");
  SetParamGroup(PT_NEXT, "SLIDE CONTROL");
  SetParamGroup(PT_MANUAL_TRANSITION, "SLIDE CONTROL");
  SetParamGroup(PT_MANUAL_SPEED, "SLIDE CONTROL");

  SetParamGroup(PT_AUTO_SLIDE, "AUTO SLIDE");
  SetParamGroup(PT_INTERVAL, "AUTO SLIDE");
  SetParamGroup(PT_END_MODE, "AUTO SLIDE");
  SetParamGroup(PT_AUTO_TRANSITION, "AUTO SLIDE");
  SetParamGroup(PT_AUTO_SPEED, "AUTO SLIDE");

  SetParamGroup(PT_DISPLAY_MODE, "DISPLAY");

  SetParamGroup(PT_ZOOM, "TRANSFORM");
  SetParamGroup(PT_POS_X, "TRANSFORM");
  SetParamGroup(PT_POS_Y, "TRANSFORM");

  lastAdvance_ = std::chrono::steady_clock::now();
  transitionStarted_ = lastAdvance_;
}

BRPdfReader::~BRPdfReader() = default;

void BRPdfReader::ChoosePdf() {
  HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool uninitialize = SUCCEEDED(initHr);

  IFileOpenDialog* dialog = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));

  if (FAILED(hr) || !dialog) {
    if (uninitialize) CoUninitialize();
    return;
  }

  const COMDLG_FILTERSPEC filters[] = {
    { L"PDF Documents (*.pdf)", L"*.pdf" },
    { L"All Files (*.*)", L"*.*" }
  };

  dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
  dialog->SetDefaultExtension(L"pdf");
  dialog->SetTitle(L"BR PDF Reader - Choose PDF");

  hr = dialog->Show(nullptr);

  if (SUCCEEDED(hr)) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        if (pdf_.Open(path)) {
          currentPage_ = 0;
          params_[PT_AUTO_SLIDE] = 0.0f;
          previousWidth_ = previousHeight_ = 0;
          transitionActive_ = false;
          pageDirty_ = true;
          pendingTransitionMode_ = TM_NORMAL;
          pendingSpeedMode_ = 0;
          lastAdvance_ = std::chrono::steady_clock::now();
        }
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }

  dialog->Release();
  if (uninitialize) CoUninitialize();
}

void BRPdfReader::MarkPageChanged(int transitionMode, int speedMode) {
  pageDirty_ = true;
  pendingTransitionMode_ = transitionMode;
  pendingSpeedMode_ = speedMode;
  lastAdvance_ = std::chrono::steady_clock::now();
}

void BRPdfReader::Previous() {
  if (!pdf_.IsOpen() || currentPage_ == 0) return;
  --currentPage_;
  MarkPageChanged(
    static_cast<int>(std::lround(params_[PT_MANUAL_TRANSITION])),
    static_cast<int>(std::lround(params_[PT_MANUAL_SPEED]))
  );
}

void BRPdfReader::Next() {
  if (!pdf_.IsOpen()) return;
  if (currentPage_ + 1 < pdf_.PageCount()) {
    ++currentPage_;
    MarkPageChanged(
      static_cast<int>(std::lround(params_[PT_MANUAL_TRANSITION])),
      static_cast<int>(std::lround(params_[PT_MANUAL_SPEED]))
    );
  }
}

float BRPdfReader::TransitionDuration() const {
  if (activeTransitionMode_ == TM_NORMAL) return 0.0f;
  return activeSpeedMode_ == 1 ? 1.10f : 0.35f;
}

float BRPdfReader::TransitionProgress() const {
  if (!transitionActive_ || activeTransitionMode_ == TM_NORMAL) return 1.0f;
  const float duration = TransitionDuration();
  if (duration <= 0.0f) return 1.0f;

  const float elapsed = std::chrono::duration<float>(
    std::chrono::steady_clock::now() - transitionStarted_
  ).count();

  return std::clamp(elapsed / duration, 0.0f, 1.0f);
}

void BRPdfReader::AdvanceAuto() {
  if (params_[PT_AUTO_SLIDE] < 0.5f || !pdf_.IsOpen()) return;

  const float seconds = std::clamp(params_[PT_INTERVAL], 1.0f, 60.0f);
  const auto now = std::chrono::steady_clock::now();

  if (std::chrono::duration<float>(now - lastAdvance_).count() < seconds) return;

  lastAdvance_ = now;

  const int mode = static_cast<int>(std::lround(params_[PT_AUTO_TRANSITION]));
  const int speed = static_cast<int>(std::lround(params_[PT_AUTO_SPEED]));

  if (currentPage_ + 1 < pdf_.PageCount()) {
    ++currentPage_;
    pageDirty_ = true;
    pendingTransitionMode_ = mode;
    pendingSpeedMode_ = speed;
  } else if (static_cast<int>(std::lround(params_[PT_END_MODE])) == 1) {
    currentPage_ = 0;
    pageDirty_ = true;
    pendingTransitionMode_ = mode;
    pendingSpeedMode_ = speed;
  } else {
    params_[PT_AUTO_SLIDE] = 0.0f;
  }
}

void BRPdfReader::RenderCurrentPage() {
  if (!pageDirty_ || !pdf_.IsOpen()) return;

  int width = 0, height = 0;
  if (!pdf_.RenderPage(currentPage_, 1080, pixels_, width, height)) return;

  const bool alreadyHasPage = textureWidth_ > 0 && textureHeight_ > 0;

  if (!alreadyHasPage) {
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels_.data());
    textureWidth_ = width;
    textureHeight_ = height;
    transitionActive_ = false;
  } else {
    glBindTexture(GL_TEXTURE_2D, previousTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels_.data());

    std::swap(texture_, previousTexture_);

    previousWidth_ = textureWidth_;
    previousHeight_ = textureHeight_;
    textureWidth_ = width;
    textureHeight_ = height;

    activeTransitionMode_ = pendingTransitionMode_;
    activeSpeedMode_ = pendingSpeedMode_;
    transitionActive_ = activeTransitionMode_ != TM_NORMAL;
    transitionStarted_ = std::chrono::steady_clock::now();
  }

  pageDirty_ = false;
  pendingTransitionMode_ = TM_NORMAL;
  pendingSpeedMode_ = 0;
}

FFResult BRPdfReader::InitGL(const FFGLViewportStruct* vp) {
  viewport_ = *vp;

  GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  if (!vs || !fs) return FF_FAIL;

  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (!linked) return FF_FAIL;

  const float quad[] = {
    -1.f,-1.f, 0.f,0.f,
     1.f,-1.f, 1.f,0.f,
     1.f, 1.f, 1.f,1.f,
    -1.f, 1.f, 0.f,1.f
  };

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), reinterpret_cast<void*>(2*sizeof(float)));

  glBindVertexArray(0);

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenTextures(1, &previousTexture_);
  glBindTexture(GL_TEXTURE_2D, previousTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  return FF_SUCCESS;
}

FFResult BRPdfReader::ProcessOpenGL(ProcessOpenGLStruct*) {
  AdvanceAuto();
  RenderCurrentPage();

  if (textureWidth_ <= 0 || textureHeight_ <= 0) return FF_SUCCESS;

  float progress = TransitionProgress();
  if (progress >= 1.0f) transitionActive_ = false;

  glUseProgram(program_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glUniform1i(glGetUniformLocation(program_, "currentTexture"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, previousTexture_);
  glUniform1i(glGetUniformLocation(program_, "previousTexture"), 1);

  glUniform1f(glGetUniformLocation(program_, "currentAspect"),
              static_cast<float>(textureWidth_) / textureHeight_);

  glUniform1f(glGetUniformLocation(program_, "previousAspect"),
              previousHeight_ > 0 ? static_cast<float>(previousWidth_) / previousHeight_
                                  : static_cast<float>(textureWidth_) / textureHeight_);

  glUniform1f(glGetUniformLocation(program_, "outputAspect"),
              viewport_.height > 0 ? static_cast<float>(viewport_.width) / viewport_.height
                                   : 16.0f/9.0f);

  glUniform1i(glGetUniformLocation(program_, "displayMode"),
              static_cast<int>(std::lround(params_[PT_DISPLAY_MODE])));

  glUniform1i(glGetUniformLocation(program_, "hasPrevious"),
              previousWidth_ > 0 && previousHeight_ > 0 ? 1 : 0);

  glUniform1i(glGetUniformLocation(program_, "transitionMode"), activeTransitionMode_);
  glUniform1f(glGetUniformLocation(program_, "transitionProgress"), progress);

  glUniform1f(glGetUniformLocation(program_, "zoomValue"),
              std::clamp(params_[PT_ZOOM], 0.25f, 4.0f));
  glUniform1f(glGetUniformLocation(program_, "positionX"),
              std::clamp(params_[PT_POS_X], -1.0f, 1.0f));
  glUniform1f(glGetUniformLocation(program_, "positionY"),
              std::clamp(params_[PT_POS_Y], -1.0f, 1.0f));

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);

  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);

  return FF_SUCCESS;
}

FFResult BRPdfReader::DeInitGL() {
  if (texture_) glDeleteTextures(1, &texture_);
  if (previousTexture_) glDeleteTextures(1, &previousTexture_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);

  texture_ = previousTexture_ = vbo_ = vao_ = program_ = 0;
  return FF_SUCCESS;
}

FFResult BRPdfReader::SetFloatParameter(unsigned int index, float value) {
  if (index >= PT_COUNT) return FF_FAIL;

  params_[index] = value;

  if (index == PT_CHOOSE_PDF && value > 0.5f) ChoosePdf();
  else if (index == PT_PREVIOUS && value > 0.5f) Previous();
  else if (index == PT_NEXT && value > 0.5f) Next();
  else if (index == PT_AUTO_SLIDE && value > 0.5f) lastAdvance_ = std::chrono::steady_clock::now();
  else if (index == PT_INTERVAL) params_[PT_INTERVAL] = std::clamp(value, 1.0f, 60.0f);
  else if (index == PT_ZOOM) params_[PT_ZOOM] = std::clamp(value, 0.25f, 4.0f);
  else if (index == PT_POS_X) params_[PT_POS_X] = std::clamp(value, -1.0f, 1.0f);
  else if (index == PT_POS_Y) params_[PT_POS_Y] = std::clamp(value, -1.0f, 1.0f);

  return FF_SUCCESS;
}

float BRPdfReader::GetFloatParameter(unsigned int index) {
  if (index >= PT_COUNT) return 0.0f;
  return params_[index];
}

char* BRPdfReader::GetParameterDisplay(unsigned int index) {
  static char display[64];

  if (index == PT_INTERVAL) {
    std::snprintf(display, sizeof(display), "%.1f sec",
                  std::clamp(params_[PT_INTERVAL], 1.0f, 60.0f));
    return display;
  }

  if (index == PT_ZOOM) {
    std::snprintf(display, sizeof(display), "%.0f%%",
                  std::clamp(params_[PT_ZOOM], 0.25f, 4.0f) * 100.0f);
    return display;
  }

  if (index == PT_POS_X || index == PT_POS_Y) {
    std::snprintf(display, sizeof(display), "%+.2f",
                  std::clamp(params_[index], -1.0f, 1.0f));
    return display;
  }

  if (index == PT_PREVIOUS || index == PT_NEXT) {
    if (pdf_.IsOpen())
      std::snprintf(display, sizeof(display), "Page %u / %u", currentPage_ + 1, pdf_.PageCount());
    else
      std::snprintf(display, sizeof(display), "No PDF");
    return display;
  }

  return CFFGLPlugin::GetParameterDisplay(index);
}
