#!/usr/bin/env python3
"""Render the actual particle fragment code in an OpenGL ES 3 mediump harness.

Requires Mesa EGL on Linux; no window, Android device, Python packages or key needed.
This isolates the material from scene geometry, lighting and CPU animation tests.
"""
import ctypes as C
import ctypes.util
import os
from pathlib import Path
import sys

os.environ.setdefault('LIBGL_ALWAYS_SOFTWARE', '1')
egl = C.CDLL(ctypes.util.find_library('EGL') or 'libEGL.so.1')


def api(name, result, *args):
    fn = getattr(egl, name)
    fn.restype, fn.argtypes = result, args
    return fn


ptr = C.c_void_p
integer = C.c_int
uint = C.c_uint
get_proc = api('eglGetProcAddress', ptr, C.c_char_p)
platform_display = C.CFUNCTYPE(ptr, uint, ptr, C.POINTER(integer))(
    get_proc(b'eglGetPlatformDisplayEXT'))
display = platform_display(0x31DD, None, None)  # EGL_PLATFORM_SURFACELESS_MESA
major, minor = integer(), integer()
if not api('eglInitialize', uint, ptr, C.POINTER(integer), C.POINTER(integer))(
        display, C.byref(major), C.byref(minor)):
    raise RuntimeError('Cannot initialize surfaceless EGL')
assert api('eglBindAPI', uint, uint)(0x30A0)  # EGL_OPENGL_ES_API
attributes = (integer * 13)(0x3033, 1, 0x3040, 0x40, 0x3024, 8,
                            0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038)
config, count = ptr(), integer()
assert api('eglChooseConfig', uint, ptr, C.POINTER(integer), C.POINTER(ptr),
           integer, C.POINTER(integer))(display, attributes, C.byref(config), 1, C.byref(count))
assert count.value == 1
size = 128
surface_attributes = (integer * 5)(0x3057, size, 0x3056, size, 0x3038)
surface = api('eglCreatePbufferSurface', ptr, ptr, ptr, C.POINTER(integer))(
    display, config, surface_attributes)
context_attributes = (integer * 3)(0x3098, 3, 0x3038)
context = api('eglCreateContext', ptr, ptr, ptr, ptr, C.POINTER(integer))(
    display, config, None, context_attributes)
assert surface and context
assert api('eglMakeCurrent', uint, ptr, ptr, ptr, ptr)(display, surface, surface, context)


def gl(name, result, *args):
    address = get_proc(name.encode())
    if not address:
        raise RuntimeError('Missing GLES function ' + name)
    return C.CFUNCTYPE(result, *args)(address)


create_shader = gl('glCreateShader', uint, uint)
shader_source = gl('glShaderSource', None, uint, integer, C.POINTER(C.c_char_p), ptr)
compile_shader = gl('glCompileShader', None, uint)
get_shader = gl('glGetShaderiv', None, uint, uint, C.POINTER(integer))
shader_log = gl('glGetShaderInfoLog', None, uint, integer, ptr, ptr)


def shader(kind, source):
    handle = create_shader(kind)
    code = C.c_char_p(source.encode())
    shader_source(handle, 1, C.byref(code), None)
    compile_shader(handle)
    valid = integer()
    get_shader(handle, 0x8B81, C.byref(valid))
    if not valid.value:
        log = C.create_string_buffer(16384)
        shader_log(handle, len(log), None, log)
        raise RuntimeError(log.value.decode())
    return handle


