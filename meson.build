
project(
	'CodeOrb - Microcontroller Debugging',
	'cpp',
)

fs = import('fs')
cxx = meson.get_compiler('cpp')

# Sources

srcs = [
	'deps/ProcessingCore/Processing.cpp',
	'deps/ProcessingCore/Log.cpp',
	'deps/ProcessingCore/SystemDebugging.cpp',
	'deps/ProcessingCore/SystemCommanding.cpp',
	'deps/ProcessingCore/TcpListening.cpp',
	'deps/ProcessingCore/TcpTransfering.cpp',
	'deps/ProcessingCommon/Res.cpp',
	'deps/ProcessingCommon/LibTime.cpp',
	'deps/ProcessingCommon/LibFilesys.cpp',
	'deps/ProcessingCommon/LibDspc.cpp',
	'deps/ProcessingCommon/KeyUser.cpp',
	'src/main.cpp',
	'src/GwSupervising.cpp',
	'src/GwMsgDispatching.cpp',
	'src/SingleWireControlling.cpp',
	'src/RemoteCommanding.cpp',
	'src/LibUart.cpp',
	'src/TelnetFiltering.cpp',
	'src/InfoGathering.cpp',
]

# Arguments

args = [
	'-DdPackageName="' + meson.project_name() + '"',
	'-DdAppName="codeorb"',
	'-DdVersion="CodeOrb_1-25.04-1"',
	'-DCONFIG_PROC_HAVE_LOG=1',
	'-DCONFIG_CMD_SIZE_HISTORY=20',
	'-DCONFIG_CMD_SIZE_BUFFER_OUT=2048',
]

# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
warningsLinux = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',
	'-Werror',
	'-Wfatal-errors',
	'-Wreorder',
	'-Wswitch-enum',
	'-Wuseless-cast',
	'-Wparentheses',
	'-Wshift-overflow',
	'-Wsign-compare',
	'-Wzero-as-null-pointer-constant',
	'-Wcast-align',
	'-Wcast-qual',
	'-Wcatch-value',
	'-Wchar-subscripts',
	'-Wswitch-default',
	'-Wctor-dtor-privacy',
	'-Wduplicated-branches',
	'-Wduplicated-cond',
	'-Wempty-body',
	'-Wextra-semi',
	'-Wfloat-equal',
	'-Wformat',
	'-Wformat-extra-args',
	'-Wimplicit-fallthrough',
	'-Wmissing-field-initializers',
	'-Wnull-dereference',
	'-Wshadow',
]

if host_machine.system() == 'windows'
	args += '-D_WIN32_WINNT=_WIN32_WINNT_WIN10'
	args += '-DWINVER=_WIN32_WINNT_WIN10'
else
	args += '-std=gnu++11'
endif

if cxx.get_id() == 'msvc'
	args += '/std:c++17'
elif cxx.get_id() == 'gcc'
	args += warningsLinux
endif

# Dependencies

deps = []
deps += dependency('threads')

if host_machine.system() == 'windows'
	deps += cxx.find_library('ws2_32')
endif

if fs.is_dir('deps/tclap_loc')
	message('submodule tclap found')

	incTclap = include_directories(['deps/tclap_loc/include'], is_system: true)
	deps += declare_dependency(include_directories: incTclap)
	args += '-DAPP_HAS_TCLAP=1'
else
	warning('submodule tclap not found. Limited CLI options')
endif

# Application

myApp = executable(
	'codeorb',
	[
		srcs,
	],
	include_directories : include_directories([
		'./deps/ProcessingCore',
		'./deps/ProcessingCommon',
		'./src',
	]),
	dependencies : [
		deps,
	],
	cpp_args : [
		args,
	],
)

