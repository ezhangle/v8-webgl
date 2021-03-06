// Copyright (c) 2012 Hewlett-Packard Development Company, L.P. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "typed_array.h"
#include "webgl_active_info.h"
#include "webgl_buffer.h"
#include "webgl_framebuffer.h"
#include "webgl_program.h"
#include "webgl_renderbuffer.h"
#include "webgl_rendering_context.h"
#include "webgl_shader.h"
#include "webgl_texture.h"
#include "webgl_uniform_location.h"

#include <string>
#include <sstream>
#include <vector>

namespace v8_webgl {

static inline v8::Handle<v8::Primitive> U() {
  return v8::Undefined();
}

// Helper for glUniform..v and glVertexAttrib..v callbacks
template<typename TNative>
class UVAHelper {
 public:
  v8::Handle<v8::Value> Process(WebGLRenderingContext* context, const v8::Arguments& args) {
    bool ok = true;
    context_ = context;

    int32_t index = ProcessArgs(args);
    if (index < 0)
      return U();

    v8::Handle<v8::Value> array_value = args[index];

    std::vector<TNative> vector;
    const TNative* array_data = NULL;
    uint32_t array_length = 0;
    if (array_value->IsUndefined() || array_value->IsNull()) {
      context_->set_gl_error(GL_INVALID_VALUE);
      return U();
    }
    else if (array_value->IsArray()) {
      vector = ArrayFromV8<TNative>(array_value, &ok);
      if (!ok) return U();
      array_data = &vector[0];
      array_length = vector.size();
    }
    else if (Array<TNative>::Type::HasInstance(array_value)) {
      typename Array<TNative>::Type* array = Array<TNative>::Type::FromV8Object(array_value->ToObject());
      if (!array)
        return ThrowObjectDisposed();
      array_data = array->GetTypedArrayData();
      array_length = array->GetTypedArrayLength();
    }
    else
      return ThrowTypeError();

    InvokeGL(array_length, array_data);
    return U();
  }

 protected:
  UVAHelper() : context_(NULL) {}
  // Return index of array arg, -1 on error
  virtual int ProcessArgs(const v8::Arguments& args) = 0;
  virtual void InvokeGL(uint32_t array_length, const TNative* array_data) = 0;
  WebGLRenderingContext* GetContext() { return context_; }

 private:
  WebGLRenderingContext* context_;
};

template<typename TNative>
class UniformHelper : public UVAHelper<TNative> {
 public:
  typedef void (*UniformCallback) (GLint, GLsizei, const TNative*);
  UniformHelper(UniformCallback gl_callback, uint32_t min_size)
      : UVAHelper<TNative>()
      , gl_callback_(gl_callback)
      , min_size_(min_size) {}

 protected:
  int ProcessArgs(const v8::Arguments& args) {
    location_id_ = 0;
    WebGLUniformLocation* location = this->GetContext()->UniformLocationFromV8(args[0]);
    if (!location) return -1;
    location_id_ = location->webgl_id();
    return 1;
  }
  void InvokeGL(uint32_t array_length, const TNative* array_data) {
    // Array must be at least min_size and a multiple of it
    if (array_length < min_size_ || (array_length % min_size_)) {
      this->GetContext()->set_gl_error(GL_INVALID_VALUE);
      return;
    }
    gl_callback_(location_id_, array_length / min_size_, array_data);
  }

 private:
  GLuint location_id_;
  UniformCallback gl_callback_;
  uint32_t min_size_;
};

template<typename TNative>
class UniformMatrixHelper : public UVAHelper<TNative> {
 public:
  typedef void (*UniformMatrixCallback) (GLint, GLsizei, GLboolean, const TNative*);
  UniformMatrixHelper(UniformMatrixCallback gl_callback, uint32_t min_size)
      : UVAHelper<TNative>()
      , gl_callback_(gl_callback)
      , min_size_(min_size) {}

 protected:
  int ProcessArgs(const v8::Arguments& args) {
    bool ok = true;
    location_id_ = 0;
    WebGLUniformLocation* location = this->GetContext()->UniformLocationFromV8(args[0]);
    if (!location) return -1;
    location_id_ = location->webgl_id();
    bool transpose = FromV8<bool>(args[1], &ok);
    if (!ok)
      return -1;
    // Must be GL_FALSE
    if (transpose) {
      this->GetContext()->set_gl_error(GL_INVALID_VALUE);
      return -1;
    }
    return 2;
  }
  void InvokeGL(uint32_t array_length, const TNative* array_data) {
    // Array must be at least min_size and a multiple of it
    if (array_length < min_size_ || (array_length % min_size_)) {
      this->GetContext()->set_gl_error(GL_INVALID_VALUE);
      return;
    }
    gl_callback_(location_id_, GL_FALSE, array_length / min_size_, array_data);
  }

 private:
  GLuint location_id_;
  UniformMatrixCallback gl_callback_;
  uint32_t min_size_;
};

template<typename TNative>
class VertexAttribHelper : public UVAHelper<TNative> {
 public:
  typedef void (*VertexAttribCallback) (GLuint, const TNative*);
  VertexAttribHelper(VertexAttribCallback gl_callback, uint32_t required_array_length)
      : UVAHelper<TNative>()
      , gl_callback_(gl_callback)
      , required_array_length_(required_array_length)  {}

 protected:
  int ProcessArgs(const v8::Arguments& args) {
    bool ok = true;
    index_ = FromV8<int32_t>(args[0], &ok);
    if (!ok)
      return -1;
    return 1;
  }
  void InvokeGL(uint32_t array_length, const TNative* array_data) {
    if (array_length < required_array_length_) {
      this->GetContext()->set_gl_error(GL_INVALID_VALUE);
      return;
    }
    gl_callback_(index_, array_data);
  }

 private:
  GLint index_;
  VertexAttribCallback gl_callback_;
  uint32_t required_array_length_;
};

//////

// WebGLContextAttributes getContextAttributes();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getContextAttributes(const v8::Arguments& args) {
  /*XXX finish, need to add WebGLContextAttributes class*/
  return U();
}

// boolean isContextLost();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isContextLost(const v8::Arguments& args) {
  return ToV8(false);
}

// DOMString[ ] getSupportedExtensions();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getSupportedExtensions(const v8::Arguments& args) { return U(); /*XXX finish*/ }

// object getExtension(DOMString name);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getExtension(const v8::Arguments& args) { return U(); /*XXX finish*/ }

// void activeTexture(GLenum texture);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_activeTexture(const v8::Arguments& args) {
  bool ok = true;
  GLenum texture = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glActiveTexture(texture);
  return U();
}

// void attachShader(WebGLProgram program, WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_attachShader(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[1], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  GLuint shader_id = shader->webgl_id();
  glAttachShader(program_id, shader_id);
  return U();
}

// void bindAttribLocation(WebGLProgram program, GLuint index, DOMString name);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bindAttribLocation(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLuint index = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  std::string name = FromV8<std::string>(args[2], &ok); if (!ok) return U();
  glBindAttribLocation(program_id, index, name.c_str());
  return U();
}

// void bindBuffer(GLenum target, WebGLBuffer buffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bindBuffer(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  WebGLBuffer* buffer = NativeFromV8<WebGLBuffer>(args[1], &ok); if (!ok) return U();
  if (!ValidateObject(buffer)) return U();
  GLuint buffer_id = buffer ? buffer->webgl_id() : 0;
  glBindBuffer(target, buffer_id);
  return U();
}

// void bindFramebuffer(GLenum target, WebGLFramebuffer framebuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bindFramebuffer(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (target != GL_FRAMEBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return U();
  }
  WebGLFramebuffer* framebuffer = NativeFromV8<WebGLFramebuffer>(args[1], &ok); if (!ok) return U();
  if (!ValidateObject(framebuffer)) return U();
  GLuint framebuffer_id = framebuffer ? framebuffer->webgl_id() : 0;
  //XXX glBindFramebufferEXT
  glBindFramebuffer(target, framebuffer_id);
  return U();
}

// void bindRenderbuffer(GLenum target, WebGLRenderbuffer renderbuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bindRenderbuffer(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (target != GL_RENDERBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return U();
  }
  WebGLRenderbuffer* renderbuffer = NativeFromV8<WebGLRenderbuffer>(args[1], &ok); if (!ok) return U();
  if (!ValidateObject(renderbuffer)) return U();
  GLuint renderbuffer_id = renderbuffer ? renderbuffer->webgl_id() : 0;
  //XXX glBindRenderbufferEXT
  glBindRenderbuffer(target, renderbuffer_id);
  return U();
}

