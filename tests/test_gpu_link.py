"""Link coverage fixtures do not load the renderer or need game input."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
import check_gpu_link as link


class GpuLinkTests(unittest.TestCase):
    def gpu_symbols(self):
        return '\n'.join('_' + name + ' T 0' for name in link.REQUIRED_EXPORTS)

    def test_macho_names_and_data_exports(self):
        result = link.audit(self.gpu_symbols() + '\n_runtime_guest_call U\n_bb_image_base U',
                            '_runtime_guest_call T 0\n_bb_image_base B 0')
        self.assertTrue(result['symbol_coverage_complete'])
        self.assertEqual(result['missing_host_symbols'], [])
        self.assertFalse(result['abi_verified'])
        self.assertFalse(result['renderer_launched'])

    def test_undefined_host_reference_cannot_satisfy_import(self):
        result = link.audit(self.gpu_symbols() + '\n_runtime_guest_call U', '_runtime_guest_call U')
        self.assertEqual(result['missing_host_symbols'], ['runtime_guest_call'])
        self.assertFalse(result['symbol_coverage_complete'])

    def test_missing_gpu_api_is_separate_from_host_coverage(self):
        result = link.audit('bbgpu_init T 0\nruntime_restart U', 'runtime_restart T 0')
        self.assertEqual(result['missing_host_symbols'], [])
        self.assertIn('bbgpu_resolve', result['missing_gpu_exports'])
        self.assertFalse(result['symbol_coverage_complete'])

    def test_external_system_libraries_are_not_host_hooks(self):
        result = link.audit(self.gpu_symbols() + '\n_runtime_restart U\n_malloc U\n_SDL_Init U',
                            '_runtime_restart T 0')
        self.assertEqual(result['required_host_symbols'], ['runtime_restart'])

    def test_empty_or_malformed_inventory_stops(self):
        for data in ('', 'nm: invalid binary', 'bbgpu_init T 0'):
            with self.subTest(data=data), self.assertRaises(ValueError):
                link.audit(data, '')

    def test_weak_undefined_and_local_symbols_do_not_satisfy_imports(self):
        result = link.audit(self.gpu_symbols() + '\nruntime_restart U', 'runtime_restart w\nruntime_restart t 0')
        self.assertEqual(result['missing_host_symbols'], ['runtime_restart'])

    @unittest.skipUnless((sys.platform.startswith('linux') or sys.platform == 'darwin') and shutil.which('cc') and shutil.which('nm'),
                         'Native fixture tools required')
    def test_actual_shared_library_and_host_export_inventory(self):
        with tempfile.TemporaryDirectory(prefix='gpu link fixture ') as temporary:
            root = Path(temporary)
            source = root / 'library.c'
            source.write_text('extern int runtime_fixture(void);\n' +
                '\n'.join('void %s(void) {}' % name for name in link.REQUIRED_EXPORTS) +
                '\nint fixture_call(void) { return runtime_fixture(); }\n')
            library = root / 'library.so'
            host = root / 'host'; host_source = root / 'host.c'
            library_flags = ['-dynamiclib', '-Wl,-undefined,dynamic_lookup'] if sys.platform == 'darwin' else ['-shared', '-fPIC']
            subprocess.run(['cc', *library_flags, str(source), '-o', str(library)], check=True)
            for exported in (False, True):
                host_source.write_text(('int runtime_fixture(void) { return 42; }\n' if exported else '') +
                                       'int main(void) { return 0; }\n')
                export_flag = '-Wl,-export_dynamic' if sys.platform == 'darwin' else '-rdynamic'
                subprocess.run(['cc', export_flag, str(host_source), '-o', str(host)], check=True)
                result = link.audit(link.inspect(library), link.inspect(host))
                self.assertEqual(result['symbol_coverage_complete'], exported)
                self.assertEqual(result['missing_host_symbols'], [] if exported else ['runtime_fixture'])


if __name__ == '__main__':
    unittest.main()
