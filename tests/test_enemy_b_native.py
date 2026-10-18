"""User-edited native images must survive packing and subsequent default runs."""
import contextlib,io,json,sys,tempfile,unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import prepare_enemy_b as pack

class NativeInputs(unittest.TestCase):
    def test_native_edits_are_authoritative_and_never_overwritten(self):
        old=pack.SOURCE,pack.OUT,pack.GEN
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);pack.SOURCE=root/'source';pack.OUT=root/'native';pack.GEN=root/'generated'
            try:
                pack.SOURCE.mkdir();pack.OUT.mkdir()
                for name in pack.NAMES:
                    Image.new('RGBA',(2160,1460),(255,0,0,255)).save(pack.SOURCE/name)
                    Image.new('RGBA',(216,146),(0,255,0,127)).save(pack.OUT/name)
                before={n:((pack.OUT/n).read_bytes(),(pack.OUT/n).stat().st_mtime_ns) for n in pack.NAMES}
                with contextlib.redirect_stdout(io.StringIO()):pack.build(from_device=True)
                self.assertEqual(before,{n:((pack.OUT/n).read_bytes(),(pack.OUT/n).stat().st_mtime_ns) for n in pack.NAMES})
                Image.new('RGBA',(216,146),(0,0,255,191)).save(pack.OUT/pack.NAMES[0])
                edited=(pack.OUT/pack.NAMES[0]).read_bytes()
                with contextlib.redirect_stdout(io.StringIO()):pack.build();pack.check(None)
                self.assertEqual(edited,(pack.OUT/pack.NAMES[0]).read_bytes())
            finally:pack.SOURCE,pack.OUT,pack.GEN=old

if __name__=='__main__':unittest.main()
