// SPDX-License-Identifier: MIT
#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"
#include "Engine/Rendering/Renderer/Diagnostics/RenderSubmissionStats.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <limits>
#include <unordered_map>

namespace
{
std::atomic<std::uint64_t> nextId{1};
std::uint64_t NewId()
{
    const auto id = nextId.fetch_add(1);
    if (!id || id == std::numeric_limits<std::uint64_t>::max())
        std::terminate();
    return id;
}
GLenum TextureTarget(RHITextureKind kind)
{
    return kind == RHITextureKind::Cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
}
GLenum BufferTarget(RHIBufferUsage use)
{
    return use == RHIBufferUsage::Uniform ? GL_UNIFORM_BUFFER : GL_ARRAY_BUFFER;
    // Index storage can be uploaded through ARRAY_BUFFER without modifying a VAO.
}
struct Format
{
    GLint internal;
    GLenum external, type;
};
Format PixelFormat(RHITextureFormat format)
{
    switch (format)
    {
    case RHITextureFormat::RGBA8:
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case RHITextureFormat::SRGBA8:
        return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case RHITextureFormat::RGBA16F:
        return {GL_RGBA16F, GL_RGBA, GL_FLOAT};
    case RHITextureFormat::R8:
        return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case RHITextureFormat::Depth24:
        return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT};
    case RHITextureFormat::Depth24Stencil8:
        return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
    }
    throw RHIError("Unsupported texture format");
}
GLenum Stage(RHIShaderStage stage)
{
    switch (stage)
    {
    case RHIShaderStage::Vertex:
        return GL_VERTEX_SHADER;
    case RHIShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case RHIShaderStage::TessControl:
        return GL_TESS_CONTROL_SHADER;
    case RHIShaderStage::TessEvaluation:
        return GL_TESS_EVALUATION_SHADER;
    case RHIShaderStage::Geometry:
        return GL_GEOMETRY_SHADER;
    }
    throw RHIError("Unsupported shader stage");
}
GLenum Blend(RHIBlendFactor factor)
{
    switch (factor)
    {
    case RHIBlendFactor::Zero:
        return GL_ZERO;
    case RHIBlendFactor::One:
        return GL_ONE;
    case RHIBlendFactor::SrcAlpha:
        return GL_SRC_ALPHA;
    case RHIBlendFactor::OneMinusSrcAlpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    }
    throw RHIError("Unsupported blend factor");
}
RHIBlendFactor BlendFromGL(GLint value)
{
    if (value == GL_ZERO)
        return RHIBlendFactor::Zero;
    if (value == GL_ONE)
        return RHIBlendFactor::One;
    if (value == GL_SRC_ALPHA)
        return RHIBlendFactor::SrcAlpha;
    if (value == GL_ONE_MINUS_SRC_ALPHA)
        return RHIBlendFactor::OneMinusSrcAlpha;
    throw RHIError("Unsupported external blend state");
}
void Enable(GLenum state, bool enabled)
{
    if (enabled)
        glEnable(state);
    else
        glDisable(state);
}
struct Texture
{
    GLuint object;
    RHITextureDesc desc;
    bool attachment = false;
};
struct Buffer
{
    GLuint object;
    RHIBufferDesc desc;
};
struct VertexInput
{
    GLuint object;
    RHIVertexInputDesc desc;
};
struct Target
{
    GLuint object = 0, renderbuffer = 0;
    RHITargetDesc desc;
    RHITarget handles;
};
struct Query
{
    GLuint object;
    bool issued = false;
};
using PushGroup = void(APIENTRY *)(GLenum, GLuint, GLsizei, const GLchar *);
using PopGroup = void(APIENTRY *)();

// Resource operations preserve bindings; a candidate may fail without disturbing callers.
template<class F> struct Rollback
{
    F action;
    bool active = true;
    ~Rollback() { if (active) action(); }
};
template<class F> Rollback(F) -> Rollback<F>;

struct Bindings
{
    GLint draw, read, texture, cube, renderbuffer, array, uniform, vao, unpack;
    Bindings()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &cube);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniform);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    }
    ~Bindings()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, array);
        glBindBuffer(GL_UNIFORM_BUFFER, uniform);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
    }
};

