// Windows shell headers must be included before FFGL/GLEW headers.
#include <windows.h>
#include <shobjidl.h>

#include "BRPdfReader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
  PluginFactory<BRPdfReader>,
  "BRP1",
  "BR PDF READER by Belajar Resolume",
  2, 1,
  1, 0,
  FF_SOURCE,
  "PDF reader and auto slideshow source for Resolume",
  "BR PDF READER V1 | Belajar Resolume"
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
uniform sampler2D sourceTexture;
uniform float sourceAspect;
uniform float outputAspect;
uniform int autoFit;
in vec2 uv;
out vec4 fragColor;

void main() {
  vec2 q = uv;
  bool outside = false;

  if (autoFit == 1) {
    if (outputAspect > sourceAspect) {
      float width = sourceAspect / outputAspect;
      outside = abs(uv.x - 0.5) > 0.5 * width;
      q.x = (uv.x - (0.5 - 0.5 * width)) / max(width, 0.0001);
    } else {
      float height = outputAspect / sourceAspect;
      outside = abs(uv.y - 0.5) > 0.5 * height;
      q.y = (uv.y - (0.5 - 0.5 * height)) / max(height, 0.0001);
    }
  }

  if (outside) {
    fragColor = vec4(0.0);
    return;
  }

  // WIC rows are top-to-bottom; OpenGL UV origin is bottom-left.
  fragColor = texture(sourceTexture, vec2(q.x, 1.0 - q.y));
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
  params_[PT_AUTO_FIT] = 1.0f;

  SetParamInfo(PT_CHOOSE_PDF, "Choose PDF", FF_TYPE_EVENT, false);
  SetParamInfo(PT_PREVIOUS, "Previous", FF_TYPE_EVENT, false);
  SetParamInfo(PT_NEXT, "Next", FF_TYPE_EVENT, false);

  SetParamInfo(PT_AUTO_SLIDE, "Auto Slide", FF_TYPE_BOOLEAN, false);
  SetParamInfo(PT_INTERVAL, "Interval Seconds", FF_TYPE_STANDARD, 5.0f);

  SetOptionParamInfo(PT_END_MODE, "When Finished", 2, 0);
  SetParamElementInfo(PT_END_MODE, 0, "Stop at Last Page", 0.0f);
  SetParamElementInfo(PT_END_MODE, 1, "Loop to First Page", 1.0f);

  SetParamInfo(PT_AUTO_FIT, "Auto Fit", FF_TYPE_BOOLEAN, true);

  SetParamGroup(PT_CHOOSE_PDF, "PDF FILE");
  SetParamGroup(PT_PREVIOUS, "SLIDE CONTROL");
  SetParamGroup(PT_NEXT, "SLIDE CONTROL");
  SetParamGroup(PT_AUTO_SLIDE, "AUTO SLIDE");
  SetParamGroup(PT_INTERVAL, "AUTO SLIDE");
  SetParamGroup(PT_END_MODE, "AUTO SLIDE");
  SetParamGroup(PT_AUTO_FIT, "DISPLAY");

  lastAdvance_ = std::chrono::steady_clock::now();
}

BRPdfReader::~BRPdfReader() = default;

void BRPdfReader::ChoosePdf() {
  HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool uninitialize = SUCCEEDED(initHr);

  IFileOpenDialog* dialog = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog));
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
  dialog->SetTitle(L"BR PDF READER by Belajar Resolume - Choose PDF");

  hr = dialog->Show(nullptr);
  if (SUCCEEDED(hr)) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        if (pdf_.Open(path)) {
          currentPage_ = 0;
          params_[PT_AUTO_SLIDE] = 0.0f;
          MarkPageChanged();
        }
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }

  dialog->Release();
  if (uninitialize) CoUninitialize();
}

void BRPdfReader::MarkPageChanged() {
  pageDirty_ = true;
  lastAdvance_ = std::chrono::steady_clock::now();
}

void BRPdfReader::Previous() {
  if (!pdf_.IsOpen() || currentPage_ == 0) return;
  --currentPage_;
  MarkPageChanged();
}

