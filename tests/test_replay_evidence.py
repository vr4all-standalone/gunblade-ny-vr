import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location(
    'gunblade_replay', Path(__file__).resolve().parents[1]/'tools/replay_model2_trace.py')
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)


class ReplayEvidenceTests(unittest.TestCase):
    def test_flat_or_panel_stereo_does_not_claim_world_geometry(self):
        for world, panel, kind in [(0, 0, 'native_raster'), (0, 8, 'panel_geometry'), (12, 2, 'world_geometry')]:
            with self.subTest(kind=kind):
                evidence = REPLAY.geometry_evidence(f'Geometry evidence: {world} world triangles, {panel} panel triangles\nGL test\n')
                self.assertEqual(evidence['evidence_kind'], kind)
                self.assertEqual(evidence['world_triangles'], world)
                self.assertEqual(evidence['panel_triangles'], panel)

    def test_old_replay_binary_without_geometry_evidence_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'rebuild'):
            REPLAY.geometry_evidence('Replayed 100 native polygons in three independent eye passes\n')


if __name__ == '__main__':
    unittest.main()
