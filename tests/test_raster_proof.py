from pathlib import Path
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import raster_proof

class RasterProofTest(unittest.TestCase):
    def pair(self,root,skip=True):
        a=root/'a';b=root/'b';a.mkdir();b.mkdir()
        for folder in (a,b):
            for region,(_,size) in raster_proof.REGIONS.items():
                (folder/(region+'-000.bin')).write_bytes(bytes(size))
            (folder/'native.tsv').write_text('time\tmode\trender_mode\n0\t11\t0\n')
            (folder/'frame-000.png').write_bytes(b'same native framebuffer')
            decision='skip' if folder==b and skip else 'reference'
            (folder/'raster.tsv').write_text('decision\tpolygons\n'+decision+'\t1\n')
        return a,b

    def test_complete_ram_bytes_and_native_trace_are_counted(self):
        with tempfile.TemporaryDirectory() as path:
            a,b=self.pair(Path(path))
            result=raster_proof.compare(a,b)
            self.assertEqual(result['ram_bytes_compared'],0x144000)
            self.assertEqual(result['ram_checkpoints'],dict(workram=1,extraram=1,backup=1))

    def test_unexercised_optimization_is_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            a,b=self.pair(Path(path),False)
            with self.assertRaisesRegex(ValueError,'never exercised'):raster_proof.compare(a,b)

    def test_fallback_requires_identical_native_pngs(self):
        with tempfile.TemporaryDirectory() as path:
            a,b=self.pair(Path(path),False)
            self.assertEqual(raster_proof.compare(a,b,fallback=True)['fallback_pngs_identical'],1)
            (b/'frame-000.png').write_bytes(b'different')
            with self.assertRaisesRegex(ValueError,'capture differs'):raster_proof.compare(a,b,fallback=True)

    def test_actual_direct_framebuffer_capture_must_match(self):
        with tempfile.TemporaryDirectory() as path:
            a,b=self.pair(Path(path))
            for folder in (a,b):
                (folder/'native.tsv').write_text('time\tmode\trender_mode\n0\t2\t1\n')
                with (folder/'raster.tsv').open('a') as stream:stream.write('direct_framebuffer\t0\n')
            (b/'frame-000.png').write_bytes(b'different direct output')
            with self.assertRaisesRegex(ValueError,'Direct framebuffer capture differs'):
                raster_proof.compare(a,b)

    def test_empty_evidence_is_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            with self.assertRaisesRegex(ValueError,'No RAM'):
                raster_proof.compare(Path(path),Path(path))

    def test_missing_checkpoint_is_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path);a=root/'a';b=root/'b';a.mkdir();b.mkdir()
            (a/'workram-001.bin').write_bytes(b'a')
            with self.assertRaisesRegex(ValueError,'checkpoint'):
                raster_proof.compare(a,b)

    def test_byte_mismatch_reports_address(self):
        with tempfile.TemporaryDirectory() as path:
            root=Path(path);a=root/'a';b=root/'b';a.mkdir();b.mkdir()
            (a/'workram-001.bin').write_bytes(b'abc');(b/'workram-001.bin').write_bytes(b'axc')
            with self.assertRaisesRegex(ValueError,'0x500001'):
                raster_proof.compare(a,b)

if __name__=='__main__':unittest.main()