class OpenGLDevice final : public RHIDevice, public RHICommandList
{
    template <class Map, class Handle> static auto &Find(Map &map, Handle handle)
    {
        auto found = map.find(handle.id);
        if (found == map.end())
            throw RHIError("Invalid, expired or foreign RHI handle");
        return found->second;
    }
    RHICapabilities m_Caps;
    std::unordered_map<std::uint64_t, Texture> m_Textures;
    std::unordered_map<std::uint64_t, Buffer> m_Buffers;
    std::unordered_map<std::uint64_t, VertexInput> m_Inputs;
    std::unordered_map<std::uint64_t, GLuint> m_Programs;
    std::unordered_map<std::uint64_t, Target> m_Targets;
    std::unordered_map<std::uint64_t, Query> m_Queries;
    GLuint m_Vao = 0;
    bool m_VaoKnown = false;
    RHIQueryHandle m_ActiveQuery;
    RHITargetHandle m_ActiveTarget;
    PushGroup m_Push = nullptr;
    PopGroup m_Pop = nullptr;
    bool m_Anisotropic = false;
    void DeleteTexture(RHITextureHandle handle)
    {
        if (handle == 0)
            return;
        auto &texture = Find(m_Textures, handle);
        glDeleteTextures(1, &texture.object);
        m_Textures.erase(handle.id);
    }
    GLint Location(RHIProgramHandle program, const std::string &name)
    {
        const auto object = Find(m_Programs, program);
        GLint current = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &current);
        if (static_cast<GLuint>(current) != object)
            throw RHIError("Uniform program is not bound");
        return glGetUniformLocation(object, name.c_str());
    }

  public:
    OpenGLDevice()
    {
        if (!glGetString || !glGetString(GL_VERSION))
            throw RHIError("RHI requires a current OpenGL context");
        GLint value = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
        m_Caps.maxTextureSize = value;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &value);
        m_Caps.uniformBufferAlignment = value;
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
        m_Caps.maxUniformBlockSize = value;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
        m_Caps.maxTextureUnits = value;
        m_Caps.timerQueries = glGenQueries && glBeginQuery && glEndQuery && glGetQueryObjectui64v;
        m_Push = reinterpret_cast<PushGroup>(glfwGetProcAddress("glPushDebugGroup"));
        m_Pop = reinterpret_cast<PopGroup>(glfwGetProcAddress("glPopDebugGroup"));
        if (!m_Push)
            m_Push = reinterpret_cast<PushGroup>(glfwGetProcAddress("glPushDebugGroupKHR"));
        if (!m_Pop)
            m_Pop = reinterpret_cast<PopGroup>(glfwGetProcAddress("glPopDebugGroupKHR"));
        m_Caps.debugGroups = m_Push && m_Pop;
        GLint extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensions);
        for (GLint i = 0; i < extensions; ++i)
            if (std::strcmp(reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i)),
                            "GL_EXT_texture_filter_anisotropic") == 0)
                m_Anisotropic = true;
    }
    ~OpenGLDevice() override
    {
        EndQuery();
        // Context must still be current, including initialization failure paths.
        while (!m_Targets.empty())
            DestroyTarget(m_Targets.begin()->first);
        while (!m_Inputs.empty())
            DestroyVertexInput(m_Inputs.begin()->first);
        while (!m_Textures.empty())
            DeleteTexture(m_Textures.begin()->first);
        while (!m_Buffers.empty())
            DestroyBuffer(m_Buffers.begin()->first);
        while (!m_Programs.empty())
            DestroyProgram(m_Programs.begin()->first);
        while (!m_Queries.empty())
            DestroyQuery(m_Queries.begin()->first);
    }
    const RHICapabilities &Capabilities() const override
    {
        return m_Caps;
    }
    RHICommandList &Commands() override
    {
        return *this;
    }
    RHITextureHandle CreateTexture(const RHITextureDesc &desc,
                                   const std::array<const void *, 6> &data) override
    {
        if (!desc.width || !desc.height || desc.width > m_Caps.maxTextureSize ||
            desc.height > m_Caps.maxTextureSize ||
            (desc.kind == RHITextureKind::Cubemap && desc.width != desc.height))
            return {};
        Bindings saved;
        GLuint object = 0;
        glGenTextures(1, &object);
        if (!object)
            return {};
        Rollback candidate{[&] { glDeleteTextures(1, &object); }};
        const GLenum target = TextureTarget(desc.kind);
        glBindTexture(target, object);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        const auto format = PixelFormat(desc.format);
        const unsigned faces = desc.kind == RHITextureKind::Cubemap ? 6 : 1;
        for (unsigned i = 0; i < faces; ++i)
            glTexImage2D(faces == 6 ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + i : target, 0, format.internal,
                         desc.width, desc.height, 0, format.external, format.type, data[i]);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                        desc.mipmaps   ? GL_LINEAR_MIPMAP_LINEAR
                        : desc.nearest ? GL_NEAREST
                                       : GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, desc.nearest ? GL_NEAREST : GL_LINEAR);
        const auto wrap = desc.wrap == RHIWrap::Repeat        ? GL_REPEAT
                          : desc.wrap == RHIWrap::ClampBorder ? GL_CLAMP_TO_BORDER
                                                              : GL_CLAMP_TO_EDGE;
        glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
        if (faces == 6)
            glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap);
        if (desc.compareDepth)
        {
            const float border[4] = {1, 1, 1, 1};
            glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, border);
            glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
        if (desc.mipmaps)
            glGenerateMipmap(target);
        if (desc.anisotropic && m_Anisotropic)
        {
            GLfloat maximum = 1;
            glGetFloatv(0x84FF, &maximum);
            glTexParameterf(target, 0x84FE, maximum);
        }
        if (glGetError() != GL_NO_ERROR)
        {
            return {};
        }
        const RHITextureHandle handle{NewId()};
        m_Textures.emplace(handle.id, Texture{object, desc});
        candidate.active = false;
        return handle;
    }
    bool DestroyTexture(RHITextureHandle handle) override
    {
        auto it = m_Textures.find(handle.id);
        if (it == m_Textures.end() || it->second.attachment)
            return false;
        DeleteTexture(handle);
        return true;
    }
    RHIBufferHandle CreateBuffer(const RHIBufferDesc &desc, const void *data) override
    {
        if (!desc.size || desc.size > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()))
            return {};
        Bindings saved;
        GLuint object = 0;
        glGenBuffers(1, &object);
        if (!object)
            return {};
        Rollback candidate{[&] { glDeleteBuffers(1, &object); }};
        const auto target = BufferTarget(desc.usage);
        glBindBuffer(target, object);
        glBufferData(target, desc.size, data, desc.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        if (glGetError() != GL_NO_ERROR)
        {
            return {};
        }
        const RHIBufferHandle handle{NewId()};
        m_Buffers.emplace(handle.id, Buffer{object, desc});
        candidate.active = false;
        return handle;
    }
    void UpdateBuffer(RHIBufferHandle handle, std::size_t offset, std::size_t size, const void *data) override
    {
        auto &buffer = Find(m_Buffers, handle);
        if (!data || offset > buffer.desc.size || size > buffer.desc.size - offset)
            throw RHIError("Buffer update exceeds allocation");
        Bindings saved;
        const auto target = BufferTarget(buffer.desc.usage);
        glBindBuffer(target, buffer.object);
        glBufferSubData(target, offset, size, data);
        if (glGetError() != GL_NO_ERROR)
            throw RHIError("Buffer update failed");
    }
    bool DestroyBuffer(RHIBufferHandle handle) override
    {
        auto it = m_Buffers.find(handle.id);
        if (it == m_Buffers.end())
            return false;
        for (const auto &input : m_Inputs)
            if (input.second.desc.vertices == handle || input.second.desc.indices == handle)
                return false;
        glDeleteBuffers(1, &it->second.object);
        m_Buffers.erase(it);
        return true;
    }
    RHIVertexInputHandle CreateVertexInput(const RHIVertexInputDesc &desc) override
    {
        auto &vertex = Find(m_Buffers, desc.vertices);
        if (vertex.desc.usage != RHIBufferUsage::Vertex || !desc.stride || desc.stride > INT_MAX)
            return {};
        GLuint index = 0;
        if (desc.indices != 0)
        {
            auto &buffer = Find(m_Buffers, desc.indices);
            if (buffer.desc.usage != RHIBufferUsage::Index)
                return {};
            index = buffer.object;
        }
        for (const auto &attribute : desc.attributes)
            if (!attribute.components || attribute.components > 4 || attribute.offset > desc.stride ||
                attribute.components * sizeof(float) > desc.stride - attribute.offset)
                return {};
        Bindings saved;
        GLuint object = 0;
        glGenVertexArrays(1, &object);
        if (!object)
            return {};
        Rollback candidate{[&] { glDeleteVertexArrays(1, &object); }};
        glBindVertexArray(object);
        glBindBuffer(GL_ARRAY_BUFFER, vertex.object);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index);
        for (const auto &attribute : desc.attributes)
        {
            glVertexAttribPointer(attribute.location, attribute.components, GL_FLOAT, GL_FALSE,
                                  static_cast<GLsizei>(desc.stride),
                                  reinterpret_cast<void *>(attribute.offset));
            glEnableVertexAttribArray(attribute.location);
        }
        if (glGetError() != GL_NO_ERROR)
        {
            return {};
        }
        const RHIVertexInputHandle handle{NewId()};
        m_Inputs.emplace(handle.id, VertexInput{object, desc});
        candidate.active = false;
        InvalidateState();
        return handle;
    }
    bool DestroyVertexInput(RHIVertexInputHandle handle) override
    {
        auto it = m_Inputs.find(handle.id);
        if (it == m_Inputs.end())
            return false;
        glDeleteVertexArrays(1, &it->second.object);
        m_Inputs.erase(it);
        InvalidateState();
        return true;
    }
    RHIProgramHandle CreateProgram(const std::vector<RHIShaderSource> &sources) override
    {
        struct Candidate
        {
            GLuint program = 0;
            std::vector<GLuint> shaders;
            ~Candidate()
            {
                for (auto shader : shaders)
                    glDeleteShader(shader);
                if (program)
                    glDeleteProgram(program);
            }
        } candidate;
        if (sources.empty())
            return {};
        candidate.shaders.reserve(sources.size());
        for (const auto &source : sources)
        {
            if (source.source.empty())
                return {};
            const GLuint shader = glCreateShader(Stage(source.stage));
            if (!shader)
                return {};
            candidate.shaders.push_back(shader);
            const char *text = source.source.c_str();
            glShaderSource(shader, 1, &text, nullptr);
            glCompileShader(shader);
            GLint success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                char log[2048]{};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                std::cerr << "[Shader Error] " << source.label << ": " << log << '\n';
                return {};
            }
        }
        candidate.program = glCreateProgram();
        if (!candidate.program)
            return {};
        for (auto shader : candidate.shaders)
            glAttachShader(candidate.program, shader);
        glLinkProgram(candidate.program);
        GLint success = 0;
        glGetProgramiv(candidate.program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char log[2048]{};
            glGetProgramInfoLog(candidate.program, sizeof(log), nullptr, log);
            std::cerr << "[Shader Error] Link: " << log << '\n';
            return {};
        }
        for (auto shader : candidate.shaders)
            glDetachShader(candidate.program, shader);
        const RHIProgramHandle handle{NewId()};
        m_Programs.emplace(handle.id, candidate.program);
        candidate.program = 0;
        return handle;
    }
    bool DestroyProgram(RHIProgramHandle handle) override
    {
        auto it = m_Programs.find(handle.id);
        if (it == m_Programs.end())
            return false;
        GLint current = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &current);
        if (static_cast<GLuint>(current) == it->second)
            glUseProgram(0);
        glDeleteProgram(it->second);
        m_Programs.erase(it);
        return true;
    }
    bool BindUniformBlock(RHIProgramHandle handle, const char *name, unsigned slot) override
    {
        const auto program = Find(m_Programs, handle);
        const auto index = glGetUniformBlockIndex(program, name);
        if (index == GL_INVALID_INDEX)
            return false;
        glUniformBlockBinding(program, index, slot);
        return true;
    }
    RHITarget CreateTarget(const RHITargetDesc &desc) override
    {
        if (!desc.width || !desc.height || desc.width > m_Caps.maxTextureSize ||
            desc.height > m_Caps.maxTextureSize || (!desc.color && !desc.depth))
            return {};
        Bindings saved;
        Target target;
        target.desc = desc;
        glGenFramebuffers(1, &target.object);
        if (!target.object)
            return {};
        auto rollback = [&]() {
            DeleteTexture(target.handles.color);
            DeleteTexture(target.handles.depth);
            if (target.renderbuffer)
                glDeleteRenderbuffers(1, &target.renderbuffer);
            glDeleteFramebuffers(1, &target.object);
        };
        Rollback candidate{rollback};
        glBindFramebuffer(GL_FRAMEBUFFER, target.object);
        RHITextureDesc texture;
        texture.width = desc.width;
        texture.height = desc.height;
        texture.wrap = RHIWrap::ClampEdge;
        if (desc.color)
        {
            texture.format = desc.colorFormat;
            texture.mipmaps = desc.mipmaps;
            texture.anisotropic = desc.mipmaps;
            target.handles.color = CreateTexture(texture, {});
            if (target.handles.color == 0)
            {
                return {};
            }
            auto &color = Find(m_Textures, target.handles.color);
            color.attachment = true;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.object, 0);
        }
        else
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
        if (desc.depth && (desc.sampleableDepth || desc.shadow))
        {
            texture.format = desc.shadow ? RHITextureFormat::Depth24 : RHITextureFormat::Depth24Stencil8;
            texture.mipmaps = false;
            texture.anisotropic = false;
            texture.nearest = !desc.shadow;
            texture.compareDepth = desc.shadow;
            texture.wrap = desc.shadow ? RHIWrap::ClampBorder : RHIWrap::ClampEdge;
            target.handles.depth = CreateTexture(texture, {});
            if (target.handles.depth == 0)
            {
                return {};
            }
            auto &depth = Find(m_Textures, target.handles.depth);
            depth.attachment = true;
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   desc.shadow ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth.object, 0);
        }
        else if (desc.depth)
        {
            glGenRenderbuffers(1, &target.renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, target.renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, desc.width, desc.height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                      target.renderbuffer);
        }
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
            glGetError() != GL_NO_ERROR)
        {
            return {};
        }
        target.handles.handle = RHITargetHandle{NewId()};
        m_Targets.emplace(target.handles.handle.id, target);
        candidate.active = false;
        return target.handles;
    }
    bool DestroyTarget(RHITargetHandle handle) override
    {
        auto it = m_Targets.find(handle.id);
        if (it == m_Targets.end())
            return false;
        auto &target = it->second;
        if (m_ActiveTarget == handle)
            m_ActiveTarget = 0;
        glDeleteFramebuffers(1, &target.object);
        if (target.renderbuffer)
            glDeleteRenderbuffers(1, &target.renderbuffer);
        DeleteTexture(target.handles.color);
        DeleteTexture(target.handles.depth);
        m_Targets.erase(it);
        return true;
    }
    void RemapTargetBindings(RHITargetHandle oldHandle, RHITargetHandle newHandle) override
    {
        if (oldHandle == 0)
            return;
        const auto &old = Find(m_Targets, oldHandle);
        const auto &next = Find(m_Targets, newHandle);
        if (m_ActiveTarget == oldHandle)
            m_ActiveTarget = newHandle;
        Bindings bindings;
        if (bindings.draw == static_cast<GLint>(old.object))
            bindings.draw = next.object;
        if (bindings.read == static_cast<GLint>(old.object))
            bindings.read = next.object;
        if (old.renderbuffer && bindings.renderbuffer == static_cast<GLint>(old.renderbuffer))
            bindings.renderbuffer = next.renderbuffer;
        if (old.handles.color != 0 && bindings.texture == static_cast<GLint>(Native(old.handles.color)))
            bindings.texture = Native(next.handles.color);
        if (old.handles.depth != 0 && bindings.texture == static_cast<GLint>(Native(old.handles.depth)))
            bindings.texture = Native(next.handles.depth);
    }
    void BeginTarget(RHITargetHandle handle, bool color, bool depth, float alpha) override
    {
        const auto &target = Find(m_Targets, handle);
        m_ActiveTarget = handle;
        glBindFramebuffer(GL_FRAMEBUFFER, target.object);
        glViewport(0, 0, target.desc.width, target.desc.height);
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        GLboolean mask[4], depthMask;
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        if (color || depth)
            glDisable(GL_SCISSOR_TEST);
        if (color)
        {
            glColorMask(1, 1, 1, 1);
            glClearColor(0, 0, 0, alpha);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        if (depth)
        {
            glDepthMask(1);
            glClearDepth(1);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        glDepthMask(depthMask);
        Enable(GL_SCISSOR_TEST, scissor);
    }
    void EndTarget() override
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_ActiveTarget = 0;
    }
    void GenerateMipmaps(RHITextureHandle handle) override
    {
        const auto &texture = Find(m_Textures, handle);
        if (!texture.desc.mipmaps)
            return;
        Bindings saved;
        const auto target = TextureTarget(texture.desc.kind);
        glBindTexture(target, texture.object);
        glGenerateMipmap(target);
    }
    void CopyTargetToCubemap(RHITargetHandle source, RHITextureHandle destination,
                             unsigned face, unsigned mip) override
    {
        const auto& target = Find(m_Targets, source);
        const auto& texture = Find(m_Textures, destination);
        unsigned maxMip = 0;
        if (texture.desc.mipmaps)
            for (unsigned size = texture.desc.width; size > 1; size >>= 1) ++maxMip;
        if (texture.desc.kind != RHITextureKind::Cubemap || texture.attachment || face >= 6 ||
            mip > maxMip || !target.handles.color ||
            target.desc.colorFormat != texture.desc.format ||
            target.desc.width != (texture.desc.width >> mip) ||
            target.desc.height != (texture.desc.height >> mip))
            throw RHIError("Invalid target-to-cubemap copy face, mip, extent or format");
        Bindings saved;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.object);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture.object);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, 0, 0, 0, 0,
                            target.desc.width, target.desc.height);
        if (glGetError() != GL_NO_ERROR)
            throw RHIError("GPU target-to-cubemap copy failed");
    }
    void BindTexture(RHITextureHandle handle, unsigned slot, RHITextureKind kind) override
    {
        if (slot >= m_Caps.maxTextureUnits)
            throw RHIError("Texture unit exceeds device limit");
        GLuint object = 0;
        if (handle != 0)
        {
            const auto &texture = Find(m_Textures, handle);
            if (m_ActiveTarget != 0)
            {
                const auto &output = Find(m_Targets, m_ActiveTarget).handles;
                if (handle == output.color || handle == output.depth)
                    throw RHIError("Texture sampling overlaps the current render target");
            }
            if (texture.desc.kind != kind)
                throw RHIError("Texture kind does not match binding");
            object = texture.object;
        }
        const auto target = TextureTarget(kind);
        RenderSubmissionStats::Get().RecordTextureBind(kind, slot, object);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(target, object);
    }
    void BindProgram(RHIProgramHandle handle) override
    {
        const auto object = handle == 0 ? 0 : Find(m_Programs, handle);
        RenderSubmissionStats::Get().RecordShaderBind(object);
        glUseProgram(object);
    }
    void SetInt(RHIProgramHandle p, const std::string &name, int value) override
    {
        glUniform1i(Location(p, name), value);
    }
    void SetFloats(RHIProgramHandle p, const std::string &name, const float *data, unsigned count) override
    {
        const auto location = Location(p, name);
        if (!data)
            throw RHIError("Missing uniform data");
        switch (count)
        {
        case 1:
            glUniform1fv(location, 1, data);
            break;
        case 3:
            glUniform3fv(location, 1, data);
            break;
        case 9:
            glUniformMatrix3fv(location, 1, GL_FALSE, data);
            break;
        case 16:
            glUniformMatrix4fv(location, 1, GL_FALSE, data);
            break;
        default:
            throw RHIError("Unsupported uniform shape");
        }
    }
    void BindUniformBuffer(RHIBufferHandle handle, unsigned slot, std::size_t offset,
                           std::size_t size) override
    {
        const auto &buffer = Find(m_Buffers, handle);
        if (buffer.desc.usage != RHIBufferUsage::Uniform || !size || offset > buffer.desc.size ||
            size > buffer.desc.size - offset || offset % m_Caps.uniformBufferAlignment ||
            size > m_Caps.maxUniformBlockSize)
            throw RHIError("Invalid uniform buffer range");
        glBindBufferRange(GL_UNIFORM_BUFFER, slot, buffer.object, offset, size);
    }
    void Draw(RHIVertexInputHandle handle, RHITopology topology, unsigned count, bool indexed,
              unsigned instances) override
    {
        const auto &input = Find(m_Inputs, handle);
        if (!count || !instances)
            return;
        if (count > INT_MAX || instances > INT_MAX)
            throw RHIError("Draw exceeds GL range");
        const auto &vertices = Find(m_Buffers, input.desc.vertices);
        if (!indexed && count > vertices.desc.size / input.desc.stride)
            throw RHIError("Draw exceeds vertex buffer");
        if (indexed && count > Find(m_Buffers, input.desc.indices).desc.size / sizeof(std::uint32_t))
            throw RHIError("Draw exceeds index buffer");
        if (!m_VaoKnown || m_Vao != input.object)
        {
            glBindVertexArray(input.object);
            m_Vao = input.object;
            m_VaoKnown = true;
        }
        auto &stats = RenderSubmissionStats::Get();
        if (instances > 1)
            stats.RecordMeshDrawInstanced(input.object, instances);
        else
            stats.RecordMeshDraw(input.object);
        const auto primitive = topology == RHITopology::Lines     ? GL_LINES
                               : topology == RHITopology::Patches ? GL_PATCHES
                                                                  : GL_TRIANGLES;
        if (topology == RHITopology::Patches)
            glPatchParameteri(GL_PATCH_VERTICES, 3);
        if (instances > 1)
        {
            if (indexed)
                glDrawElementsInstanced(primitive, count, GL_UNSIGNED_INT, nullptr, instances);
            else
                glDrawArraysInstanced(primitive, 0, count, instances);
        }
        else if (indexed)
            glDrawElements(primitive, count, GL_UNSIGNED_INT, nullptr);
        else
            glDrawArrays(primitive, 0, count);
        if (glGetError() != GL_NO_ERROR)
            throw RHIError("GPU draw failed");
    }
    RHIPipelineState GetPipelineState() const override
    {
        RHIPipelineState state;
        state.seamlessCubemap = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        state.depthTest = glIsEnabled(GL_DEPTH_TEST);
        state.blend = glIsEnabled(GL_BLEND);
        GLboolean depth;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
        state.depthWrite = depth;
        GLint value = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &value);
        state.depthCompare = value == GL_LEQUAL   ? RHICompare::LessEqual
                             : value == GL_ALWAYS ? RHICompare::Always
                                                  : RHICompare::Less;
        glGetIntegerv(GL_CULL_FACE_MODE, &value);
        state.cull = !glIsEnabled(GL_CULL_FACE) ? RHICull::None
                     : value == GL_FRONT        ? RHICull::Front
                                                : RHICull::Back;
        GLint modes[2];
        glGetIntegerv(GL_POLYGON_MODE, modes);
        state.wireframe = modes[0] == GL_LINE;
        state.offsetFill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        state.offsetLine = glIsEnabled(GL_POLYGON_OFFSET_LINE);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &state.offsetFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &state.offsetUnits);
        glGetIntegerv(GL_BLEND_SRC_RGB, &value);
        state.sourceRgb = BlendFromGL(value);
        glGetIntegerv(GL_BLEND_DST_RGB, &value);
        state.destinationRgb = BlendFromGL(value);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
        state.sourceAlpha = BlendFromGL(value);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &value);
        state.destinationAlpha = BlendFromGL(value);
        return state;
    }
    void SetPipelineState(const RHIPipelineState &state) override
    {
        Enable(GL_TEXTURE_CUBE_MAP_SEAMLESS, state.seamlessCubemap);
        Enable(GL_DEPTH_TEST, state.depthTest);
        glDepthMask(state.depthWrite);
        glDepthFunc(state.depthCompare == RHICompare::LessEqual ? GL_LEQUAL
                    : state.depthCompare == RHICompare::Always  ? GL_ALWAYS
                                                                : GL_LESS);
        Enable(GL_BLEND, state.blend);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(Blend(state.sourceRgb), Blend(state.destinationRgb), Blend(state.sourceAlpha),
                            Blend(state.destinationAlpha));
        Enable(GL_CULL_FACE, state.cull != RHICull::None);
        glCullFace(state.cull == RHICull::Front ? GL_FRONT : GL_BACK);
        glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
        Enable(GL_POLYGON_OFFSET_FILL, state.offsetFill);
        Enable(GL_POLYGON_OFFSET_LINE, state.offsetLine);
        glPolygonOffset(state.offsetFactor, state.offsetUnits);
    }
    void InvalidateState() override
    {
        m_VaoKnown = false;
    }
    RHIQueryHandle CreateQuery() override
    {
        if (!m_Caps.timerQueries)
            return {};
        GLuint object = 0;
        glGenQueries(1, &object);
        if (!object)
            return {};
        Rollback candidate{[&] { glDeleteQueries(1, &object); }};
        const RHIQueryHandle handle{NewId()};
        m_Queries.emplace(handle.id, Query{object});
        candidate.active = false;
        return handle;
    }
    bool DestroyQuery(RHIQueryHandle handle) override
    {
        auto it = m_Queries.find(handle.id);
        if (it == m_Queries.end() || handle == m_ActiveQuery)
            return false;
        glDeleteQueries(1, &it->second.object);
        m_Queries.erase(it);
        return true;
    }
    void BeginQuery(RHIQueryHandle handle) override
    {
        if (m_ActiveQuery != 0)
            throw RHIError("Nested elapsed queries are not supported");
        auto &query = Find(m_Queries, handle);
        glBeginQuery(GL_TIME_ELAPSED, query.object);
        query.issued = true;
        m_ActiveQuery = handle;
    }
    void EndQuery() override
    {
        if (m_ActiveQuery != 0)
        {
            glEndQuery(GL_TIME_ELAPSED);
            m_ActiveQuery = 0;
        }
    }
    bool TryQueryResult(RHIQueryHandle handle, std::uint64_t &value) override
    {
        auto &query = Find(m_Queries, handle);
        if (!query.issued || m_ActiveQuery == handle)
            return false;
        GLuint ready = GL_FALSE;
        glGetQueryObjectuiv(query.object, GL_QUERY_RESULT_AVAILABLE, &ready);
        if (!ready)
            return false;
        GLuint64 result = 0;
        glGetQueryObjectui64v(query.object, GL_QUERY_RESULT, &result);
        value = result;
        return true;
    }
    bool PushDebugGroup(const char *name) override
    {
        if (!m_Caps.debugGroups || !name)
            return false;
        m_Push(0x824A, 0, static_cast<GLsizei>(std::strlen(name)), name);
        return true;
    }
    void PopDebugGroup() override
    {
        if (m_Caps.debugGroups)
            m_Pop();
    }
    unsigned Native(RHITextureHandle h)
    {
        return h == 0 ? 0 : Find(m_Textures, h).object;
    }
    unsigned Native(RHITargetHandle h)
    {
        return h == 0 ? 0 : Find(m_Targets, h).object;
    }
    unsigned Native(RHIProgramHandle h)
    {
        return h == 0 ? 0 : Find(m_Programs, h);
    }
};
} // namespace
std::unique_ptr<RHIDevice> CreateRHIDevice()
{
    return std::make_unique<OpenGLDevice>();
}
unsigned GetOpenGLTexture(RHIDevice &d, RHITextureHandle h)
{
    return dynamic_cast<OpenGLDevice &>(d).Native(h);
}
unsigned GetOpenGLTarget(RHIDevice &d, RHITargetHandle h)
{
    return dynamic_cast<OpenGLDevice &>(d).Native(h);
}
unsigned GetOpenGLProgram(RHIDevice &d, RHIProgramHandle h)
{
    return dynamic_cast<OpenGLDevice &>(d).Native(h);
}