// void bindTexture(GLenum target, WebGLTexture texture);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bindTexture(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  switch (target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  WebGLTexture* texture = NativeFromV8<WebGLTexture>(args[1], &ok); if (!ok) return U();
  if (!ValidateObject(texture)) return U();
  GLuint texture_id = texture ? texture->webgl_id() : 0;
  glBindTexture(target, texture_id);
  return U();
}

// void blendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_blendColor(const v8::Arguments& args) {
  bool ok = true;
  GLclampf red = FromV8<float>(args[0], &ok); if (!ok) return U();
  GLclampf green = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLclampf blue = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLclampf alpha = FromV8<float>(args[3], &ok); if (!ok) return U();
  glBlendColor(red, green, blue, alpha);
  return U();
 }

// void blendEquation(GLenum mode);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_blendEquation(const v8::Arguments& args) {
  bool ok = true;
  GLenum mode = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateBlendEquation("blendEquation", mode))
    return U();
  glBlendEquation(mode);
  return U();
}

// void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_blendEquationSeparate(const v8::Arguments& args) {
  bool ok = true;
  GLenum modeRGB = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateBlendEquation("blendEquationSeparate", modeRGB))
    return U();
  GLenum modeAlpha = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateBlendEquation("blendEquationSeparate", modeAlpha))
    return U();
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return U();
}

// void blendFunc(GLenum sfactor, GLenum dfactor);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_blendFunc(const v8::Arguments& args) {
  bool ok = true;
  GLenum sfactor = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum dfactor = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateBlendFuncFactors("blendFunc", sfactor, dfactor))
    return U();
  glBlendFunc(sfactor, dfactor);
  return U();
}

// void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, 
//                        GLenum srcAlpha, GLenum dstAlpha);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_blendFuncSeparate(const v8::Arguments& args) {
  bool ok = true;
  GLenum srcRGB = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum dstRGB = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateBlendFuncFactors("blendFuncSeparate", srcRGB, dstRGB))
    return U();
  GLenum srcAlpha = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  GLenum dstAlpha = FromV8<uint32_t>(args[3], &ok); if (!ok) return U();
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return U();
}

// void bufferData(GLenum target, GLsizeiptr size, GLenum usage);
// void bufferData(GLenum target, ArrayBufferView data, GLenum usage);
// void bufferData(GLenum target, ArrayBuffer data, GLenum usage);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bufferData(const v8::Arguments& args) {
  bool ok = true;
  if (args[1]->IsNull()) {
    set_gl_error(GL_INVALID_VALUE);
    return U();
  }
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  void* data = NULL;
  GLsizeiptr size = 0;
  uint32_t length = 0;
  if (TypedArrayToData(args[1], &data, &length, &ok))
    size = length;
  else {
    if (!ok)
      return U();
    size = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  }
  GLenum usage = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  if (!ValidateBufferDataParameters("bufferData", target, usage))
    return U();
  glBufferData(target, size, data, usage);
  return U();
}

// void bufferSubData(GLenum target, GLintptr offset, ArrayBufferView data);
// void bufferSubData(GLenum target, GLintptr offset, ArrayBuffer data);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_bufferSubData(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateBufferDataParameters("bufferSubData", target, GL_STATIC_DRAW))
    return U();
  GLintptr offset = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  void* data = NULL;
  GLsizeiptr size = 0;
  uint32_t length = 0;
  if (TypedArrayToData(args[2], &data, &length, &ok))
    size = length;
  else
    return U();

  glBufferSubData(target, offset, size, data);
  return U();
}

// GLenum checkFramebufferStatus(GLenum target);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_checkFramebufferStatus(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (target != GL_FRAMEBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return ToV8<uint32_t>(0);
  }
  return ToV8(glCheckFramebufferStatus(target));
}

// void clear(GLbitfield mask);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_clear(const v8::Arguments& args) {
  bool ok = true;
  GLbitfield mask = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
    set_gl_error(GL_INVALID_VALUE);
    return U();
  }
  glClear(mask);
  return U();
}

// void clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_clearColor(const v8::Arguments& args) {
  bool ok = true;
  GLclampf red = FromV8<float>(args[0], &ok); if (!ok) return U();
  GLclampf green = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLclampf blue = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLclampf alpha = FromV8<float>(args[3], &ok); if (!ok) return U();
  glClearColor(red, green, blue, alpha);
  return U();
}

// void clearDepth(GLclampf depth);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_clearDepth(const v8::Arguments& args) {
  bool ok = true;
  GLclampf depth = FromV8<float>(args[0], &ok); if (!ok) return U();
  glClearDepth(depth);
  return U();
}

// void clearStencil(GLint s);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_clearStencil(const v8::Arguments& args) {
  bool ok = true;
  GLint s = FromV8<int32_t>(args[0], &ok); if (!ok) return U();
  glClearStencil(s);
  return U();
}

// void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_colorMask(const v8::Arguments& args) {
  bool ok = true;
  GLboolean red = FromV8<bool>(args[0], &ok); if (!ok) return U();
  GLboolean green = FromV8<bool>(args[1], &ok); if (!ok) return U();
  GLboolean blue = FromV8<bool>(args[2], &ok); if (!ok) return U();
  GLboolean alpha = FromV8<bool>(args[3], &ok); if (!ok) return U();
  glColorMask(red, green, blue, alpha);
  return U();
}

// void compileShader(WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_compileShader(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  GLuint shader_id = shader->webgl_id();
  GLint shader_type = 0;
  glGetShaderiv(shader_id, GL_SHADER_TYPE, &shader_type);
  if (shader_type == 0)
    return U();

  std::string translated_source;
  std::string shader_log;
  bool is_valid = shader_compiler_.TranslateShaderSource
                  (shader->source().c_str(), shader_type,
                   &translated_source, &shader_log);

  shader->set_is_valid(is_valid);
  shader->set_log(shader_log);
  if (!is_valid)
    return U();

  // Compile translated source
  const char* shader_source[] = { translated_source.c_str() };
  glShaderSource(shader_id, 1, shader_source, NULL);
  glCompileShader(shader_id);

  //XXX should assert translated compile status is GL_TRUE
#if 0
  GLint compile_status = 0;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
#endif

  return U();
}

// void copyTexImage2D(GLenum target, GLint level, GLenum internalformat, 
//                     GLint x, GLint y, GLsizei width, GLsizei height, 
//                     GLint border);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_copyTexImage2D(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLint level = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLenum internalformat = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  GLint x = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[4], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[5], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[6], &ok); if (!ok) return U();
  GLint border = FromV8<int32_t>(args[7], &ok); if (!ok) return U();
  if (!ValidateTexFuncParameters("copyTexImage2D", target, level, internalformat, width, height, border, internalformat, GL_UNSIGNED_BYTE))
    return U();
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return U();
}

// void copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                        GLint x, GLint y, GLsizei width, GLsizei height);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_copyTexSubImage2D(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLint level = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLint xoffset = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLint yoffset = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  GLint x = FromV8<int32_t>(args[4], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[5], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[6], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[7], &ok); if (!ok) return U();
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return U();
}

// WebGLBuffer createBuffer();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createBuffer(const v8::Arguments& args) {
  GLuint buffer_id = 0;
  glGenBuffers(1, &buffer_id);
  WebGLBuffer* buffer = CreateBuffer(buffer_id);
  return buffer->ToV8Object();
}

// WebGLFramebuffer createFramebuffer();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createFramebuffer(const v8::Arguments& args) {
  GLuint framebuffer_id = 0;
  //XXX glGenFramebuffersEXT etc.
  glGenFramebuffers(1, &framebuffer_id);
  WebGLFramebuffer* framebuffer = CreateFramebuffer(framebuffer_id);
  return framebuffer->ToV8Object();
}

// WebGLProgram createProgram();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createProgram(const v8::Arguments& args) {
  GLuint program_id = glCreateProgram();
  WebGLProgram* program = CreateProgram(program_id);
  return program->ToV8Object();
}

// WebGLRenderbuffer createRenderbuffer();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createRenderbuffer(const v8::Arguments& args) {
  GLuint renderbuffer_id = 0;
  //XXX glGenRenderbuffersEXT etc.
  glGenRenderbuffers(1, &renderbuffer_id);
  WebGLRenderbuffer* renderbuffer = CreateRenderbuffer(renderbuffer_id);
  return renderbuffer->ToV8Object();
}

