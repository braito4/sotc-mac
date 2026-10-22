"""Offline regression for copied dependency metadata with spaced install roots."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('relocate_workspace',
    Path(__file__).parents[1]/'tools/relocate_workspace.py')
relocation=importlib.util.module_from_spec(spec)
spec.loader.exec_module(relocation)


class RelocationTests(unittest.TestCase):
    def test_raw_and_pkgconfig_escaped_roots_relocate_without_leftover_escapes(self):
        old='/tmp/original workspace/project';new=Path('/tmp/new-project')
        for escaped in (old,old.replace(' ','\\ '),old.replace(' ','\\\\ ')):
            text='prefix='+escaped+'/deps\nexec_prefix='+escaped+'/deps\n'
            result=relocation.remap(text,{old},new)
            self.assertEqual(result,'prefix=/tmp/new-project/deps\nexec_prefix=/tmp/new-project/deps\n')
            self.assertEqual(relocation.remap(result,{old},new),result)

    def test_metadata_is_idempotent_and_does_not_follow_symlinks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);path=root/'fmt.pc';link=root/'redirect.pc'
            path.write_text('prefix=/tmp/previous\\\\ workspace/project/deps\n')
            self.assertTrue(relocation.metadata(path,{'/tmp/previous workspace/project'}))
            self.assertEqual(path.read_text(),'prefix='+str(relocation.ROOT)+'/deps\n')
            self.assertFalse(relocation.metadata(path,{'/tmp/previous workspace/project'}))
            link.symlink_to(path)
            self.assertFalse(relocation.metadata(link,{'/tmp/previous workspace/project'}))


if __name__=='__main__':
    unittest.main()
