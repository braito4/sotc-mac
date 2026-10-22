"""Compile bounded metadata lookup against the actual pinned BinaryInfo layout."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class ShaderMetadataTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_real_binary_info_layout_and_bounded_lookup(self):
        header=ROOT/'runtime/upstream/gpu/shadps4/video_core/amdgpu/regs_shader.h'
        source=header.read_text()
        begin=source.index('struct BinaryInfo {')
        end=source.index('enum class FpRoundMode',begin)
        with tempfile.TemporaryDirectory(prefix='shader metadata ') as directory:
            work=Path(directory)
            (work/'binary_info_fixture.h').write_text(
                '#include <array>\n#include <cstdint>\nusing u8=std::uint8_t;'
                'using u32=std::uint32_t;using u64=std::uint64_t;\n'+source[begin:end])
            binary=work/'check'
            subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror',
                            '-I'+str(work),'-I'+str(ROOT/'runtime/src'),
                            str(ROOT/'tests/test_shader_metadata.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=15)

    @unittest.skipUnless(shutil.which('c++') and (ROOT/'work/gpu-source-22923f080229/deps/macos-arm64/include/fmt/format.h').is_file(),
                         'C++ compiler and prepared renderer headers required')
    def test_runtime_wrapper_and_required_failure_evidence(self):
        import os
        source=ROOT/'runtime/upstream'
        prefix=ROOT/'work/gpu-source-22923f080229/deps/macos-arm64'
        with tempfile.TemporaryDirectory(prefix='shader lookup ') as directory:
            work=Path(directory)
            binary=work/'lookup'
            subprocess.run(['c++','-std=c++23','-Wall','-Wextra',
                            '-I'+str(ROOT/'runtime/src'), '-I'+str(source/'gpu/shim'),
                            '-I'+str(source/'gpu/shadps4'), '-I'+str(prefix/'include'),
                            str(ROOT/'tests/test_shader_lookup.cpp'), '-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=15)
            result=subprocess.run([str(binary),'fail'],capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,BB_GPU_USER_DIR=str(work)))
            self.assertEqual(result.returncode,23,result.stderr)
            self.assertIn('STOP_GPU_SHADER_METADATA: role=synthetic-required',result.stderr)
            folder=work/'shader-metadata-failure'
            self.assertEqual((folder/'code.bin').read_bytes(),bytes(4096))
            self.assertIn('role=synthetic-required',(folder/'context.txt').read_text())
            # A second failure preserves the first evidence instead of overwriting it.
            (folder/'context.txt').write_text('preserve first capture')
            result=subprocess.run([str(binary),'fail'],capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,BB_GPU_USER_DIR=str(work)))
            self.assertEqual(result.returncode,23)
            self.assertEqual((folder/'context.txt').read_text(),'preserve first capture')

if __name__=='__main__':
    unittest.main()