// WebGLShader createShader(GLenum type);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createShader(const v8::Arguments& args) {
  bool ok = true;
  GLenum type = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLuint shader_id = glCreateShader(type);
  WebGLShader* shader = CreateShader(shader_id);
  return shader->ToV8Object();
}

// WebGLTexture createTexture();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_createTexture(const v8::Arguments& args) {
  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  WebGLTexture* texture = CreateTexture(texture_id);
  return texture->ToV8Object();
}

// void cullFace(GLenum mode);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_cullFace(const v8::Arguments& args) {
  bool ok = true;
  GLenum mode = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glCullFace(mode);
  return U();
}

// void deleteBuffer(WebGLBuffer buffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteBuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLBuffer* buffer = NativeFromV8<WebGLBuffer>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(buffer)) return U();
  GLuint buffer_id = buffer ? buffer->webgl_id() : 0;
  glDeleteBuffers(1, &buffer_id);
  DeleteBuffer(buffer);
  return U();
}

// void deleteFramebuffer(WebGLFramebuffer framebuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteFramebuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLFramebuffer* framebuffer = NativeFromV8<WebGLFramebuffer>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(framebuffer)) return U();
  GLuint framebuffer_id = framebuffer ? framebuffer->webgl_id() : 0;
  //XXX glDeleteFramebuffersEXT etc.
  glDeleteFramebuffers(1, &framebuffer_id);
  DeleteFramebuffer(framebuffer);
  return U();
}

// void deleteProgram(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteProgram(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program ? program->webgl_id() : 0;
  glDeleteProgram(program_id);
  DeleteProgram(program);
  return U();
}

// void deleteRenderbuffer(WebGLRenderbuffer renderbuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteRenderbuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLRenderbuffer* renderbuffer = NativeFromV8<WebGLRenderbuffer>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(renderbuffer)) return U();
  GLuint renderbuffer_id = renderbuffer ? renderbuffer->webgl_id() : 0;
  //XXX glDeleteRenderbuffersEXT etc.
  glDeleteRenderbuffers(1, &renderbuffer_id);
  DeleteRenderbuffer(renderbuffer);
  return U();
}

// void deleteShader(WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteShader(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(shader)) return U();
  GLuint shader_id = shader ? shader->webgl_id() : 0;
  glDeleteShader(shader_id);
  DeleteShader(shader);
  return U();
}

// void deleteTexture(WebGLTexture texture);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_deleteTexture(const v8::Arguments& args) {
  bool ok = true;
  WebGLTexture* texture = NativeFromV8<WebGLTexture>(args[0], &ok); if (!ok) return U();
  if (!ValidateObject(texture)) return U();
  GLuint texture_id = texture ? texture->webgl_id() : 0;
  glDeleteTextures(1, &texture_id);
  DeleteTexture(texture);
  return U();
}

// void depthFunc(GLenum func);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_depthFunc(const v8::Arguments& args) {
  bool ok = true;
  GLenum func = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glDepthFunc(func);
  return U();
}

// void depthMask(GLboolean flag);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_depthMask(const v8::Arguments& args) {
  bool ok = true;
  GLboolean flag = FromV8<bool>(args[0], &ok); if (!ok) return U();
  glDepthMask(flag);
  return U();
}

// void depthRange(GLclampf zNear, GLclampf zFar);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_depthRange(const v8::Arguments& args) {
  bool ok = true;
  GLclampf zNear = FromV8<float>(args[0], &ok); if (!ok) return U();
  GLclampf zFar = FromV8<float>(args[1], &ok); if (!ok) return U();
  glDepthRange(zNear, zFar);
  return U();
}

// void detachShader(WebGLProgram program, WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_detachShader(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[1], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  GLuint shader_id = shader->webgl_id();
  glDetachShader(program_id, shader_id);
  return U();
}

// void disable(GLenum cap);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_disable(const v8::Arguments& args) {
  bool ok = true;
  GLenum cap = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateCapability("disable", cap))
    return U();
  glDisable(cap);
  return U();
}

// void disableVertexAttribArray(GLuint index);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_disableVertexAttribArray(const v8::Arguments& args) {
  bool ok = true;
  GLuint index = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glDisableVertexAttribArray(index);
  return U();
}

// void drawArrays(GLenum mode, GLint first, GLsizei count);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_drawArrays(const v8::Arguments& args) {
  bool ok = true;
  GLenum mode = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateDrawMode("drawArrays", mode))
    return U();
  GLint first = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLsizei count = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  glDrawArrays(mode, first, count);
  return U();
}

// void drawElements(GLenum mode, GLsizei count, GLenum type, GLintptr offset);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_drawElements(const v8::Arguments& args) {
  bool ok = true;
  GLenum mode = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateDrawMode("drawElements", mode))
    return U();
  GLsizei count = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLenum type = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  GLintptr offset = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
  return U();
}

// void enable(GLenum cap);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_enable(const v8::Arguments& args) {
  bool ok = true;
  GLenum cap = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateCapability("enable", cap))
    return U();
  glEnable(cap);
  return U();
}

// void enableVertexAttribArray(GLuint index);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_enableVertexAttribArray(const v8::Arguments& args) {
  bool ok = true;
  GLuint index = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glEnableVertexAttribArray(index);
  return U();
}

// void finish();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_finish(const v8::Arguments& args) {
  glFinish();
  return U();
}

// void flush();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_flush(const v8::Arguments& args) {
  glFlush();
  return U();
}

// void framebufferRenderbuffer(GLenum target, GLenum attachment, 
//                              GLenum renderbuffertarget, 
//                              WebGLRenderbuffer renderbuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_framebufferRenderbuffer(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum attachment = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateFramebufferFuncParameters("framebufferRenderbuffer", target, attachment))
    return U();
  GLenum renderbuffertarget = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  if (renderbuffertarget != GL_RENDERBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return U();
  }
  WebGLRenderbuffer* renderbuffer = NativeFromV8<WebGLRenderbuffer>(args[3], &ok); if (!ok) return U();
  if (!ValidateObject(renderbuffer)) return U();
  GLuint renderbuffer_id = renderbuffer ? renderbuffer->webgl_id() : 0;
  //XXX glFramebufferRenderbufferEXT
  glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer_id);
  return U();
}

// void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, 
//                           WebGLTexture texture, GLint level);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_framebufferTexture2D(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum attachment = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateFramebufferFuncParameters("framebufferTexture2D", target, attachment))
    return U();
  GLenum textarget = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  WebGLTexture* texture = NativeFromV8<WebGLTexture>(args[3], &ok); if (!ok) return U();
  if (!ValidateObject(texture)) return U();
  GLuint texture_id = texture ? texture->webgl_id() : 0;
  GLint level = FromV8<int32_t>(args[4], &ok); if (!ok) return U();
  //XXX glFramebufferTexture2DEXT
  glFramebufferTexture2D(target, attachment, textarget, texture_id, level);
  return U();
}

// void frontFace(GLenum mode);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_frontFace(const v8::Arguments& args) {
  bool ok = true;
  GLenum mode = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glFrontFace(mode);
  return U();
}

// void generateMipmap(GLenum target);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_generateMipmap(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  //XXX glGenerateMipmapEXT etc.
  glGenerateMipmap(target);
  return U();
}

// WebGLActiveInfo getActiveAttrib(WebGLProgram program, GLuint index);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getActiveAttrib(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLuint index = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();

  GLint max_name_length = 0;
  glGetProgramiv(program_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  std::vector<char> name_vec(max_name_length);
  GLsizei name_length = 0;
  GLint size = 0;
  GLenum type = 0;
  glGetActiveAttrib(program_id, index, max_name_length, &name_length, &size, &type, &name_vec[0]);
  WebGLActiveInfo* active_info = CreateActiveInfo(size, type, &name_vec[0]);
  return active_info->ToV8Object();
}

// WebGLActiveInfo getActiveUniform(WebGLProgram program, GLuint index);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getActiveUniform(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLuint index = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();

  GLint max_name_length = 0;
  glGetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  std::vector<char> name_vec(max_name_length);
  GLsizei name_length = 0;
  GLint size = 0;
  GLenum type = 0;
  glGetActiveUniform(program_id, index, max_name_length, &name_length, &size, &type, &name_vec[0]);
  // If it's an array and the name doesn't end with "[0]", then append it
  if (size > 1 && (name_length < 3
                   || (name_vec[name_length - 3] != '['
                       && name_vec[name_length - 2] != '0'
                       && name_vec[name_length - 1] != ']'))) {
    name_vec[name_length - 3] = '[';
    name_vec[name_length - 2] = '0';
    name_vec[name_length - 1] = ']';
    name_vec[name_length] = 0;
  }
  WebGLActiveInfo* active_info = CreateActiveInfo(size, type, &name_vec[0]);
  return active_info->ToV8Object();
}

