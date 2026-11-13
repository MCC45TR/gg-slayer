#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Create a bounded tetrahedral RGB transform from measured screen/LED pairs."""
import argparse
import itertools
import json
from pathlib import Path
import struct
import subprocess
import time

MAGIC=b'SC3D'+bytes([1])

def det(a,b,c):
    return a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def solve(matrix,rhs):
    a=[list(map(float,row))+[float(y)] for row,y in zip(matrix,rhs)]
    for column in range(3):
        pivot=max(range(column,3),key=lambda i:abs(a[i][column]))
        if abs(a[pivot][column])<1e-12:raise ValueError('Degenerate tetrahedron')
        a[column],a[pivot]=a[pivot],a[column]
        divisor=a[column][column]
        a[column]=[x/divisor for x in a[column]]
        for row in range(3):
            if row!=column:
                factor=a[row][column];a[row]=[x-factor*y for x,y in zip(a[row],a[column])]
    return tuple(row[3] for row in a)
def sphere(points,tetra):
    a=points[tetra[0]]
    others=[points[i] for i in tetra[1:]]
    center=solve([[2*(y-x) for x,y in zip(a,b)] for b in others],
                 [sum(y*y for y in b)-sum(x*x for x in a) for b in others])
    radius=sum((x-y)**2 for x,y in zip(center,a))
    return center,radius

def triangulate(inputs):
    points=[tuple(v/255 for v in p) for p in inputs]
    n=len(points)
    points.extend([(-10.,-10.,-10.),(30.,-10.,-10.),(-10.,30.,-10.),(-10.,-10.,30.)])
    cells=[(n,n+1,n+2,n+3)]
    for index in range(n):
        point=points[index];bad=[]
        for cell in cells:
            center,radius=sphere(points,cell)
            if sum((x-y)**2 for x,y in zip(point,center))<=radius+1e-10:bad.append(cell)
        if not bad:raise ValueError('Point was not enclosed during triangulation')
        faces={}
        for cell in bad:
            for face in itertools.combinations(cell,3):
                key=tuple(sorted(face));faces[key]=faces.get(key,0)+1
        cells=[cell for cell in cells if cell not in bad]
        for face,count in faces.items():
            if count!=1:continue
            a,b,c=(points[i] for i in face)
            if abs(det(sub(b,a),sub(c,a),sub(point,a)))>1e-12:cells.append((*face,index))
    result=[cell for cell in cells if all(i<n for i in cell)]
    volume=sum(abs(det(sub(inputs[b],inputs[a]),sub(inputs[c],inputs[a]),sub(inputs[d],inputs[a]))) for a,b,c,d in result)
    if volume!=6*255**3:raise ValueError(f'Triangulation does not cover RGB cube: {volume}')
    if len(result)>128:raise ValueError('Too many tetrahedra')
    if {i for cell in result for i in cell}!=set(range(n)):raise ValueError('A measurement was omitted from the mesh')
    return result

def evaluate(inputs,outputs,cells,rgb):
    for i,point in enumerate(inputs):
        if tuple(point)==tuple(rgb):return list(outputs[i])
    for a,b,c,d in cells:
        ba,ca,da,x=(sub(p,inputs[a]) for p in (inputs[b],inputs[c],inputs[d],rgb))
        denominator=det(ba,ca,da)
        weights=[det(x,ca,da),det(ba,x,da),det(ba,ca,x)]
        weights.insert(0,denominator-sum(weights))
        if denominator<0:denominator=-denominator;weights=[-w for w in weights]
        if min(weights)<0:continue
        return [(sum(w*outputs[i][channel] for w,i in zip(weights,(a,b,c,d)))+denominator//2)//denominator for channel in range(3)]
    raise ValueError(f'RGB cube has an uncovered point: {rgb}')

def fit(data):
    samples=data.get('samples',[])
    if not data.get('complete') or len(samples)!=20 or any(not s for s in samples):raise ValueError('All 20 confirmed samples are required')
    inputs=[];outputs=[]
    for sample in samples:
        x,y=sample['screen'],sample['led']
        if any(len(v)!=3 or any(type(c) is not int or not 0<=c<=255 for c in v) for v in (x,y)):raise ValueError('Invalid RGB triplet')
        if x in inputs:raise ValueError('Duplicate reference color')
        inputs.append(x);outputs.append(y)
    corners={tuple(p) for p in itertools.product((0,255),repeat=3)}
    if not corners.issubset(map(tuple,inputs)):raise ValueError('All RGB cube corners are required')
    if outputs[inputs.index([0,0,0])]!=[0,0,0]:raise ValueError('Black must stay unlit')
    cells=triangulate(inputs)
    # Check actual integer interpolation, including faces, edges and interior.
    levels=list(range(0,256,16))+[255]
    for rgb in itertools.product(levels,repeat=3):
        value=evaluate(inputs,outputs,cells,rgb)
        if any(not 0<=v<=255 for v in value):raise ValueError('Unbounded model')
    errors=[max(abs(a-b) for a,b in zip(evaluate(inputs,outputs,cells,x),y)) for x,y in zip(inputs,outputs)]
    payload=MAGIC+bytes([len(inputs),len(cells),0])
    payload+=b''.join(bytes(x+y) for x,y in zip(inputs,outputs))
    payload+=b''.join(bytes(cell) for cell in cells)
    return {'version':1,'kind':'tetrahedral-rgb','vertices':inputs,'led_vertices':outputs,
            'tetrahedra':[list(t) for t in cells],'hex':payload.hex(),'vertex_max_error':max(errors),
            'coverage_points':len(levels)**3,'created_at':time.time(),
            'limits':'Manual screen match at the recorded brightness and display settings; no optical instrument measurement.'}

def apply_profile(model,name='panel'):
    chunks=[model['hex'][i:i+256] for i in range(0,len(model['hex']),256)]
    command=['keyboard','color-profile','set',name,*chunks]
    if len(command)>12:raise ValueError('Profile exceeds broker request limit')
    white=model['led_vertices'][model['vertices'].index([255,255,255])]
    subprocess.run(['/usr/bin/busctl','--system','call','org.gamegaraj.Slayer1','/org/gamegaraj/Slayer1','org.gamegaraj.Slayer1','Apply','asas',str(len(command)),*command,'4','keyboard_color_profile',name,'keyboard_white_point',','.join(map(str,white))],check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--apply',action='store_true');parser.add_argument('--name',default='panel')
    args=parser.parse_args()
    model=fit(json.loads(args.input.read_text()))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    temporary=args.output.with_suffix(args.output.suffix+'.tmp')
    temporary.write_text(json.dumps(model,indent=2)+'\n');temporary.replace(args.output)
    ramp=[evaluate(model['vertices'],model['led_vertices'],model['tetrahedra'],[v,v,v]) for v in range(256)]
    args.output.with_suffix('.csv').write_text('input,red,green,blue\n'+''.join(str(v)+','+','.join(map(str,rgb))+'\n' for v,rgb in enumerate(ramp)))
    print(f"20 samples, {len(model['tetrahedra'])} tetrahedra, {model['coverage_points']} coverage checks, vertex error {model['vertex_max_error']}",flush=True)
    if args.apply:apply_profile(model,args.name)

if __name__=='__main__':main()