material_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('materials/effects.mat')
fragment = material_path.read_text().split('fragment {', 1)[1].rsplit('}', 1)[0]
vertex = shader(0x8B31, '''#version 300 es
precision highp float;
void main() {
    vec2 p=vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position=vec4(p*2.0-1.0,0,1);
}''')
fragment = shader(0x8B30, '''#version 300 es
precision mediump float;
precision highp int;
struct Params { int kind; highp float time; };
uniform Params materialParams;
struct MaterialInputs { vec4 baseColor; vec4 emissive; };
layout(location=0) out vec4 result;
void prepareMaterial(inout MaterialInputs m) { }
highp vec2 getUV0() { return gl_FragCoord.xy / 128.0; }
vec4 getColor() { return vec4(1.0,0.45,0.08,1.0); }
highp float getExposure() { return 1.0/40960.0; }
''' + fragment + '''
void main() {
    MaterialInputs m;material(m);
    if(any(isnan(m.emissive)) || any(isinf(m.emissive))) {
        result=vec4(1.0,0.0,1.0,1.0);return;
    }
    highp vec3 color=m.baseColor.rgb+m.emissive.rgb*
        mix(1.0,getExposure(),m.emissive.a)*m.baseColor.a;
    result=vec4(clamp(color,0.0,1.0),m.baseColor.a);
}''')
program = gl('glCreateProgram', uint)()
attach = gl('glAttachShader', None, uint, uint)
attach(program, vertex)
attach(program, fragment)
gl('glLinkProgram', None, uint)(program)
valid = integer()
gl('glGetProgramiv', None, uint, uint, C.POINTER(integer))(program, 0x8B82, C.byref(valid))
if not valid.value:
    log = C.create_string_buffer(16384)
    gl('glGetProgramInfoLog', None, uint, integer, ptr, ptr)(program, len(log), None, log)
    raise RuntimeError(log.value.decode())
gl('glUseProgram', None, uint)(program)
location = gl('glGetUniformLocation', integer, uint, C.c_char_p)
kind_location = location(program, b'materialParams.kind')
time_location = location(program, b'materialParams.time')
assert kind_location >= 0 and time_location >= 0
version = gl('glGetString', C.c_char_p, uint)(0x1F02).decode()
assert version.startswith('OpenGL ES 3'), version
precision_range, precision = (integer * 2)(), integer()
gl('glGetShaderPrecisionFormat', None, uint, uint, C.POINTER(integer), C.POINTER(integer))(
    0x8B30, 0x8DF1, precision_range, C.byref(precision))
print(f'{version}; mediump precision: {precision.value} bits')
gl('glViewport', None, integer, integer, integer, integer)(0, 0, size, size)
clear = gl('glClear', None, uint)
uniform_int = gl('glUniform1i', None, integer, integer)
uniform_float = gl('glUniform1f', None, integer, C.c_float)
draw = gl('glDrawArrays', None, uint, integer, integer)
read = gl('glReadPixels', None, integer, integer, integer, integer, uint, uint, ptr)
pixels = (C.c_ubyte * (size * size * 4))()
failures = []
for kind, name in enumerate(['fire', 'explosion', 'spray', 'lightning', 'smoke']):
    counts = []
    for time in [.3, 1.8, 3600.3]:
        clear(0x4000)
        uniform_int(kind_location, kind)
        uniform_float(time_location, time)
        draw(4, 0, 3)
        read(0, 0, size, size, 0x1908, 0x1401, pixels)
        visible = sum(pixels[i + 3] > 8 and max(pixels[i:i + 3]) > 3
                      for i in range(0, len(pixels), 4))
        invalid = sum(pixels[i] == 255 and pixels[i + 1] == 0 and pixels[i + 2] == 255
                      for i in range(0, len(pixels), 4))
        if invalid:
            failures.append(f'{name} produced non-finite emission at {time}s: {invalid} pixels')
        if visible < 200:
            failures.append(f'{name} vanished at {time}s: {visible} visible pixels')
        counts.append(visible)
    print(f'{name}: visible at 0.3, 1.8 and 3600.3 seconds: {counts}')
assert gl('glGetError', uint)() == 0
api('eglMakeCurrent', uint, ptr, ptr, ptr, ptr)(display, None, None, None)
api('eglDestroyContext', uint, ptr, ptr)(display, context)
api('eglDestroySurface', uint, ptr, ptr)(display, surface)
api('eglTerminate', uint, ptr)(display)

if failures:
    raise AssertionError('; '.join(failures))