// WebGLShader[ ] getAttachedShaders(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getAttachedShaders(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLuint shaders[2] = {0};
  GLsizei count = 0;
  glGetAttachedShaders(program_id, 2, &count, shaders);

  v8::Local<v8::Array> array = v8::Array::New(count);
  for (int i = 0; i < count; i++) {
    WebGLShader* shader = IdToShader(shaders[i]);
    if (shader)
      array->Set(ToV8(i), shader->ToV8Object());
  }
  return array;
}

// GLint getAttribLocation(WebGLProgram program, DOMString name);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getAttribLocation(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  std::string name = FromV8<std::string>(args[1], &ok); if (!ok) return U();
  GLint location = glGetAttribLocation(program_id, name.c_str());
  return ToV8(location);
}

// any getParameter(GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getParameter(const v8::Arguments& args) {
  bool ok = true;
  GLenum pname = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  switch (pname) {
    case GL_ACTIVE_TEXTURE:
    case GL_BLEND_DST_ALPHA:
    case GL_BLEND_DST_RGB:
    case GL_BLEND_EQUATION_ALPHA:
    case GL_BLEND_EQUATION_RGB:
    case GL_BLEND_SRC_ALPHA:
    case GL_BLEND_SRC_RGB:
    case GL_CULL_FACE_MODE:
    case GL_DEPTH_FUNC:
    case GL_FRONT_FACE:
    case GL_GENERATE_MIPMAP_HINT:
    case GL_STENCIL_BACK_FAIL:
    case GL_STENCIL_BACK_FUNC:
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
    case GL_STENCIL_BACK_VALUE_MASK:
    case GL_STENCIL_BACK_WRITEMASK:
    case GL_STENCIL_FAIL:
    case GL_STENCIL_FUNC:
    case GL_STENCIL_PASS_DEPTH_FAIL:
    case GL_STENCIL_PASS_DEPTH_PASS:
    case GL_STENCIL_VALUE_MASK:
    case GL_STENCIL_WRITEMASK: {
      GLint value = 0;
      GetIntegerv(pname, &value);
      return ToV8(static_cast<uint32_t>(value));
    }

    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_DEPTH_RANGE: {
      GLfloat value[2] = {0};
      glGetFloatv(pname, value);
      return Float32Array::Create(value, 2);
    }

    case GL_BLEND_COLOR:
    case GL_COLOR_CLEAR_VALUE: {
      GLfloat value[4] = {0};
      glGetFloatv(pname, value);
      return Float32Array::Create(value, 4);
    }

    case GL_MAX_VIEWPORT_DIMS: {
      GLint value[2] = {0};
      GetIntegerv(pname, value);
      return Int32Array::Create(value, 2);
    }

    case GL_SCISSOR_BOX:
    case GL_VIEWPORT: {
      GLint value[4] = {0};
      GetIntegerv(pname, value);
      return Int32Array::Create(value, 4);
    }

    case GL_ALPHA_BITS:
    case GL_BLUE_BITS:
    case GL_DEPTH_BITS:
    case GL_GREEN_BITS:
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
    case GL_MAX_RENDERBUFFER_SIZE:
    case GL_MAX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_TEXTURE_SIZE:
    case GL_MAX_VARYING_VECTORS:
    case GL_MAX_VERTEX_ATTRIBS:
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
#if 0
    case GL_NUM_SHADER_BINARY_FORMATS:
      //XXX not sure about this
#endif
    case GL_PACK_ALIGNMENT:
    case GL_RED_BITS:
    case GL_SAMPLE_BUFFERS:
    case GL_SAMPLES:
    case GL_STENCIL_BACK_REF:
    case GL_STENCIL_BITS:
    case GL_STENCIL_CLEAR_VALUE:
    case GL_STENCIL_REF:
    case GL_SUBPIXEL_BITS:
    case GL_UNPACK_ALIGNMENT: {
      GLint value = 0;
      GetIntegerv(pname, &value);
      return ToV8(value);
    }

    case GL_COLOR_WRITEMASK: {
      GLboolean value[4] = {0};
      glGetBooleanv(pname, value);
      bool bool_value[4];
      for (int i = 0; i < 4; i++)
        bool_value[i] = static_cast<bool>(value[i]);
      return ArrayToV8<bool>(bool_value, 4);
    }

    case GL_BLEND:
    case GL_CULL_FACE:
    case GL_DEPTH_TEST:
    case GL_DEPTH_WRITEMASK:
    case GL_DITHER:
    case GL_POLYGON_OFFSET_FILL:
    case GL_SAMPLE_COVERAGE_INVERT:
    case GL_SCISSOR_TEST:
    case GL_STENCIL_TEST: {
      GLboolean value = 0;
      glGetBooleanv(pname, &value);
      return ToV8(static_cast<bool>(value));
    }

    case GL_DEPTH_CLEAR_VALUE:
    case GL_LINE_WIDTH:
    case GL_POLYGON_OFFSET_FACTOR:
    case GL_POLYGON_OFFSET_UNITS:
    case GL_SAMPLE_COVERAGE_VALUE: {
      GLfloat value = 0;
      glGetFloatv(pname, &value);
      return ToV8<double>(value);
    }

    case GL_ARRAY_BUFFER_BINDING:
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: {
      GLint buffer_id = 0;
      GetIntegerv(pname, &buffer_id);
      WebGLBuffer* buffer = IdToBuffer(buffer_id);
      return ToV8OrNull(buffer);
    }

    case GL_COMPRESSED_TEXTURE_FORMATS:
      // XXX needed for WEBGL_compressed_texture_s3tc extension
      return Uint32Array::Create(0, NULL);

    case GL_CURRENT_PROGRAM: {
      GLint program_id = 0;
      GetIntegerv(pname, &program_id);
      WebGLProgram* program = IdToProgram(program_id);
      return ToV8OrNull(program);
    }

    case GL_FRAMEBUFFER_BINDING:
    case GL_RENDERBUFFER_BINDING: {
      GLint framebuffer_id = 0;
      GetIntegerv(pname, &framebuffer_id);
      WebGLFramebuffer* framebuffer = IdToFramebuffer(framebuffer_id);
      return ToV8OrNull(framebuffer);
    }

    case GL_RENDERER:
      return ToV8("v8-webgl");

    case GL_SHADING_LANGUAGE_VERSION: {
      std::string version = std::string("WebGL GLSL ES 1.0 (") + std::string(reinterpret_cast<const char*>(glGetString(pname))) + std::string(")");
      return ToV8(version.c_str());
    }

    case GL_TEXTURE_BINDING_2D:
    case GL_TEXTURE_BINDING_CUBE_MAP: {
      GLint texture_id = 0;
      GetIntegerv(pname, &texture_id);
      WebGLTexture* texture = IdToTexture(texture_id);
      return ToV8OrNull(texture);
    }

#if 0
    case GL_UNPACK_FLIP_Y_WEBGL:
    case GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
    case GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      //XXX these are WebGL specific - need to track and store them in our context
#endif

    case GL_VENDOR:
      return ToV8("rectalogic");

    case GL_VERSION: {
      std::string version = std::string("WebGL 1.0 (") + std::string(reinterpret_cast<const char*>(glGetString(pname))) + std::string(")");
      return ToV8(version.c_str());
    }

#if 0
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      //XXX used with OES_standard_derivatives extension

    case GL_UNMASKED_RENDERER_WEBGL:
    case GL_UNMASKED_VENDOR_WEBGL:
      //XXX used with WEBGL_debug_renderer_info extension

    case GL_VERTEX_ARRAY_BINDING_OES:
      //XXX used with OES_vertex_array_object extension
#endif

    default: {
      set_gl_error(GL_INVALID_ENUM);
      Log(Logger::kWarn, "getParameter: Unrecognized parameter name: %d", pname);
      return U();
    }
  }
}

// any getBufferParameter(GLenum target, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getBufferParameter(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return v8::Null();
  }
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (pname != GL_BUFFER_SIZE && pname != GL_BUFFER_USAGE) {
    set_gl_error(GL_INVALID_ENUM);
    return v8::Null();
  }
  GLint value = 0;
  glGetBufferParameteriv(target, pname, &value);
  if (pname == GL_BUFFER_SIZE)
    return ToV8(value);
  return ToV8(static_cast<uint32_t>(value));
}

