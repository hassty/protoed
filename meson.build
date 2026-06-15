project(
  'protoed',
  ['cpp', 'c'],
  version: '0.1',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=3', 'cpp_std=c++20', 'c_std=c99'],
)

gl_dep = dependency('gl', required: true)
glfw_dep = dependency('glfw', fallback: ['glfw', 'glfw_dep'], required: true, static: true)
protobuf_dep = dependency('protobuf', fallback: ['protobuf'], required: true, static: true)
imgui_dep = dependency('imgui', fallback: ['imgui'], required: true, static: true)
nfde_dep = dependency('nativefiledialog-extended', fallback: ['nativefiledialog-extended'], required: true, static: true)

dependencies = [
  gl_dep,
  glfw_dep,
  protobuf_dep,
  imgui_dep,
  nfde_dep,
]

sources = [
  'main.cpp',
  'glfw_native.cpp',
]

includes = [
  include_directories('vendor/cppcodec'),
]

cpp_args = []

if get_option('buildtype') == 'debug'
  cpp_args = cpp_args + ['-DENABLE_ASSERT']
endif

if host_machine.system() == 'linux'
  cpp_args = cpp_args + ['-DOS_IMPLEMENTATION_LINUX', '-Wno-gnu-zero-variadic-macro-arguments']
else
  cpp_args = cpp_args + ['-DOS_IMPLEMENTATION_WINDOWS']
endif

executable(
  'protoed',
  sources,
  include_directories: includes,
  install: true,
  dependencies: dependencies,
  cpp_args: cpp_args,
)
