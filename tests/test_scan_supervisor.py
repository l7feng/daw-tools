import importlib.util, json, sys, tempfile, unittest
from pathlib import Path
spec = importlib.util.spec_from_file_location('scanner', Path(__file__).resolve().parents[1] / 'scripts/scan-plugins.py')
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)

class SupervisorTests(unittest.TestCase):
    def test_failures_do_not_stop_later_candidates(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            worker = root / 'worker.py'
            worker.write_text("""import json,sys,time
from pathlib import Path
root=Path(sys.argv[sys.argv.index('--workspace')+1])
name=Path(json.loads((root/'scan-request.json').read_text())['path']).stem
if name=='crash':sys.exit(23)
if name=='hang':time.sleep(30)
if name=='malformed':(root/'scan-result.xml').write_text('<bad>')
else:(root/'scan-result.xml').write_text('<PLUGIN_SCAN><PLUGIN name="test"/></PLUGIN_SCAN>')
""", encoding='utf-8')
            candidates = [root / (name + '.vst3') for name in ['ok', 'crash', 'hang', 'malformed', 'after']]
            results = scanner.scan_batch(candidates + candidates[:1], root / 'jobs', [sys.executable, str(worker)], 1)
            self.assertEqual([r['status'] for r in results], ['ok', 'failed', 'timeout', 'failed', 'ok'])
            self.assertEqual(results[1]['exit_code'], 23)
            self.assertEqual(json.loads((root/'jobs/report.json').read_text(encoding='utf-8')), results)

if __name__ == '__main__':unittest.main()