// GLenum getError();
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getError(const v8::Arguments& args) {
  GLenum error = gl_error();
  return ToV8(error);
}

// any getFramebufferAttachmentParameter(GLenum target, GLenum attachment, 
//                                       GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getFramebufferAttachmentParameter(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum attachment = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateFramebufferFuncParameters("getFramebufferAttachmentParameter", target, attachment))
    return v8::Null();
  GLenum pname = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  GLint value = 0;
  //XXX glGetFramebufferAttachmentParameterivEXT etc.
  glGetFramebufferAttachmentParameteriv(target, attachment, pname, &value);

  switch (pname) {
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: {
      GLint type = 0;
      glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
      switch (type) {
        case GL_RENDERBUFFER: {
          WebGLRenderbuffer* renderbuffer = IdToRenderbuffer(value);
          return ToV8OrNull(renderbuffer);
        }
        case GL_TEXTURE: {
          WebGLTexture* texture = IdToTexture(value);
          return ToV8OrNull(texture);
        }
        default:
          set_gl_error(GL_INVALID_ENUM);
          return v8::Null();
      }
    }
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      return ToV8<uint32_t>(value);
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      return ToV8(value);
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
}

// any getProgramParameter(WebGLProgram program, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getProgramParameter(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLint value = 0;
  glGetProgramiv(program_id, pname, &value);

  switch (pname) {
    case GL_DELETE_STATUS:
    case GL_VALIDATE_STATUS:
    case GL_LINK_STATUS:
      return ToV8(static_cast<bool>(value));
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_UNIFORMS:
      return ToV8(value);
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
}

// DOMString getProgramInfoLog(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getProgramInfoLog(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  GLint length = 0;
  glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
  if (!length)
    return v8::String::Empty();
  GLsizei size = 0;
  std::vector<char> buffer(length);
  glGetProgramInfoLog(program_id, length, &size, &buffer[0]);
  std::string log(&buffer[0], size);
  return ToV8(log);
}

// any getRenderbufferParameter(GLenum target, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getRenderbufferParameter(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (target != GL_RENDERBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return v8::Null();
  }
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLint value = 0;
  //XXX glGetRenderbufferParameterivEXT
  glGetRenderbufferParameteriv(target, pname, &value);
  switch (pname) {
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return ToV8(static_cast<uint32_t>(value));
    case GL_RENDERBUFFER_WIDTH:
    case GL_RENDERBUFFER_HEIGHT:
    case GL_RENDERBUFFER_RED_SIZE:
    case GL_RENDERBUFFER_GREEN_SIZE:
    case GL_RENDERBUFFER_BLUE_SIZE:
    case GL_RENDERBUFFER_ALPHA_SIZE:
    case GL_RENDERBUFFER_DEPTH_SIZE:
    case GL_RENDERBUFFER_STENCIL_SIZE:
      return ToV8(value);
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
}

// any getShaderParameter(WebGLShader shader, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getShaderParameter(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  GLuint shader_id = shader->webgl_id();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  switch (pname) {
    case GL_COMPILE_STATUS:
      return ToV8(shader->is_valid());
    case GL_DELETE_STATUS: {
      GLint value = 0;
      glGetShaderiv(shader_id, pname, &value);
      return ToV8<bool>(static_cast<bool>(value));
    }
    case GL_SHADER_TYPE: {
      GLint value = 0;
      glGetShaderiv(shader_id, pname, &value);
      return ToV8<uint32_t>(value);
    }
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
}

// DOMString getShaderInfoLog(WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getShaderInfoLog(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  if (!shader->is_valid())
    return ToV8(shader->log());

  GLuint shader_id = shader->webgl_id();
  GLint length = 0;
  glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
  if (!length)
    return v8::String::Empty();
  GLsizei size = 0;
  std::vector<char> vector(length);
  glGetShaderInfoLog(shader_id, length, &size, &vector[0]);
  return ToV8<const char*>(static_cast<const char*>(&vector[0]));
}

// DOMString getShaderSource(WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getShaderSource(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  return ToV8(shader->source());
}

// any getTexParameter(GLenum target, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getTexParameter(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateTextureBinding("getTextParameter", target, false))
    return v8::Null();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  switch (pname) {
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
  GLint value = 0;
  glGetTexParameteriv(target, pname, &value);
  return ToV8(static_cast<uint32_t>(value));
}

static bool UniformTypeToBaseLength(GLenum uniform_type, GLenum* uniform_base_type, uint32_t* length) {
  switch (uniform_type) {
    case GL_BOOL:
      *uniform_base_type = GL_BOOL;
      *length = 1;
      break;
    case GL_BOOL_VEC2:
      *uniform_base_type = GL_BOOL;
      *length = 2;
      break;
    case GL_BOOL_VEC3:
      *uniform_base_type = GL_BOOL;
      *length = 3;
      break;
    case GL_BOOL_VEC4:
      *uniform_base_type = GL_BOOL;
      *length = 4;
      break;
    case GL_INT:
      *uniform_base_type = GL_INT;
      *length = 1;
      break;
    case GL_INT_VEC2:
      *uniform_base_type = GL_INT;
      *length = 2;
      break;
    case GL_INT_VEC3:
      *uniform_base_type = GL_INT;
      *length = 3;
      break;
    case GL_INT_VEC4:
      *uniform_base_type = GL_INT;
      *length = 4;
      break;
    case GL_FLOAT:
      *uniform_base_type = GL_FLOAT;
      *length = 1;
      break;
    case GL_FLOAT_VEC2:
      *uniform_base_type = GL_FLOAT;
      *length = 2;
      break;
    case GL_FLOAT_VEC3:
      *uniform_base_type = GL_FLOAT;
      *length = 3;
      break;
    case GL_FLOAT_VEC4:
      *uniform_base_type = GL_FLOAT;
      *length = 4;
      break;
    case GL_FLOAT_MAT2:
      *uniform_base_type = GL_FLOAT;
      *length = 4;
      break;
    case GL_FLOAT_MAT3:
      *uniform_base_type = GL_FLOAT;
      *length = 9;
      break;
    case GL_FLOAT_MAT4:
      *uniform_base_type = GL_FLOAT;
      *length = 16;
      break;
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
      *uniform_base_type = GL_INT;
      *length = 1;
      break;
    default:
      return false;
  }
  return true;
}

// any getUniform(WebGLProgram program, WebGLUniformLocation location);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getUniform(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  WebGLUniformLocation* location = NativeFromV8<WebGLUniformLocation>(args[1], &ok); if (!ok) return U();
  if (!RequireObject(location)) return U();
  if (!ValidateLocationProgram(location, program_id)) return U();

  GLint location_id = location->webgl_id();
  GLint active_uniforms = 0;
  glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &active_uniforms);
  GLint max_name_length = 0;
  glGetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  std::vector<char> name_vec(max_name_length);
  std::string array_ending("[0]");
  // Search for our location_id
  for (GLint i = 0; i < active_uniforms; i++) {
    GLsizei name_length = 0;
    GLint uniform_size = 0;
    GLenum uniform_type = 0;
    glGetActiveUniform(program_id, i, max_name_length, &name_length,
                       &uniform_size, &uniform_type, &name_vec[0]);
    std::string uniform_name(&name_vec[0], name_length);

    // Strip "[0]" from name ending, if it's an array
    if (uniform_size > 1 && uniform_name.length() > array_ending.length()
        && uniform_name.compare(uniform_name.length() - array_ending.length(),
                                array_ending.length(), array_ending) == 0) {
      uniform_name.resize(uniform_name.length() - 3);
    }

    // For arrays, iterate through each element appending "[index]" to the name
    // and checking location
    for (GLint index = 0; index < uniform_size; index++) {
      std::string name(uniform_name);
      if (uniform_size > 1 && index >= 1) {
        std::stringstream ss(name);
        ss << "[" << index << "]";
        name = ss.str();
      }
      // Look the name up again
      GLint uniform_location_id = glGetUniformLocation(program_id, name.c_str());
      if (uniform_location_id == location_id) {
        GLenum uniform_base_type = 0;
        uint32_t length = 0;
        if (!UniformTypeToBaseLength(uniform_type, &uniform_base_type, &length)) {
          set_gl_error(GL_INVALID_VALUE);
          return v8::Null();
        }

        switch (uniform_base_type) {
          case GL_FLOAT: {
            GLfloat value[16] = {0};
            glGetUniformfv(program_id, location_id, value);
            if (length == 1)
              return ToV8<double>(value[0]);
            return Float32Array::Create(value, length);
          }
          case GL_INT: {
            GLint value[4] = {0};
            glGetUniformiv(program_id, location_id, value);
            if (length == 1)
              return ToV8(value[0]);
            return Int32Array::Create(value, length);
          }
          case GL_BOOL: {
            GLint value[4] = {0};
            glGetUniformiv(program_id, location_id, value);
            if (length > 1) {
              bool bool_value[4] = {0};
              for (uint32_t j = 0; j < length; j++)
                bool_value[j] = static_cast<bool>(value[j]);
              return ArrayToV8<bool>(bool_value, length);
            }
            return ToV8(static_cast<bool>(value[0]));
          }
        }
      }
    }
  }

  set_gl_error(GL_INVALID_VALUE);
  return v8::Null();
}

