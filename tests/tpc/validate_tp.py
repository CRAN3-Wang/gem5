#!/usr/bin/env python3
"""Exercise real O3/T2/P1 cooperation through the project's production factory."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gem5', type=Path, default=ROOT / 'build/X86/gem5.fast')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/tp-validation')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env['LD_LIBRARY_PATH'] = str(Path(sys.base_prefix) / 'lib') + ':' + env.get('LD_LIBRARY_PATH', '')

    def run(command, log):
        with log.open('w') as output:
            subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                           stdout=output, stderr=subprocess.STDOUT, check=True, timeout=900)

    binary = out / 'pointer'
    run(['gcc', '-O2', '-Wall', '-Wextra', '-Werror', '-static', '-fno-pie', '-no-pie',
         ROOT / 'tests/tpc/pointer.c', '-o', binary], out / 'compile.log')
    symbols = subprocess.check_output(['nm', str(binary)], text=True)
    producer = int(re.search(r'^([0-9a-f]+) T p1_producer$', symbols, re.M)[1], 16)
    dependent = int(re.search(r'^([0-9a-f]+) T p1_dependent$', symbols, re.M)[1], 16)
    cases = [('no-prefetch', 'positive'), ('tpc-t', 'positive')]
    cases += [('tpc-tp', mode) for mode in ('positive', 'negative', 'zero', 'varying', 'independent')]
    cases += [('tpc', 'positive'), ('tpc-c', 'positive')]
    results = {}
    for profile, mode in cases:
        label = profile + '-' + mode
        native = subprocess.check_output([str(binary), mode, '4096', '4'], text=True)
        folder = out / label
        log = out / (label + '.log')
        run([args.gem5.resolve(), '-d', folder, ROOT / 'tests/tpc/t2.py',
             '--binary', binary, '--profile', profile, '--trace',
             '--max-ticks', '100000000000',
             '--args', mode, '4096', '4'], log)
        assert (folder / 'benchmark.stdout').read_text() == native, label
        text = log.read_text()
        assert 'TPC_FUNCTIONAL_PASS' in text, label
        stats = {}
        for line in (folder / 'stats.txt').read_text().splitlines():
            fields = line.split()
            if len(fields) > 1:
                try:
                    stats[fields[0]] = float(fields[1])
                except ValueError:
                    pass

        def component(source, metric):
            suffix = f'.{source}.{metric}'
            return sum(v for k, v in stats.items() if k.endswith(suffix))

        accounting = {}
        for source in ('T2', 'P1', 'C1'):
            def count(metric):
                return stats.get('system.prefetch_accounting.' + metric + '::' + source, 0)
            accepted = count('accepted')
            assert accepted == sum(count(k) for k in
                                   ('timelyUseful', 'late', 'unusedRemoved', 'aborted', 'unresolved')), label
            accounting[source] = {'accepted': accepted, 'timelyUseful': count('timelyUseful')}
        predictions = []
        retired_streams = {}
        feedback = []
        for line in text.splitlines():
            if not any(tag in line for tag in ('P1_PREDICT', 'T2_RETIRE', 'P1_VALUE', 'T2_FEEDBACK')):
                continue
            values = {k: int(v, 0) for k, v in re.findall(r'(\w+)=(-?0x[0-9a-f]+|-?\d+)', line)}
            if 'T2_RETIRE' in line:
                if values['matched']:
                    retired_streams[values['seq']] = values['stream']
                continue
            if 'P1_VALUE' in line:
                assert retired_streams.get(values['seq']) == values['stream'], line
                continue
            if 'T2_FEEDBACK' in line:
                assert values['effective_distance'] == values['base_distance'] * (2 if values['pointer'] else 1), line
                feedback.append(values)
                continue
            assert values['producer'] == producer and values['dependent'] == dependent, line
            assert values['va'] == values['pointer'] + values['delta'], line
            assert values['tick'] >= values['ready'], line
            predictions.append(values)
        if profile in ('tpc-t', 'tpc-tp', 'tpc'):
            assert component('t2', 'cpuLoads') > 0 and component('t2', 'branches') > 0, label
            # The sequential overwrite control can keep producer data resident,
            # so it need not activate/confirm a miss-trained T2 stream.
            if mode != 'independent':
                assert component('t2', 'confirmed') > 0, label
                assert accounting['T2']['accepted'] > 0, label
        if profile in ('tpc-tp', 'tpc') and mode in ('positive', 'negative', 'zero'):
            assert predictions and component('p1', 'trained') > 0, label
            expected = {'positive': 8, 'negative': -8, 'zero': 0}[mode]
            assert all(p['delta'] == expected for p in predictions), label
            # These require feedback from a real P1 relation into real T2, then
            # translation and exact-byte subscription. Retirement alone fails.
            assert component('t2', 'producerTranslations') > 0, label
            assert component('p1', 'residentValues') + component('p1', 'deferredValues') > 0, label
            assert any(f['pointer'] for f in feedback), label
            if mode == 'positive':
                assert component('p1', 'returns') > 0 and 'origin_source=1' in text, label
                assert accounting['P1']['accepted'] > 0 and accounting['P1']['timelyUseful'] > 0, label
        else:
            assert not predictions and accounting['P1']['accepted'] == 0, label
        results[label] = {'checksum_matches': True, 'predictions': len(predictions),
                          'producerTranslations': component('t2', 'producerTranslations'),
                          'residentValues': component('p1', 'residentValues'),
                          'deferredValues': component('p1', 'deferredValues'),
                          'returns': component('p1', 'returns'), 'accounting': accounting}
        print(label, 'PASS', flush=True)
    report = out / 'report.json'
    report.write_text(json.dumps(results, indent=2) + '\n')
    print(report)


if __name__ == '__main__':
    main()
