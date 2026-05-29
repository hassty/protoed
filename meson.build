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

dependencies = [
  gl_dep,
  glfw_dep,
  protobuf_dep,
  imgui_dep,
]

sources = [
  'main.cpp',
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
  include_directories('vendor/cppcodec'),
]

cpp_args = host_machine.system() == 'linux' \
  ? ['-DOS_IMPLEMENTATION_LINUX', '-DENABLE_ASSERT', '-Wno-gnu-zero-variadic-macro-arguments'] \
  : ['-DOS_IMPLEMENTATION_WINDOWS', '-DENABLE_ASSERT']

link_args = host_machine.system() == 'linux' \
  ? ['-Wl,--no-as-needed'] \
  : []

executable(
  'protoed',
  sources,
  generated,
  include_directories: includes,
  install: true,
  dependencies: dependencies,
  cpp_args: cpp_args,
  link_args: link_args,
)
