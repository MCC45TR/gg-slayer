#!/usr/bin/python3
"""Cross-check the fitter against the exact integer evaluator used in the driver."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.fit_color_profile import fit,evaluate
from tools.color_calibration import TARGETS
samples=[]
for _,rgb in TARGETS:
    r,g,b=rgb
    samples.append({'screen':rgb,'led':[r-(min(r,max(g,b))*95+127)//255,g,b]})
model=fit({'complete':True,'samples':samples})
assert model['vertex_max_error']==0
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'model.bin';path.write_bytes(bytes.fromhex(model['hex']))
    result=subprocess.run([sys.argv[1],'--evaluate',str(path)],check=True,capture_output=True,text=True)
    rows=result.stdout.splitlines();assert len(rows)==4913
    for row in rows:
        values=list(map(int,row.split(',')))
        assert values[3:]==evaluate(model['vertices'],model['led_vertices'],model['tetrahedra'],values[:3]),row
print('20 measured anchors, full RGB cube coverage, 4913 Python/kernel integer comparisons: PASS')