// WebGLUniformLocation getUniformLocation(WebGLProgram program, DOMString name);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getUniformLocation(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  std::string name = FromV8<std::string>(args[1], &ok); if (!ok) return U();
  GLint location_id = glGetUniformLocation(program_id, name.c_str());
  WebGLUniformLocation* location = CreateUniformLocation(program_id, location_id);
  return location->ToV8Object();
}

// any getVertexAttrib(GLuint index, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getVertexAttrib(const v8::Arguments& args) {
  bool ok = true;
  GLuint index = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
      GLint value = 0;
      glGetVertexAttribiv(index, pname, &value);
      WebGLBuffer* buffer = IdToBuffer(value);
      return ToV8OrNull(buffer);
    }
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: {
      GLint value = 0;
      glGetVertexAttribiv(index, pname, &value);
      return ToV8(static_cast<bool>(value));
    }
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: {
      GLint value = 0;
      glGetVertexAttribiv(index, pname, &value);
      return ToV8(value);
    }
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: {
      GLint value = 0;
      glGetVertexAttribiv(index, pname, &value);
      return ToV8(static_cast<uint32_t>(value));
    }
    case GL_CURRENT_VERTEX_ATTRIB: {
      GLfloat value[4] = {0};
      glGetVertexAttribfv(index, pname, value);
      return Float32Array::Create(value, 4);
    }
    default:
      set_gl_error(GL_INVALID_ENUM);
      return v8::Null();
  }
}

// GLsizeiptr getVertexAttribOffset(GLuint index, GLenum pname);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_getVertexAttribOffset(const v8::Arguments& args) {
  bool ok = true;
  GLuint index = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLvoid* pointer = 0;
  glGetVertexAttribPointerv(index, pname, &pointer);
  return ToV8<int32_t>(static_cast<GLsizeiptr>(reinterpret_cast<intptr_t>(pointer)));
}

// void hint(GLenum target, GLenum mode);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_hint(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  //XXX nedd to handle GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES when we support extensions
  if (target != GL_GENERATE_MIPMAP_HINT) {
    set_gl_error(GL_INVALID_ENUM);
    return U();
  }
  GLenum mode = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  glHint(target, mode);
  return U();
}

// GLboolean isBuffer(WebGLBuffer buffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isBuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLBuffer* buffer = NativeFromV8<WebGLBuffer>(args[0], &ok); if (!ok) return U();
  if (!buffer)
    return ToV8(false);
  GLuint buffer_id = buffer ? buffer->webgl_id() : 0;
  return ToV8<bool>(glIsBuffer(buffer_id));
}

// GLboolean isEnabled(GLenum cap);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isEnabled(const v8::Arguments& args) {
  bool ok = true;
  GLenum cap = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateCapability("isEnabled", cap))
    return ToV8(false);
  glIsEnabled(cap);
  return U();
}

// GLboolean isFramebuffer(WebGLFramebuffer framebuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isFramebuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLFramebuffer* framebuffer = NativeFromV8<WebGLFramebuffer>(args[0], &ok); if (!ok) return U();
  if (!framebuffer)
    return ToV8(false);
  GLuint framebuffer_id = framebuffer ? framebuffer->webgl_id() : 0;
  return ToV8<bool>(glIsFramebuffer(framebuffer_id));
}

// GLboolean isProgram(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isProgram(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!program)
    return ToV8(false);
  GLuint program_id = program ? program->webgl_id() : 0;
  return ToV8<bool>(glIsProgram(program_id));
}

// GLboolean isRenderbuffer(WebGLRenderbuffer renderbuffer);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isRenderbuffer(const v8::Arguments& args) {
  bool ok = true;
  WebGLRenderbuffer* renderbuffer = NativeFromV8<WebGLRenderbuffer>(args[0], &ok); if (!ok) return U();
  if (!renderbuffer)
    return ToV8(false);
  GLuint renderbuffer_id = renderbuffer ? renderbuffer->webgl_id() : 0;
  return ToV8<bool>(glIsRenderbuffer(renderbuffer_id));
}

// GLboolean isShader(WebGLShader shader);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isShader(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!shader)
    return ToV8(false);
  GLuint shader_id = shader ? shader->webgl_id() : 0;
  return ToV8<bool>(glIsShader(shader_id));
}

// GLboolean isTexture(WebGLTexture texture);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_isTexture(const v8::Arguments& args) {
  bool ok = true;
  WebGLTexture* texture = NativeFromV8<WebGLTexture>(args[0], &ok); if (!ok) return U();
  if (!texture)
    return ToV8(false);
  GLuint texture_id = texture ? texture->webgl_id() : 0;
  return ToV8<bool>(glIsTexture(texture_id));
}

// void lineWidth(GLfloat width);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_lineWidth(const v8::Arguments& args) {
  bool ok = true;
  GLfloat width = FromV8<float>(args[0], &ok); if (!ok) return U();
  glLineWidth(width);
  return U();
}

// void linkProgram(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_linkProgram(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  glLinkProgram(program_id);
  return U();
}

//XXX this needs to deal with custom WebGL attrs like UNPACK_FLIP_Y_WEBGL
// void pixelStorei(GLenum pname, GLint param);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_pixelStorei(const v8::Arguments& args) {
  bool ok = true;
  GLenum pname = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLint param = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  switch (pname) {
    case GL_UNPACK_FLIP_Y_WEBGL:
    case GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
    case GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
      //XXX how should we handle these?
      return U();
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      switch (param) {
        case 1:
        case 2:
        case 4:
        case 8:
          break;
        default:
          set_gl_error(GL_INVALID_VALUE);
          return U();
      }
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  glPixelStorei(pname, param);
  return U();
}

// void polygonOffset(GLfloat factor, GLfloat units);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_polygonOffset(const v8::Arguments& args) {
  bool ok = true;
  GLfloat factor = FromV8<float>(args[0], &ok); if (!ok) return U();
  GLfloat units = FromV8<float>(args[1], &ok); if (!ok) return U();
  glPolygonOffset(factor, units);
  return U();
}

// void readPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
//                 GLenum format, GLenum type, ArrayBufferView pixels);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_readPixels(const v8::Arguments& args) {
  bool ok = true;
  GLint x = FromV8<int32_t>(args[0], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  GLenum format = FromV8<uint32_t>(args[4], &ok); if (!ok) return U();
  GLenum type = FromV8<uint32_t>(args[5], &ok); if (!ok) return U();
  if (format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
    set_gl_error(GL_INVALID_OPERATION);
    return U();
  }
  Uint8Array* array = NativeFromV8<Uint8Array>(args[6], &ok); if (!ok) return U();
  if (!RequireObject(array)) return U();

  GLint alignment = 4;
  GetIntegerv(GL_PACK_ALIGNMENT, &alignment);
  // GL_RGBA is 4 bytes per pixel
  uint32_t row_bytes = width * 4;
  if (row_bytes % alignment != 0)
    row_bytes = row_bytes + (alignment - (row_bytes % alignment));

  uint32_t length_required = row_bytes * height;
  uint32_t length = array->GetArrayLength();
  if (length < length_required) {
    set_gl_error(GL_INVALID_OPERATION);
    return U();
  }

  void* data = array->GetArrayData();
  glReadPixels(x, y, width, height, format, type, data);

  return U();
}

