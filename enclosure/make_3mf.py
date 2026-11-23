#!/usr/bin/env python3
"""Lays out every part of one revision on a single 180 x 180 mm plate (Bambu
A1 mini) and writes a plain 3MF. Two-colour pieces (faceplate + its A/B
letters, each badge + its letters) are one object with two parts, so the
letters part can be given a second filament in the slicer.

Usage: ./make_3mf.py <rev number> <description used by export.sh>
       ./make_3mf.py 32 two-colour-faceplate-letters
"""
import os
import sys
import zipfile

import numpy as np

rev = "r%02d" % int(sys.argv[1])
desc = sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
stl = os.path.join(here, "stl")


def f(part, d=desc):
    return "%s_%s_%s.stl" % (part, rev, d)


# name, files (first = main part, rest = second-colour parts), min corner (x, y)
PARTS = [
    ("Body", [f("body")], (8, 8)),
    ("Faceplate (letters = filament 2)", [f("faceplate"), f("faceplate_letters")], (8, 52)),
    ("Lid", [f("lid")], (8, 96)),
    ("Separator", [f("separator")], (8, 140)),
    ("Buttons standard + reset", [f("buttons")], (90, 8)),
    ("Buttons longer 0.4", [f("buttons", "variant-longer-0.4mm")], (90, 24)),
    ("Buttons shorter 0.4", [f("buttons", "variant-shorter-0.4mm")], (90, 40)),
    ("Badge 1 (letters = filament 2)", [f("badge_plate"), f("badge_letters")], (90, 60)),
    ("Badge 2 (letters = filament 2)", [f("badge_plate"), f("badge_letters")], (90, 74)),
]


def load(name):
    v = []
    for line in open(os.path.join(stl, name)):
        line = line.strip()
        if line.startswith("vertex"):
            v.append([float(x) for x in line.split()[1:4]])
    v = np.array(v)
    uniq, inv = np.unique(np.round(v, 5), axis=0, return_inverse=True)
    return uniq, inv.reshape(-1, 3)


objects, build, oid = [], [], 1
for name, files, (tx, ty) in PARTS:
    meshes = [load(x) for x in files]
    allv = np.vstack([m[0] for m in meshes])
    lo, hi = allv.min(0), allv.max(0)
    assert tx + hi[0] - lo[0] <= 180 and ty + hi[1] - lo[1] <= 180, name + " is off the plate"
    off = np.array([tx - lo[0], ty - lo[1], -lo[2]])  # same offset for all parts: they stay aligned
    ids = []
    for k, (v, t) in enumerate(meshes):
        v = v + off
        vs = "".join('<vertex x="%.4f" y="%.4f" z="%.4f"/>' % tuple(p) for p in v)
        ts = "".join('<triangle v1="%d" v2="%d" v3="%d"/>' % tuple(p) for p in t)
        pname = name if len(files) == 1 else name.split(" (")[0] + (" letters" if k else "")
        objects.append('<object id="%d" name="%s" type="model"><mesh><vertices>%s</vertices>'
                       "<triangles>%s</triangles></mesh></object>" % (oid, pname, vs, ts))
        ids.append(oid)
        oid += 1
    if len(ids) == 1:
        build.append(ids[0])
    else:
        comps = "".join('<component objectid="%d"/>' % i for i in ids)
        objects.append('<object id="%d" name="%s" type="model"><components>%s</components></object>'
                       % (oid, name, comps))
        build.append(oid)
        oid += 1

model = ('<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" '
         'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
         '<metadata name="Title">BattleBuddy case %s</metadata><resources>%s</resources><build>%s</build></model>'
         % (rev, "".join(objects), "".join('<item objectid="%d"/>' % i for i in build)))
types = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/'
         'content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.'
         'relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.'
         '3dmanufacturing-3dmodel+xml"/></Types>')
rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/'
        'package/2006/relationships"><Relationship Target="/3D/3dmodel.model" Id="rel0" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
os.makedirs(os.path.join(here, "print"), exist_ok=True)
out = os.path.join(here, "print", "battlebuddy_%s_all-parts_one-plate.3mf" % rev)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", types)
    z.writestr("_rels/.rels", rels)
    z.writestr("3D/3dmodel.model", model)
print("wrote", os.path.relpath(out, here))
