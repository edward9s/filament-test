"""Exercise the real C++ exporter, including CPU columns and JSON escaping."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_compare import fixture
from compare import load


class NativeExportTests(unittest.TestCase):
    def test_native_csv_json_round_trip(self):
        root=Path(__file__).resolve().parents[1]
        subprocess.run(['python3',str(root/'benchmark/build.py')],check=True,stdout=subprocess.DEVNULL)
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp)
            files=fixture()
            metadata=json.loads(files['metadata.json'])
            for key in ('cpu_scene_ms','cpu_effects_ms','cpu_submit_ms','cpu_wait_ms'):
                metadata['metrics'][key]=dict(unit='ms',kind='cpu',source='synthetic fixture',scope='synthetic known duration',scope_id='fixture.'+key)
            source=tmp/'metadata.json';source.write_text(json.dumps(metadata))
            subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror',str(root/'benchmark/test_export.cpp'),'-o',str(tmp/'export')],check=True)
            subprocess.run([str(tmp/'export'),str(source),str(tmp/'output')],check=True)
            runs=load([tmp/'output']);self.assertEqual(len(runs),1)
            self.assertEqual(runs[0]['summary']['application']['average_fps'],10)
            frame=runs[0]['frames'][0]
            for key,value in [('cpu_scene_ms',.2),('cpu_effects_ms',.3),('cpu_submit_ms',.4),('cpu_wait_ms',.5)]:
                self.assertEqual(float(frame[key]),value)
            self.assertEqual(json.loads((tmp/'output/quote.json').read_text()),'測試\n"\\\x01')


if __name__=='__main__':unittest.main()