// void renderbufferStorage(GLenum target, GLenum internalformat, 
//                          GLsizei width, GLsizei height);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_renderbufferStorage(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum internalformat = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  if (target != GL_RENDERBUFFER) {
    set_gl_error(GL_INVALID_ENUM);
    return U();
  }
  //XXX some of these formats may not be supported on desktop GL - so we convert them (see GraphicsContext3DOpenGL.cpp:renderbufferStorage
  //XXX this means getRenderbufferParameter will return the wrong value - we should really stash the original value in the bound WebGLRenderbuffer
  //XXX http://www.khronos.org/webgl/public-mailing-list/archives/1010/msg00123.html
  switch (internalformat) {
    case GL_DEPTH_STENCIL:
      internalformat = GL_DEPTH24_STENCIL8;
      break;
    case GL_DEPTH_COMPONENT16:
      internalformat = GL_DEPTH_COMPONENT;
      break;
    case GL_RGBA4:
    case GL_RGB5_A1:
      internalformat = GL_RGBA;
      break;
    case GL_RGB565:
      internalformat = GL_RGB;
      break;
    case GL_STENCIL_INDEX8:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }

  glRenderbufferStorage(target, internalformat, width, height);
  return U();
}

// void sampleCoverage(GLclampf value, GLboolean invert);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_sampleCoverage(const v8::Arguments& args) {
  bool ok = true;
  GLclampf value = FromV8<float>(args[0], &ok); if (!ok) return U();
  GLboolean invert = FromV8<bool>(args[1], &ok); if (!ok) return U();
  glSampleCoverage(value, invert);
  return U();
}

// void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_scissor(const v8::Arguments& args) {
  bool ok = true;
  GLint x = FromV8<int32_t>(args[0], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  glScissor(x, y, width, height);
  return U();
}

// void shaderSource(WebGLShader shader, DOMString source);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_shaderSource(const v8::Arguments& args) {
  bool ok = true;
  WebGLShader* shader = NativeFromV8<WebGLShader>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(shader)) return U();
  if (!ValidateObject(shader)) return U();
  std::string source = FromV8<std::string>(args[1], &ok); if (!ok) return U();
  shader->set_source(source);
  return U();
}

// void stencilFunc(GLenum func, GLint ref, GLuint mask);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilFunc(const v8::Arguments& args) {
  bool ok = true;
  GLenum func = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateStencilFunc("stencilFunc", func))
    return U();
  GLint ref = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLuint mask = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  glStencilFunc(func, ref, mask);
  return U();
}

// void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilFuncSeparate(const v8::Arguments& args) {
  bool ok = true;
  GLenum face = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  switch (face) {
    case GL_FRONT_AND_BACK:
    case GL_FRONT:
    case GL_BACK:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  GLenum func = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  if (!ValidateStencilFunc("stencilFuncSeparate", func))
    return U();
  GLint ref = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLuint mask = FromV8<uint32_t>(args[3], &ok); if (!ok) return U();
  glStencilFuncSeparate(face, func, ref, mask);
  return U();
}

// void stencilMask(GLuint mask);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilMask(const v8::Arguments& args) {
  bool ok = true;
  GLuint mask = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  glStencilMask(mask);
  return U();
}

// void stencilMaskSeparate(GLenum face, GLuint mask);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilMaskSeparate(const v8::Arguments& args) {
  bool ok = true;
  GLenum face = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  switch (face) {
    case GL_FRONT_AND_BACK:
    case GL_FRONT:
    case GL_BACK:
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  GLuint mask = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  glStencilMaskSeparate(face, mask);
  return U();
}

// void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilOp(const v8::Arguments& args) {
  bool ok = true;
  GLenum fail = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum zfail = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLenum zpass = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  glStencilOp(fail, zfail, zpass);
  return U();
}

// void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_stencilOpSeparate(const v8::Arguments& args) {
  bool ok = true;
  GLenum face = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLenum fail = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLenum zfail = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  GLenum zpass = FromV8<uint32_t>(args[3], &ok); if (!ok) return U();
  glStencilOpSeparate(face, fail, zfail, zpass);
  return U();
}

// void texImage2D(GLenum target, GLint level, GLenum internalformat, 
//                 GLsizei width, GLsizei height, GLint border, GLenum format, 
//                 GLenum type, ArrayBufferView pixels);
// void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                 GLenum format, GLenum type, ImageData pixels);
// void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                 GLenum format, GLenum type, HTMLImageElement image) raises (DOMException);
// void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                 GLenum format, GLenum type, HTMLCanvasElement canvas) raises (DOMException);
// void texImage2D(GLenum target, GLint level, GLenum internalformat,
//                 GLenum format, GLenum type, HTMLVideoElement video) raises (DOMException);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_texImage2D(const v8::Arguments& args) {
  //XXX we validate for 6 args, check for more for the other overloads
  return U(); /*XXX finish*/
}

// void texParameterf(GLenum target, GLenum pname, GLfloat param);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_texParameterf(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateTextureBinding("texParameterf", target, false))
    return U();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLfloat param = FromV8<float>(args[2], &ok); if (!ok) return U();
  if (!ValidateTexParameter("texParameterf", pname, static_cast<GLint>(param)))
    return U();
  glTexParameterf(target, pname, param);
  return U();
}

// void texParameteri(GLenum target, GLenum pname, GLint param);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_texParameteri(const v8::Arguments& args) {
  bool ok = true;
  GLenum target = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  if (!ValidateTextureBinding("texParameteri", target, false))
    return U();
  GLenum pname = FromV8<uint32_t>(args[1], &ok); if (!ok) return U();
  GLint param = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  if (!ValidateTexParameter("texParameteri", pname, param))
    return U();
  glTexParameteri(target, pname, param);
  return U();
}

// void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                    GLsizei width, GLsizei height, 
//                    GLenum format, GLenum type, ArrayBufferView pixels);
// void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                    GLenum format, GLenum type, ImageData pixels);
// void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                    GLenum format, GLenum type, HTMLImageElement image) raises (DOMException);
// void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                    GLenum format, GLenum type, HTMLCanvasElement canvas) raises (DOMException);
// void texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
//                    GLenum format, GLenum type, HTMLVideoElement video) raises (DOMException);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_texSubImage2D(const v8::Arguments& args) {
  //XXX we validate for 7 args, check for overloads with more
  return U(); /*XXX finish*/
}

// void uniform1f(WebGLUniformLocation location, GLfloat x);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform1f(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  glUniform1f(location_id, x);
  return U();
}

// void uniform1fv(WebGLUniformLocation location, FloatArray v);
// void uniform1fv(WebGLUniformLocation location, sequence<float> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform1fv(const v8::Arguments& args) {
  UniformHelper<GLfloat> h(glUniform1fv, 1);
  return h.Process(this, args);
}

// void uniform1i(WebGLUniformLocation location, GLint x);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform1i(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLint x = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  glUniform1i(location_id, x);
  return U();
}

// void uniform1iv(WebGLUniformLocation location, Int32Array v);
// void uniform1iv(WebGLUniformLocation location, sequence<long> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform1iv(const v8::Arguments& args) {
  UniformHelper<GLint> h(glUniform1iv, 1);
  return h.Process(this, args);
}

// void uniform2f(WebGLUniformLocation location, GLfloat x, GLfloat y);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform2f(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  glUniform2f(location_id, x, y);
  return U();
}

// void uniform2fv(WebGLUniformLocation location, FloatArray v);
// void uniform2fv(WebGLUniformLocation location, sequence<float> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform2fv(const v8::Arguments& args) {
  UniformHelper<GLfloat> h(glUniform2fv, 2);
  return h.Process(this, args);
}

// void uniform2i(WebGLUniformLocation location, GLint x, GLint y);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform2i(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLint x = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  glUniform2i(location_id, x, y);
  return U();
}

// void uniform2iv(WebGLUniformLocation location, Int32Array v);
// void uniform2iv(WebGLUniformLocation location, sequence<long> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform2iv(const v8::Arguments& args) {
  UniformHelper<GLint> h(glUniform2iv, 2);
  return h.Process(this, args);
}

// void uniform3f(WebGLUniformLocation location, GLfloat x, GLfloat y, GLfloat z);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform3f(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLfloat z = FromV8<float>(args[3], &ok); if (!ok) return U();
  glUniform3f(location_id, x, y, z);
  return U();
}

// void uniform3fv(WebGLUniformLocation location, FloatArray v);
// void uniform3fv(WebGLUniformLocation location, sequence<float> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform3fv(const v8::Arguments& args) {
  UniformHelper<GLfloat> h(glUniform3fv, 3);
  return h.Process(this, args);
}

