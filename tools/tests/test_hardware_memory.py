"""RAM profile boundaries and relocated shell-only settings."""
import os,subprocess,tempfile,unittest,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class MemoryProfileTests(unittest.TestCase):
 def select(self,raw,mode='auto'):
  return subprocess.run(['bash','-c','source "$1"; sotc_select_staging_memory "$2" "$3" || exit $?; printf "%s:%s" "$SOTC_RAM_DETECTED_GIB" "$BB_STAGING_KEEP_MB"','fixture',str(ROOT/'tools/hardware-memory.sh'),str(raw),mode],capture_output=True,text=True)
 def test_thresholds(self):
  for gib,expected in [(8,512),(16,512),(23,512),(24,768),(31,768),(32,1024),(48,1024),(63,1024),(64,1536),(128,1536)]:
   result=self.select(gib*1024**3);self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,f'{gib}:{expected}')
 def test_conservative_and_invalid(self):
  self.assertEqual(self.select(64*1024**3,'conservative').stdout,'64:512')
  for value in ['', 'unknown','-1','1;echo BAD', '99999999999999999999']:
   self.assertEqual(self.select(value).stdout,'0:512')
  self.assertEqual(self.select('00009').stdout,'0:512')
  self.assertEqual(self.select(16*1024**3,'large').returncode,24)
 def test_relocated_settings(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)
   shutil.copy2(ROOT/'tools/runtime-settings.sh',p/'settings.sh')
   shutil.copy2(ROOT/'tools/hardware-memory.sh',p/'hardware-memory.sh')
   result=subprocess.run(['bash','-c','sotc_state="$1"; source "$1/settings.sh"; test "$BB_STAGING_KEEP_MB" = 512','fixture',str(p)],cwd='/tmp',env=dict(os.environ,SOTC_RAM_PROFILE='conservative'),capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
