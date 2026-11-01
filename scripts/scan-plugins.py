"""Explicit candidates only: one disposable process per VST3, no device setup."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, subprocess, uuid
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[1]

def scan_batch(candidates, folder, command, timeout=20, verify_fixture=False):
    folder.mkdir(parents=True, exist_ok=False)
    records = []
    seen = set()
    for candidate in candidates:
        path = Path(candidate).resolve()
        key = os.path.normcase(str(path))
        if key in seen:
            continue
        seen.add(key)
        job = folder / str(len(records))
        job.mkdir()
        (job / 'studio-validation.marker').write_text('isolated plugin scan')
        (job / 'scan-request.json').write_text(json.dumps({'path': str(path), 'verifyStudioFixture': verify_fixture}), encoding='utf-8')
        (job / 'temp').mkdir()
        env = dict(os.environ, TEMP=str(job / 'temp'), TMP=str(job / 'temp'))
        record = {'path': str(path), 'job': str(job), 'status': 'failed'}
        try:
            with (job / 'stdout.log').open('wb') as out, (job / 'stderr.log').open('wb') as err:
                result = subprocess.run([*command, '--workspace', str(job), '--scan-plugin'],
                                        env=env, stdout=out, stderr=err, timeout=timeout)
            record['exit_code'] = result.returncode
            if result.returncode != 0:
                record['error'] = (job / 'FAIL.txt').read_text(encoding='utf-8') if (job / 'FAIL.txt').exists() else 'Worker terminated without successful result'
            else:
                tree = ET.parse(job / 'scan-result.xml').getroot()
                if tree.tag != 'PLUGIN_SCAN' or not list(tree):
                    raise ValueError('Empty or invalid plugin description list')
                record['plugins'] = [dict(item.attrib) for item in tree]
                record['status'] = 'ok'
        except subprocess.TimeoutExpired:
            record.update(status='timeout', error='Worker exceeded timeout and was terminated')
        except (OSError, ValueError, ET.ParseError) as error:
            record['error'] = str(error)
        records.append(record)
        # A completed candidate is persisted even if later scans fail.
        partial = folder / 'report.partial'
        partial.write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
        partial.replace(folder / 'report.json')
    return records

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plugins', nargs='+', type=Path)
    parser.add_argument('--verified', action='store_true', help='Use the independently linked current application')
    parser.add_argument('--timeout', type=float, default=20)
    parser.add_argument('--verify-studio-fixture', action='store_true', help='Only for the in-repository Studio Test Gain fixture')
    args = parser.parse_args()
    if not 0 < args.timeout <= 300:
        parser.error('timeout must be within (0, 300] seconds')
    executable = ROOT / 'build/editor/StudioWorkbench_artefacts/Release' / ('Studio Workbench Verified.exe' if args.verified else 'Studio Workbench.exe')
    folder = ROOT / 'build/native-validation' / ('scan-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    records = scan_batch(args.plugins, folder, [str(executable)], args.timeout, args.verify_studio_fixture)
    (folder / 'worker-sha256.txt').write_text(hashlib.sha256(executable.read_bytes()).hexdigest())
    print(folder / 'report.json')
    return 0 if all(item['status'] == 'ok' for item in records) else 1

if __name__ == '__main__':
    raise SystemExit(main())