// void uniform3i(WebGLUniformLocation location, GLint x, GLint y, GLint z);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform3i(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLint x = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLint z = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  glUniform3i(location_id, x, y, z);
  return U();
}

// void uniform3iv(WebGLUniformLocation location, Int32Array v);
// void uniform3iv(WebGLUniformLocation location, sequence<long> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform3iv(const v8::Arguments& args) {
  UniformHelper<GLint> h(glUniform3iv, 3);
  return h.Process(this, args);
}

// void uniform4f(WebGLUniformLocation location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform4f(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLfloat z = FromV8<float>(args[3], &ok); if (!ok) return U();
  GLfloat w = FromV8<float>(args[4], &ok); if (!ok) return U();
  glUniform4f(location_id, x, y, z, w);
  return U();
}

// void uniform4fv(WebGLUniformLocation location, FloatArray v);
// void uniform4fv(WebGLUniformLocation location, sequence<float> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform4fv(const v8::Arguments& args) {
  UniformHelper<GLfloat> h(glUniform4fv, 4);
  return h.Process(this, args);
}

// void uniform4i(WebGLUniformLocation location, GLint x, GLint y, GLint z, GLint w);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform4i(const v8::Arguments& args) {
  bool ok = true;
  WebGLUniformLocation* location = UniformLocationFromV8(args[0]);
  if (!location) return U();
  GLuint location_id = location->webgl_id();

  GLint x = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLint z = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  GLint w = FromV8<int32_t>(args[4], &ok); if (!ok) return U();
  glUniform4i(location_id, x, y, z, w);
  return U();
}

// void uniform4iv(WebGLUniformLocation location, Int32Array v);
// void uniform4iv(WebGLUniformLocation location, sequence<long> v);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniform4iv(const v8::Arguments& args) {
  UniformHelper<GLint> h(glUniform4iv, 4);
  return h.Process(this, args);
}

// void uniformMatrix2fv(WebGLUniformLocation location, GLboolean transpose, 
//                       FloatArray value);
// void uniformMatrix2fv(WebGLUniformLocation location, GLboolean transpose, 
//                       sequence<float> value);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniformMatrix2fv(const v8::Arguments& args) {
  UniformMatrixHelper<GLfloat> h(glUniformMatrix2fv, 2*2);
  return h.Process(this, args);
}

// void uniformMatrix3fv(WebGLUniformLocation location, GLboolean transpose, 
//                       FloatArray value);
// void uniformMatrix3fv(WebGLUniformLocation location, GLboolean transpose, 
//                       sequence<float> value);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniformMatrix3fv(const v8::Arguments& args) {
  UniformMatrixHelper<GLfloat> h(glUniformMatrix3fv, 3*3);
  return h.Process(this, args);
}

// void uniformMatrix4fv(WebGLUniformLocation location, GLboolean transpose, 
//                       FloatArray value);
// void uniformMatrix4fv(WebGLUniformLocation location, GLboolean transpose, 
//                       sequence<float> value);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_uniformMatrix4fv(const v8::Arguments& args) {
  UniformMatrixHelper<GLfloat> h(glUniformMatrix4fv, 4*4);
  return h.Process(this, args);
}

// void useProgram(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_useProgram(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  glUseProgram(program_id);
  return U();
}

// void validateProgram(WebGLProgram program);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_validateProgram(const v8::Arguments& args) {
  bool ok = true;
  WebGLProgram* program = NativeFromV8<WebGLProgram>(args[0], &ok); if (!ok) return U();
  if (!RequireObject(program)) return U();
  if (!ValidateObject(program)) return U();
  GLuint program_id = program->webgl_id();
  glValidateProgram(program_id);
  return U();
}

// void vertexAttrib1f(GLuint indx, GLfloat x);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib1f(const v8::Arguments& args) {
  bool ok = true;
  GLint index = FromV8<int32_t>(args[0], &ok); if (!ok) return U();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  glVertexAttrib1f(index, x);
  return U();
}

// void vertexAttrib1fv(GLuint indx, FloatArray values);
// void vertexAttrib1fv(GLuint indx, sequence<float> values);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib1fv(const v8::Arguments& args) {
  VertexAttribHelper<GLfloat> h(glVertexAttrib1fv, 1);
  return h.Process(this, args);
}

// void vertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib2f(const v8::Arguments& args) {
  bool ok = true;
  GLint index = FromV8<int32_t>(args[0], &ok); if (!ok) return U();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  glVertexAttrib2f(index, x, y);
  return U();
}

// void vertexAttrib2fv(GLuint indx, FloatArray values);
// void vertexAttrib2fv(GLuint indx, sequence<float> values);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib2fv(const v8::Arguments& args) {
  VertexAttribHelper<GLfloat> h(glVertexAttrib2fv, 2);
  return h.Process(this, args);
}

// void vertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib3f(const v8::Arguments& args) {
  bool ok = true;
  GLint index = FromV8<int32_t>(args[0], &ok); if (!ok) return U();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLfloat z = FromV8<float>(args[3], &ok); if (!ok) return U();
  glVertexAttrib3f(index, x, y, z);
  return U();
}

// void vertexAttrib3fv(GLuint indx, FloatArray values);
// void vertexAttrib3fv(GLuint indx, sequence<float> values);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib3fv(const v8::Arguments& args) {
  VertexAttribHelper<GLfloat> h(glVertexAttrib3fv, 3);
  return h.Process(this, args);
}

// void vertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib4f(const v8::Arguments& args) {
  bool ok = true;
  GLint index = FromV8<int32_t>(args[0], &ok); if (!ok) return U();

  GLfloat x = FromV8<float>(args[1], &ok); if (!ok) return U();
  GLfloat y = FromV8<float>(args[2], &ok); if (!ok) return U();
  GLfloat z = FromV8<float>(args[3], &ok); if (!ok) return U();
  GLfloat w = FromV8<float>(args[4], &ok); if (!ok) return U();
  glVertexAttrib4f(index, x, y, z, w);
  return U();
}

// void vertexAttrib4fv(GLuint indx, FloatArray values);
// void vertexAttrib4fv(GLuint indx, sequence<float> values);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttrib4fv(const v8::Arguments& args) {
  VertexAttribHelper<GLfloat> h(glVertexAttrib4fv, 4);
  return h.Process(this, args);
}

// void vertexAttribPointer(GLuint indx, GLint size, GLenum type, 
//                          GLboolean normalized, GLsizei stride, GLintptr offset);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_vertexAttribPointer(const v8::Arguments& args) {
  bool ok = true;
  GLuint indx = FromV8<uint32_t>(args[0], &ok); if (!ok) return U();
  GLint size = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLenum type = FromV8<uint32_t>(args[2], &ok); if (!ok) return U();
  uint32_t type_size = 0;
  switch (type) {
    case GL_BYTE:
      type_size = sizeof(GLbyte);
      break;
    case GL_UNSIGNED_BYTE:
      type_size = sizeof(GLubyte);
      break;
    case GL_SHORT:
      type_size = sizeof(GLshort);
      break;
    case GL_UNSIGNED_SHORT:
      type_size = sizeof(GLushort);
      break;
    case GL_FLOAT:
      type_size = sizeof(GLfloat);
      break;
    default:
      set_gl_error(GL_INVALID_ENUM);
      return U();
  }
  GLboolean normalized = FromV8<bool>(args[3], &ok); if (!ok) return U();
  GLsizei stride = FromV8<int32_t>(args[4], &ok); if (!ok) return U();
  GLintptr offset = FromV8<int32_t>(args[5], &ok); if (!ok) return U();
  if (size < 1 || size > 4 || stride < 0 || stride > 255 || offset < 0) {
    set_gl_error(GL_INVALID_VALUE);
    return U();
  }
  if ((stride % type_size) || (offset % type_size)) {
    set_gl_error(GL_INVALID_OPERATION);
    return U();
  }
  glVertexAttribPointer(indx, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
  return U();
}

// void viewport(GLint x, GLint y, GLsizei width, GLsizei height);
v8::Handle<v8::Value> WebGLRenderingContext::Callback_viewport(const v8::Arguments& args) {
  bool ok = true;
  GLint x = FromV8<int32_t>(args[0], &ok); if (!ok) return U();
  GLint y = FromV8<int32_t>(args[1], &ok); if (!ok) return U();
  GLsizei width = FromV8<int32_t>(args[2], &ok); if (!ok) return U();
  GLsizei height = FromV8<int32_t>(args[3], &ok); if (!ok) return U();
  glViewport(x, y, width, height);
  return U();
}

}
