project(
  'protoed',
  ['cpp', 'c'],
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23', 'c_std=c99'],
)

add_project_arguments(['-DOS_IMPLEMENTATION_LINUX', '-DENABLE_ASSERT', '-Wno-gnu-zero-variadic-macro-arguments',], language: 'cpp')

gl_dep = dependency('gl', required: true)
glfw_dep = dependency('glfw3', required: true)
protobuf_dep = dependency('protobuf', required: true)

dependencies = [
  gl_dep,
  glfw_dep,
  protobuf_dep,
]

sources = [
  'main.cpp',
  'vendor/imgui/imgui.cpp',
  'vendor/imgui/imgui_demo.cpp',
  'vendor/imgui/imgui_draw.cpp',
  'vendor/imgui/imgui_tables.cpp',
  'vendor/imgui/imgui_widgets.cpp',
  'vendor/imgui/misc/cpp/imgui_stdlib.cpp',
  'vendor/imgui/backends/imgui_impl_glfw.cpp',
  'vendor/imgui/backends/imgui_impl_opengl3.cpp',
]

protoc = find_program('protoc')
proto_dir = meson.current_source_dir() / 'proto'

# TODO: generated dir inside build dir
gen = generator(protoc,
                output: ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
                arguments: ['--proto_path=' + proto_dir,
                            '--cpp_out=@BUILD_DIR@',
                            '@INPUT@'])

generated = gen.process(proto_dir / 'msg.proto')

includes = [
  include_directories('vendor/imgui'),
  include_directories('vendor/imgui/backends'),
  include_directories('vendor/cppcodec'),
]

executable(
  'protoed',
  sources,
  generated,
  include_directories: includes,
  install : true,
  cpp_args: ['-DIMGUI_DEFINE_MATH_OPERATORS'],
  dependencies : dependencies,
)
