"""Production interval policy controls; explicit flush and persistence stay identical."""
import os
import subprocess
import tempfile
from pathlib import Path
from check_buffer_copy_chunks import function
from build_archive_interval_trial import build

def main():
    rel='gpu/shadps4/video_core/renderer_metal/mtl_pso_cache.mm'
    old=(build.BASE/rel).read_text();new=(build.OUT/rel).read_text()
    for name in ('bool PsoCache::Flush()', 'bool PsoCache::WriteArchive()', 'PsoCache::~PsoCache()'):
        assert function(old,name)==function(new,name),name
    old_writer=function(old,'void PsoCache::WriterLoop()')
    new_writer=function(new,'void PsoCache::WriterLoop()')
    message='    std::printf("Metal binary archive: background write interval=%lld s; explicit flush unchanged\\n",\n                static_cast<long long>(kWriteInterval.count()));\n'
    assert new_writer.replace(message,'')==old_writer
    policy=function(new,'static std::chrono::seconds SotcArchiveWriteInterval()')
    with tempfile.TemporaryDirectory() as tmp:
        p=Path(tmp);src=p/'policy.cpp';exe=p/'policy'
        src.write_text('#include <chrono>\n#include <cstdlib>\n#include <cstdio>\n'+policy+
                       '\nint main(){printf("%lld\\n",(long long)SotcArchiveWriteInterval().count());}\n')
        subprocess.run(['clang++','-std=c++23','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
        cases={None:10,'':10,'10':10,'120':120,'600':600,'00120':120,'0':10,'9':10,
               '601':10,'-120':10,'+120':10,'120s':10,' 120':10,'120 ':10,'99999999999999999999':10}
        for value,expected in cases.items():
            env=dict(os.environ);env.pop('SOTC_ARCHIVE_WRITE_SECONDS',None)
            if value is not None:env['SOTC_ARCHIVE_WRITE_SECONDS']=value
            r=subprocess.run([str(exe)],env=env,capture_output=True,text=True,check=True)
            assert int(r.stdout)==expected,(value,r.stdout)
    print('PASS: 15 production policy cases; writer synchronization, explicit Flush, atomic persistence and destructor unchanged')
if __name__=='__main__':main()