void BRPdfReader::Next() {
  if (!pdf_.IsOpen()) return;
  if (currentPage_ + 1 < pdf_.PageCount()) {
    ++currentPage_;
    MarkPageChanged();
  }
}

void BRPdfReader::AdvanceAuto() {
  if (params_[PT_AUTO_SLIDE] < 0.5f || !pdf_.IsOpen())
    return;

  const float seconds = std::clamp(params_[PT_INTERVAL], 0.25f, 120.0f);
  const auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration<float>(now - lastAdvance_).count() < seconds)
    return;

  lastAdvance_ = now;

  if (currentPage_ + 1 < pdf_.PageCount()) {
    ++currentPage_;
    pageDirty_ = true;
    return;
  }

  if (static_cast<int>(std::lround(params_[PT_END_MODE])) == 1) {
    currentPage_ = 0;
    pageDirty_ = true;
  } else {
    // Stop at the final page.
    params_[PT_AUTO_SLIDE] = 0.0f;
  }
}

void BRPdfReader::RenderCurrentPage() {
  if (!pageDirty_ || !pdf_.IsOpen())
    return;

  int width = 0;
  int height = 0;
  if (!pdf_.RenderPage(currentPage_, 1080, pixels_, width, height))
    return;

  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
               width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels_.data());

  textureWidth_ = width;
  textureHeight_ = height;
  pageDirty_ = false;
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
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));

  glBindVertexArray(0);

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  return FF_SUCCESS;
}

FFResult BRPdfReader::ProcessOpenGL(ProcessOpenGLStruct*) {
  AdvanceAuto();
  RenderCurrentPage();

  if (textureWidth_ <= 0 || textureHeight_ <= 0)
    return FF_SUCCESS;

  glUseProgram(program_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);

  glUniform1i(glGetUniformLocation(program_, "sourceTexture"), 0);
  glUniform1f(glGetUniformLocation(program_, "sourceAspect"),
              static_cast<float>(textureWidth_) / textureHeight_);
  glUniform1f(glGetUniformLocation(program_, "outputAspect"),
              viewport_.height > 0
                ? static_cast<float>(viewport_.width) / viewport_.height
                : 16.0f / 9.0f);
  glUniform1i(glGetUniformLocation(program_, "autoFit"),
              params_[PT_AUTO_FIT] >= 0.5f ? 1 : 0);

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);
  glUseProgram(0);

  return FF_SUCCESS;
}

FFResult BRPdfReader::DeInitGL() {
  if (texture_) glDeleteTextures(1, &texture_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (program_) glDeleteProgram(program_);
  texture_ = vao_ = vbo_ = program_ = 0;
  return FF_SUCCESS;
}

FFResult BRPdfReader::SetFloatParameter(unsigned int index, float value) {
  if (index >= PT_COUNT) return FF_FAIL;
  params_[index] = value;

  if (index == PT_CHOOSE_PDF && value > 0.5f) ChoosePdf();
  else if (index == PT_PREVIOUS && value > 0.5f) Previous();
  else if (index == PT_NEXT && value > 0.5f) Next();
  else if (index == PT_AUTO_SLIDE && value > 0.5f)
    lastAdvance_ = std::chrono::steady_clock::now();

  return FF_SUCCESS;
}

float BRPdfReader::GetFloatParameter(unsigned int index) {
  return index < PT_COUNT ? params_[index] : 0.0f;
}

char* BRPdfReader::GetParameterDisplay(unsigned int index) {
  static char display[64];

  if (index == PT_INTERVAL) {
    std::snprintf(display, sizeof(display), "%.1f sec",
                  std::clamp(params_[PT_INTERVAL], 0.25f, 120.0f));
    return display;
  }

  if (index == PT_PREVIOUS || index == PT_NEXT) {
    if (pdf_.IsOpen())
      std::snprintf(display, sizeof(display), "Page %u / %u",
                    currentPage_ + 1, pdf_.PageCount());
    else
      std::snprintf(display, sizeof(display), "No PDF");
    return display;
  }

  return CFFGLPlugin::GetParameterDisplay(index);
}
