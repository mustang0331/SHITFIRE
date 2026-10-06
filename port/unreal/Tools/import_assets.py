"""Imports every asset in asset_manifest.json and regenerates the textured
terrain/road materials. BUILDER-ASSETS' counterpart to gen_assets.py: the
.uassets under /Game/Textures, /Game/Vegetation, /Game/Models plus
M_Terrain/M_Road are build products of THIS script + the manifest; delete
and re-run to regenerate (cache first via download_assets.py).

M_Terrain and M_Road keep gen_assets.py's parameter names exactly (its
docstring invites regenerating them with texture-sampling graphs), so no
C++ changes: the color params become per-layer tints (new default white),
layer/slope/height params keep their names and defaults, and texture +
tiling params are added on top. Run order when both scripts run:
gen_assets.py first, THEN import_assets.py (each fully regenerates the
materials it owns; import_assets.py's M_Terrain/M_Road are the shipped
ones, gen_assets.py's remain the no-content fallback).

Textures: albedo sRGB on; normal maps TC_Normalmap, sRGB off (ambientCG
NormalGL flavor; the custom nodes do their own rg*2-1 unpack, and macro
shading dominates at the 1,500-3,200 m mission ranges).

Vegetation meshes get script-generated materials (MI_* of M_VegBase) with
used_with_instanced_static_meshes baked in - imported glTF materials
inherit from engine Interchange parents whose usage flags we cannot edit,
which silently breaks HISM rendering in -game (same trap gen_assets.py
documents for M_Placeholder). Unit models under /Game/Models keep their
imported materials (not instanced yet; BUILDER-CHARACTERS wires them).

Run headless (asset cache must exist - run download_assets.py first):
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/import_assets.py" -unattended -nopause
"""
import json
import math
import os
import struct

import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
HERE = os.path.dirname(os.path.abspath(__file__))

WHITE_TEX_CANDIDATES = [
    "/Engine/EngineResources/WhiteSquareTexture",
    "/Engine/EngineResources/DefaultTexture",
]


def log(msg):
    unreal.log("IMPORT_ASSETS: " + msg)


def cache_root():
    root = os.environ.get("SHITFIRE_ASSET_CACHE")
    if not root:
        base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        root = os.path.join(base, "shitfire-asset-cache")
    if not os.path.isdir(root):
        raise RuntimeError("asset cache missing at %s - run download_assets.py" % root)
    return root


def import_file(filename, dest_path, dest_name=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest_path
    if dest_name:
        task.destination_name = dest_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    return list(task.imported_object_paths)


def loaded_assets_under(path):
    out = []
    for op in EAL.list_assets(path, recursive=True, include_folder=False):
        a = EAL.load_asset(op)
        if a:
            out.append((op, a))
    return out


# ------------------------------------------------------------------ textures
def import_textures(man, cache):
    tex = {}
    for t in man["textures"]:
        exdir = os.path.join(cache, "textures", t["id"])
        for kind, fname, srgb in [
            ("Alb", t["albedo"], True),
            ("Nrm", t["normal_gl"], False),
        ]:
            src = os.path.join(exdir, fname)
            if not os.path.exists(src):
                raise RuntimeError("missing " + src)
            name = "%s_%s" % (t["prefix"], kind)
            full = "%s/%s" % (t["dest"], name)
            if EAL.does_asset_exist(full):
                EAL.delete_asset(full)
            import_file(src, t["dest"], name)
            tx = EAL.load_asset(full)
            if not tx:
                raise RuntimeError("texture import failed: " + full)
            tx.set_editor_property("srgb", srgb)
            if kind == "Nrm":
                tx.set_editor_property(
                    "compression_settings",
                    unreal.TextureCompressionSettings.TC_NORMALMAP)
                tx.set_editor_property("flip_green_channel", False)
            EAL.save_loaded_asset(tx)
            tex[name] = tx
            log("texture %s <- %s" % (full, fname))
    return tex


# ------------------------------------------ FBX2glTF armature bake (soldiers)
# The Quaternius soldier GLBs (FBX2glTF 0.9.7) carry an armature node with
# scale 100 + rot -90X, a skinned mesh node with its own transform M (rot
# -90X, scale 58.6) and inverse bind matrices IBM = inverse(G_bind) * M, so
# that spec renderers (which ignore M) are right. UE Interchange takes
# inverse(IBM) as the bind pose and bakes M into the vertex buffer, so the
# import came out 136x too large, lying along -Y, with a Root bone scale of
# 170.6 (2026-10-06 bench frame; FINDINGS in units/ShitfireUnitAssets.h). No
# pipeline option fixes a vertex/bone mismatch, so these two models are
# re-written as plain metre-scale glTF before import: M baked into the
# geometry, the armature folded into the root joint, its scale pushed into
# every descendant translation, IBM' = S * IBM * M^-1 (scale-free), and the
# animation translation keys scaled to match. The skinning result is
# unchanged (verified to 4e-7 m with the glTF skinning formula); the cache
# file is untouched, the baked copy lands in Saved/BakedGlb (git-ignored).
BAKE_ARMATURE_MODELS = {"SM_Soldier", "SM_SoldierSwat"}
_F32 = 5126


def _identity():
    return [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]


def _mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def _mat_from_col_major(v):
    return [[v[c * 4 + r] for c in range(4)] for r in range(4)]


def _mat_to_col_major(m):
    return [m[r][c] for c in range(4) for r in range(4)]


def _quat_to_mat(q):
    x, y, z, w = q
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0.0],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0.0],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def _trs_to_mat(t, r, s):
    m = _quat_to_mat(r)
    for i in range(3):
        for j in range(3):
            m[i][j] *= s[j]
        m[i][3] = t[i]
    return m


def _node_local(n):
    if "matrix" in n:
        return _mat_from_col_major(n["matrix"])
    return _trs_to_mat(n.get("translation", [0, 0, 0]),
                       n.get("rotation", [0, 0, 0, 1]),
                       n.get("scale", [1, 1, 1]))


def _mat_inverse(m):
    a = [row[:] + [1.0 if i == j else 0.0 for j in range(4)] for i, row in enumerate(m)]
    for c in range(4):
        p = max(range(c, 4), key=lambda r: abs(a[r][c]))
        a[c], a[p] = a[p], a[c]
        piv = a[c][c]
        if abs(piv) < 1e-18:
            raise ValueError("singular matrix")
        a[c] = [v / piv for v in a[c]]
        for r in range(4):
            if r != c and a[r][c] != 0.0:
                f = a[r][c]
                a[r] = [rv - f * cv for rv, cv in zip(a[r], a[c])]
    return [row[4:] for row in a]


def _apply_point(m, p):
    return [m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3)]


def _apply_dir(m, d):
    return [m[i][0] * d[0] + m[i][1] * d[1] + m[i][2] * d[2] for i in range(3)]


def _normalize(v):
    n = math.sqrt(sum(c * c for c in v))
    return [c / n for c in v] if n > 0 else v


def _quat_mul(a, b):
    """Hamilton product a*b (apply b first, then a); glTF (x,y,z,w)."""
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz]


def _mat_to_quat(m):
    tr = m[0][0] + m[1][1] + m[2][2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        return [(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, 0.25 * s]
    if m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2
        return [0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s]
    if m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2
        return [(m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s]
    s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2
    return [(m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s, (m[1][0] - m[0][1]) / s]


def _decompose(m):
    """Affine -> (t, q, s); assumes no shear or mirroring."""
    t = [m[0][3], m[1][3], m[2][3]]
    s = [math.sqrt(sum(m[i][j] ** 2 for i in range(3))) for j in range(3)]
    r = [[m[i][j] / s[j] for j in range(3)] for i in range(3)]
    det = (r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1])
           - r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0])
           + r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]))
    if det < 0:
        raise ValueError("negative determinant (mirroring) not supported")
    return t, _normalize(_mat_to_quat(r)), s


def _read_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, _ver, length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ValueError(path + " is not a GLB")
    off, chunks = 12, []
    while off < length:
        clen, ctype = struct.unpack_from("<II", data, off)
        chunks.append((ctype, bytearray(data[off + 8:off + 8 + clen])))
        off += 8 + clen
    if chunks[0][0] != 0x4E4F534A or chunks[1][0] != 0x004E4942:
        raise ValueError(path + " unexpected chunk layout")
    return json.loads(bytes(chunks[0][1]).decode("utf-8")), chunks[1][1]


def _write_glb(path, g, binbuf):
    bb = bytes(binbuf) + b"\0" * ((4 - len(binbuf) % 4) % 4)
    g["buffers"][0]["byteLength"] = len(bb)
    js = json.dumps(g, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(bb)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A))
        f.write(js)
        f.write(struct.pack("<II", len(bb), 0x004E4942))
        f.write(bb)


class _Accessors:
    """Read/write float accessors in place (tightly packed float32 only)."""
    NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

    def __init__(self, g, binbuf):
        self.g, self.bin = g, binbuf
        self.touched = {}

    def _layout(self, idx):
        a = self.g["accessors"][idx]
        if a["componentType"] != _F32 or "sparse" in a:
            raise ValueError("accessor %d is not plain float32" % idx)
        bv = self.g["bufferViews"][a["bufferView"]]
        n = self.NCOMP[a["type"]]
        stride = bv.get("byteStride", n * 4)
        if stride != n * 4:
            raise ValueError("accessor %d is interleaved (stride %d)" % (idx, stride))
        return bv.get("byteOffset", 0) + a.get("byteOffset", 0), n, a["count"]

    def read(self, idx):
        off, n, cnt = self._layout(idx)
        flat = struct.unpack_from("<%df" % (n * cnt), self.bin, off)
        return [list(flat[i * n:(i + 1) * n]) for i in range(cnt)]

    def write(self, idx, rows, tag):
        prev = self.touched.get(idx)
        if prev is not None:
            if prev == tag:
                return  # same transform already applied (shared accessor)
            raise ValueError("accessor %d needs two different transforms (%s, %s)" % (idx, prev, tag))
        self.touched[idx] = tag
        off, n, cnt = self._layout(idx)
        if len(rows) != cnt:
            raise ValueError("row count mismatch on accessor %d" % idx)
        flat = [c for r in rows for c in r]
        struct.pack_into("<%df" % (n * cnt), self.bin, off, *flat)
        a = self.g["accessors"][idx]
        if "min" in a or "max" in a:
            a["min"] = [min(r[i] for r in rows) for i in range(n)]
            a["max"] = [max(r[i] for r in rows) for i in range(n)]


def bake_fbx2gltf_armature(src, dst):
    """Rewrite an FBX2glTF armature-scale GLB as plain metre-scale glTF
    (see the section comment). Raises on anything it was not written for."""
    g, binbuf = _read_glb(src)
    nodes = g["nodes"]
    acc = _Accessors(g, binbuf)
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i

    def global_mat(i):
        m = _node_local(nodes[i])
        while i in parent:
            i = parent[i]
            m = _mat_mul(_node_local(nodes[i]), m)
        return m

    skins = g.get("skins", [])
    if not skins:
        raise ValueError("no skins in " + src)
    joint_set = set(j for s in skins for j in s["joints"])

    def root_joint_of(skin):
        r = skin.get("skeleton", skin["joints"][0])
        while r in parent and parent[r] in joint_set:
            r = parent[r]
        return r

    roots = set(root_joint_of(s) for s in skins)
    if len(roots) != 1:
        raise ValueError("skins have different root joints %s" % roots)
    root = roots.pop()

    # ancestors of the root joint form a chain whose only other children are
    # skinned mesh nodes (those become identity anyway)
    anc = []
    a = root
    while a in parent:
        child = a
        a = parent[a]
        anc.append(a)
        if nodes[a].get("mesh") is not None or nodes[a].get("skin") is not None:
            raise ValueError("ancestor %s of root joint carries a mesh" % nodes[a].get("name"))
        for c in nodes[a].get("children", []):
            if c != child and "skin" not in nodes[c]:
                raise ValueError("ancestor %s has a non-skinned sibling %s" % (
                    nodes[a].get("name"), nodes[c].get("name")))
    P = _identity()
    for ai in reversed(anc):
        P = _mat_mul(P, _node_local(nodes[ai]))
    tP, qP, sP = _decompose(P)
    if max(sP) - min(sP) > 1e-6 * max(sP):
        raise ValueError("ancestor scale not uniform: %s" % sP)
    s = sP[0]

    skinned = [i for i, n in enumerate(nodes) if "skin" in n]
    old_M = {i: global_mat(i) for i in skinned}

    # 1. fold P into the root joint, drop its (uniform) scale
    t0, q0, s0 = _decompose(_mat_mul(P, _node_local(nodes[root])))
    if max(abs(v - s) for v in s0) > 1e-6 * s:
        raise ValueError("root joint carries its own scale %s" % s0)
    nodes[root].pop("matrix", None)
    nodes[root]["translation"], nodes[root]["rotation"], nodes[root]["scale"] = t0, q0, [1.0, 1.0, 1.0]
    for ai in anc:
        for k in ("translation", "rotation", "scale", "matrix"):
            nodes[ai].pop(k, None)

    # 2. push the scale into every descendant translation; prop nodes
    #    (static meshes hung off joints) also take it on their own scale so
    #    their world placement is unchanged
    def push(i):
        for c in nodes[i].get("children", []):
            n = nodes[c]
            if "matrix" in n:
                raise ValueError("node %s uses a matrix" % n.get("name"))
            n["translation"] = [v * s for v in n.get("translation", [0, 0, 0])]
            if n.get("mesh") is not None and "skin" not in n:
                n["scale"] = [v * s for v in n.get("scale", [1, 1, 1])]
                if n.get("children"):
                    raise ValueError("prop node %s has children" % n.get("name"))
                continue
            push(c)
    push(root)

    # 3. skinned mesh nodes: bake M into the geometry, node -> identity
    S_s = _trs_to_mat([0, 0, 0], [0, 0, 0, 1], [s, s, s])
    mesh_users = {}
    for i in skinned:
        mesh_users.setdefault(nodes[i]["mesh"], []).append(i)

    def same(a, b):
        return max(abs(x - y) for ra, rb in zip(a, b) for x, y in zip(ra, rb)) <= 1e-6

    for mi, users in mesh_users.items():
        M = old_M[users[0]]
        tag = "M%d" % users[0]
        lin = [[M[i][j] for j in range(3)] + [0.0] for i in range(3)] + [[0, 0, 0, 1.0]]
        inv_t = _mat_inverse(lin)
        inv_t = [[inv_t[j][i] for j in range(4)] for i in range(4)]  # inverse transpose
        for prim in g["meshes"][mi]["primitives"]:
            at = prim["attributes"]
            acc.write(at["POSITION"], [_apply_point(M, p) for p in acc.read(at["POSITION"])], tag)
            if "NORMAL" in at:
                acc.write(at["NORMAL"], [_normalize(_apply_dir(inv_t, d)) for d in acc.read(at["NORMAL"])], tag)
            if "TANGENT" in at:
                acc.write(at["TANGENT"], [_normalize(_apply_dir(M, d[:3])) + [d[3]]
                                          for d in acc.read(at["TANGENT"])], tag)
            if "targets" in prim:
                raise ValueError("morph targets not handled (mesh %d)" % mi)
        for u in users:
            if not same(old_M[u], M):
                raise ValueError("mesh %d used by nodes with different transforms" % mi)
            for k in ("translation", "rotation", "scale", "matrix"):
                nodes[u].pop(k, None)

    # 4. inverse bind matrices: IBM' = S * IBM * M^-1
    for si, skin in enumerate(skins):
        users = [i for i in skinned if nodes[i]["skin"] == si]
        if not users:
            raise ValueError("skin %d unused" % si)
        for u in users[1:]:
            if not same(old_M[u], old_M[users[0]]):
                raise ValueError("skin %d used by nodes with different transforms" % si)
        Minv = _mat_inverse(old_M[users[0]])
        rows = acc.read(skin["inverseBindMatrices"])
        acc.write(skin["inverseBindMatrices"],
                  [_mat_to_col_major(_mat_mul(_mat_mul(S_s, _mat_from_col_major(r)), Minv)) for r in rows],
                  "IBM%d" % users[0])

    # 5. animations: translations scale with the joints; the root joint's
    #    keys get P applied
    for an in g.get("animations", []):
        for ch in an["channels"]:
            tgt, path = ch["target"]["node"], ch["target"]["path"]
            out = an["samplers"][ch["sampler"]]["output"]
            if path == "translation":
                if tgt == root:
                    acc.write(out, [_apply_point(P, v) for v in acc.read(out)], "rootT")
                else:
                    acc.write(out, [[c * s for c in v] for v in acc.read(out)], "scale")
            elif path == "rotation" and tgt == root:
                acc.write(out, [_normalize(_quat_mul(qP, q)) for q in acc.read(out)], "rootR")
            elif path == "scale" and tgt == root:
                raise ValueError("root joint has scale animation")

    # 6. the result must be scale-free, with the root joint bound where it rests
    worst_root, worst_scale = 0.0, 0.0
    for skin in skins:
        for j, ibm in zip(skin["joints"], acc.read(skin["inverseBindMatrices"])):
            _t, _q, sc = _decompose(_mat_inverse(_mat_from_col_major(ibm)))
            worst_scale = max(worst_scale, max(abs(v - 1.0) for v in sc))
            if j == root:
                prod = _mat_mul(global_mat(j), _mat_from_col_major(ibm))
                worst_root = max(worst_root, max(abs(prod[i][k] - (1.0 if i == k else 0.0))
                                                 for i in range(4) for k in range(4)))
    if worst_root > 1e-3 or worst_scale > 1e-3:
        raise ValueError("bake left scale %.2e / root offset %.2e in the bind pose" % (worst_scale, worst_root))
    _write_glb(dst, g, binbuf)
    log("baked armature %s -> %s (ancestor scale %.1f, root |G*IBM-I| %.1e)" % (
        os.path.basename(src), dst, s, worst_root))


def baked_glb_dir():
    d = os.path.normpath(os.path.join(HERE, "..", "Saved", "BakedGlb"))
    if not os.path.isdir(d):
        os.makedirs(d)
    return d


# -------------------------------------------------------------------- models
def glb_material_info(path):
    """Names + base colors of the materials inside a .glb (JSON chunk 0)."""
    with open(path, "rb") as f:
        data = f.read()
    magic = data[0:4]
    if magic != b"glTF":
        raise RuntimeError(path + " is not a GLB")
    clen, ctype = struct.unpack_from("<I4s", data, 12)
    if ctype != b"JSON":
        raise RuntimeError(path + " unexpected first chunk " + repr(ctype))
    g = json.loads(data[20:20 + clen].decode("utf-8"))
    mats = []
    for m in g.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        mats.append({
            "name": m.get("name", ""),
            "factor": pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0]),
            "has_tex": "baseColorTexture" in pbr,
        })
    return mats


def ensure_veg_base():
    """M_VegBase: BaseTex * Tint, rough, ISM+Nanite usage baked in."""
    full = "/Game/Vegetation/M_VegBase"
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = TOOLS.create_asset("M_VegBase", "/Game/Vegetation",
                             unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mat.set_editor_property("used_with_nanite", True)
    except Exception as e:  # noqa: BLE001
        log("note: used_with_nanite not settable (%s)" % e)
    white = None
    for cand in WHITE_TEX_CANDIDATES:
        if EAL.does_asset_exist(cand):
            white = EAL.load_asset(cand)
            break
    ts = MEL.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    ts.set_editor_property("parameter_name", "BaseTex")
    if white:
        ts.set_editor_property("texture", white)
    else:
        log("WARNING: no engine white texture found; BaseTex default unset")
    tint = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -700, 300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(
        mat, unreal.MaterialExpressionMultiply, -350, 100)
    MEL.connect_material_expressions(ts, "RGB", mul, "A")
    MEL.connect_material_expressions(tint, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -700, 500)
    rough.set_editor_property("parameter_name", "VegRoughness")
    rough.set_editor_property("default_value", 0.85)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log("built M_VegBase")
    return mat


def pick_mesh(assets):
    """Largest static mesh among imported assets (else the skeletal mesh)."""
    best, best_vol = None, -1.0
    for op, a in assets:
        if isinstance(a, unreal.StaticMesh):
            try:
                size = a.get_bounding_box().get_size()
                vol = size.x * size.y * size.z
            except Exception:  # noqa: BLE001
                vol = 0.0
            if vol > best_vol:
                best, best_vol = op, vol
    if best:
        return best, "static"
    for op, a in assets:
        if isinstance(a, unreal.SkeletalMesh):
            return op, "skeletal"
    return None, None


def import_models(man, cache, veg_base):
    for m in man["models"]:
        src = os.path.join(cache, "models", m["name"] + ".glb")
        if not os.path.exists(src):
            raise RuntimeError("missing " + src)
        keep = m.get("keep_imported_materials", False)
        dest = m["dest"]
        imp_dir = dest if keep else dest + "/_imp_" + m["name"]
        final = "%s/%s" % (dest, m["name"])
        # regenerate from scratch each run
        if EAL.does_asset_exist(final):
            EAL.delete_asset(final)
        if not keep and EAL.does_directory_exist(imp_dir):
            EAL.delete_directory(imp_dir)
        if keep and EAL.does_directory_exist(imp_dir):
            EAL.delete_directory(imp_dir)
        import_src = src
        if m["name"] in BAKE_ARMATURE_MODELS:
            # same basename: Interchange names the animation assets after it
            import_src = os.path.join(baked_glb_dir(), os.path.basename(src))
            bake_fbx2gltf_armature(src, import_src)
        import_file(import_src, imp_dir)
        assets = loaded_assets_under(imp_dir)
        mesh_op, mesh_kind = pick_mesh(assets)
        if not mesh_op:
            log("WARNING: no mesh found in %s - skipped" % src)
            continue
        if not EAL.rename_asset(mesh_op.split(".")[0], final):
            raise RuntimeError("rename failed %s -> %s" % (mesh_op, final))
        mesh = EAL.load_asset(final)

        if not keep and mesh_kind == "static":
            # Script-generated materials (HISM/Nanite usage flags), palette
            # texture kept, imported Interchange materials dropped.
            ginfo = glb_material_info(src)
            by_name = {mi["name"]: mi for mi in ginfo if mi["name"]}
            palette = None
            for op, a in loaded_assets_under(imp_dir):
                if isinstance(a, unreal.Texture2D):
                    pal_path = "%s/T_%s_Palette" % (dest, m["name"])
                    if EAL.does_asset_exist(pal_path):
                        EAL.delete_asset(pal_path)
                    if EAL.rename_asset(op.split(".")[0], pal_path):
                        palette = EAL.load_asset(pal_path)
                    break
            tint_over = m.get("tint")
            n_slots = mesh.get_num_sections(0)
            for i in range(n_slots):
                slot_name = ""
                try:
                    mats_list = mesh.get_editor_property("static_materials")
                    slot_name = str(mats_list[i].material_slot_name)
                except Exception:  # noqa: BLE001
                    pass
                gi = by_name.get(slot_name)
                if gi is None and len(ginfo) == 1:
                    gi = ginfo[0]
                factor = (gi or {}).get("factor", [1, 1, 1, 1])
                has_tex = (gi or {}).get("has_tex", palette is not None)
                mi_name = "MI_%s_%d" % (m["name"], i)
                mi_full = "%s/%s" % (dest, mi_name)
                if EAL.does_asset_exist(mi_full):
                    EAL.delete_asset(mi_full)
                mic = TOOLS.create_asset(
                    mi_name, dest, unreal.MaterialInstanceConstant,
                    unreal.MaterialInstanceConstantFactoryNew())
                MEL.set_material_instance_parent(mic, veg_base)
                col = [factor[0], factor[1], factor[2]]
                if tint_over:
                    col = [col[0] * tint_over[0], col[1] * tint_over[1],
                           col[2] * tint_over[2]]
                MEL.set_material_instance_vector_parameter_value(
                    mic, "Tint", unreal.LinearColor(col[0], col[1], col[2], 1.0))
                if has_tex and palette:
                    MEL.set_material_instance_texture_parameter_value(
                        mic, "BaseTex", palette)
                MEL.update_material_instance(mic)
                EAL.save_loaded_asset(mic)
                mesh.set_material(i, mic)
            # drop the imported material/texture leftovers
            EAL.delete_directory(imp_dir)

        if m.get("nanite") and mesh_kind == "static":
            try:
                ns = mesh.get_editor_property("nanite_settings")
                ns.set_editor_property("enabled", True)
                mesh.set_editor_property("nanite_settings", ns)
            except Exception as e:  # noqa: BLE001
                log("note: nanite not set on %s (%s)" % (m["name"], e))
        EAL.save_loaded_asset(mesh)
        log("model %s (%s)" % (final, mesh_kind))


# ---------------------------------------------------- terrain/road materials
def fresh_material(name):
    full = "/Game/Materials/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = TOOLS.create_asset(name, "/Game/Materials",
                             unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("create_asset failed for " + full)
    return mat


def scalar_param(mat, name, value, x=-1500, y=0):
    e = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector_param(mat, name, rgb, x=-1500, y=0):
    e = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value",
                          unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def texture_param(mat, name, tex, x=-1500, y=0):
    e = MEL.create_material_expression(
        mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", tex)
    return e


def custom_node(mat, code, input_names, out_type, x=-500, y=0):
    e = MEL.create_material_expression(
        mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", out_type)
    ins = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


def finish(mat):
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    if not EAL.save_loaded_asset(mat):
        raise RuntimeError("save failed for " + mat.get_name())
    log("wrote " + mat.get_path_name())


# Dual-scale sample macro: detail tile + decorrelated macro tile beats
# visible repetition at the 1,500-3,200 m mission ranges.
def dual(tex, tile):
    return ("lerp(Texture2DSample({t}, {t}Sampler, uv / max({s}, 0.01)).rgb, "
            "Texture2DSample({t}, {t}Sampler, uv / (max({s}, 0.01) * mm)).rgb, mb)"
            ).format(t=tex, s=tile)


TERRAIN_WEIGHTS = """
float3 wp = WP.xyz / 100.0;
float slope = saturate(1.0 - N.z);
float2 uv = wp.xy;
float mm = max(MacroTileMul, 1.5);
float mb = saturate(MacroBlend);
float wBS = saturate(wp.z / max(BeachTopM, 0.01));
float wSc = saturate((wp.z - ScrubStartM) / max(ScrubFeatherM, 0.01)) * (1.0 - slope);
float wRk = saturate((slope - SlopeRockStart) / max(SlopeRockFeather, 0.001));
"""

TERRAIN_COLOR_HLSL = TERRAIN_WEIGHTS + """
float3 beach = {beach} * BeachColor.rgb;
float3 sand  = {sand} * SandColor.rgb;
float3 scrub = {scrub} * ScrubColor.rgb;
float3 rock  = {rock} * RockColor.rgb;
float3 c = lerp(beach, sand, wBS);
c = lerp(c, scrub, wSc);
c = lerp(c, rock, wRk);
// gen_assets.py's 2-tap value noise, kept for macro brightness breakup
float2 p = wp.xy / max(NoiseScaleM, 0.001);
float2 ip = floor(p);
float2 fp = frac(p);
fp = fp * fp * (3.0 - 2.0 * fp);
float h00 = frac(sin(dot(ip,                float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float n = lerp(lerp(h00, h10, fp.x), lerp(h01, h11, fp.x), fp.y);
c *= (1.0 - NoiseAmp) + 2.0 * NoiseAmp * n;
return c;
""".format(beach=dual("TBeachAlb", "BeachTileM"), sand=dual("TSandAlb", "SandTileM"),
           scrub=dual("TScrubAlb", "ScrubTileM"), rock=dual("TRockAlb", "RockTileM"))

# World-space output (tangent_space_normal off): the RMC terrain mesh's
# tangent basis is not trusted; vertex normal + planar-projected map detail.
# NormalGL rg*2-1 unpack; green-channel orientation [assumed] - at mission
# ranges an inverted bump is invisible, flip NormalStrength's sign if close
# inspection ever says otherwise.
TERRAIN_NORMAL_HLSL = TERRAIN_WEIGHTS + """
float2 nb = Texture2DSample(TBeachNrm, TBeachNrmSampler, uv / max(BeachTileM, 0.01)).rg * 2.0 - 1.0;
float2 ns = Texture2DSample(TSandNrm,  TSandNrmSampler,  uv / max(SandTileM,  0.01)).rg * 2.0 - 1.0;
float2 nc = Texture2DSample(TScrubNrm, TScrubNrmSampler, uv / max(ScrubTileM, 0.01)).rg * 2.0 - 1.0;
float2 nr = Texture2DSample(TRockNrm,  TRockNrmSampler,  uv / max(RockTileM,  0.01)).rg * 2.0 - 1.0;
float2 nxy = lerp(nb, ns, wBS);
nxy = lerp(nxy, nc, wSc);
nxy = lerp(nxy, nr, wRk);
return normalize(float3(N.xy + nxy * NormalStrength, max(N.z, 0.05)));
"""


def build_terrain_material(tex):
    mat = fresh_material("M_Terrain")
    mat.set_editor_property("tangent_space_normal", False)
    wp = MEL.create_material_expression(
        mat, unreal.MaterialExpressionWorldPosition, -1900, 0)
    nrm = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVertexNormalWS, -1900, 200)

    y = [0]

    def nexty():
        y[0] += 150
        return y[0]

    # gen_assets.py param names preserved; colors are now layer TINTS.
    params = [
        ("RockColor", vector_param(mat, "RockColor", (1.0, 1.0, 1.0), -1500, nexty())),
        ("ScrubColor", vector_param(mat, "ScrubColor", (1.0, 1.0, 1.0), -1500, nexty())),
        ("SandColor", vector_param(mat, "SandColor", (1.0, 1.0, 1.0), -1500, nexty())),
        ("BeachColor", vector_param(mat, "BeachColor", (1.0, 1.0, 1.0), -1500, nexty())),
        ("SlopeRockStart", scalar_param(mat, "SlopeRockStart", 0.13, -1500, nexty())),
        ("SlopeRockFeather", scalar_param(mat, "SlopeRockFeather", 0.10, -1500, nexty())),
        ("ScrubStartM", scalar_param(mat, "ScrubStartM", 5.0, -1500, nexty())),
        ("ScrubFeatherM", scalar_param(mat, "ScrubFeatherM", 28.0, -1500, nexty())),
        ("BeachTopM", scalar_param(mat, "BeachTopM", 2.5, -1500, nexty())),
        ("NoiseScaleM", scalar_param(mat, "NoiseScaleM", 61.0, -1500, nexty())),
        ("NoiseAmp", scalar_param(mat, "NoiseAmp", 0.12, -1500, nexty())),
        # new texture-era params
        ("RockTileM", scalar_param(mat, "RockTileM", 6.1, -1500, nexty())),
        ("ScrubTileM", scalar_param(mat, "ScrubTileM", 7.3, -1500, nexty())),
        ("SandTileM", scalar_param(mat, "SandTileM", 5.9, -1500, nexty())),
        ("BeachTileM", scalar_param(mat, "BeachTileM", 4.7, -1500, nexty())),
        ("MacroTileMul", scalar_param(mat, "MacroTileMul", 17.0, -1500, nexty())),
        ("MacroBlend", scalar_param(mat, "MacroBlend", 0.35, -1500, nexty())),
    ]
    texes = [
        ("TRockAlb", texture_param(mat, "TRockAlb", tex["T_Rock_Alb"], -1500, nexty())),
        ("TScrubAlb", texture_param(mat, "TScrubAlb", tex["T_Scrub_Alb"], -1500, nexty())),
        ("TSandAlb", texture_param(mat, "TSandAlb", tex["T_Sand_Alb"], -1500, nexty())),
        ("TBeachAlb", texture_param(mat, "TBeachAlb", tex["T_Beach_Alb"], -1500, nexty())),
    ]
    ntexes = [
        ("TRockNrm", texture_param(mat, "TRockNrm", tex["T_Rock_Nrm"], -1500, nexty())),
        ("TScrubNrm", texture_param(mat, "TScrubNrm", tex["T_Scrub_Nrm"], -1500, nexty())),
        ("TSandNrm", texture_param(mat, "TSandNrm", tex["T_Sand_Nrm"], -1500, nexty())),
        ("TBeachNrm", texture_param(mat, "TBeachNrm", tex["T_Beach_Nrm"], -1500, nexty())),
    ]
    nstr = scalar_param(mat, "NormalStrength", 1.0, -1500, nexty())

    weight_params = ["BeachTopM", "ScrubStartM", "ScrubFeatherM",
                     "SlopeRockStart", "SlopeRockFeather",
                     "MacroTileMul", "MacroBlend"]
    tile_params = ["RockTileM", "ScrubTileM", "SandTileM", "BeachTileM"]
    tint_params = ["RockColor", "ScrubColor", "SandColor", "BeachColor"]
    noise_params = ["NoiseScaleM", "NoiseAmp"]
    pmap = dict(params)

    color_inputs = (["WP", "N"] + [n for n, _ in texes] + tile_params +
                    weight_params + tint_params + noise_params)
    cn = custom_node(mat, TERRAIN_COLOR_HLSL, color_inputs,
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3, -500, 0)
    MEL.connect_material_expressions(wp, "", cn, "WP")
    MEL.connect_material_expressions(nrm, "", cn, "N")
    for n, e in texes:
        MEL.connect_material_expressions(e, "", cn, n)
    for n in tile_params + weight_params + tint_params + noise_params:
        MEL.connect_material_expressions(pmap[n], "", cn, n)
    MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal_inputs = (["WP", "N"] + [n for n, _ in ntexes] + tile_params +
                     weight_params + ["NormalStrength"])
    nn = custom_node(mat, TERRAIN_NORMAL_HLSL, normal_inputs,
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3, -500, 600)
    MEL.connect_material_expressions(wp, "", nn, "WP")
    MEL.connect_material_expressions(nrm, "", nn, "N")
    for n, e in ntexes:
        MEL.connect_material_expressions(e, "", nn, n)
    for n in tile_params + weight_params:
        MEL.connect_material_expressions(pmap[n], "", nn, n)
    MEL.connect_material_expressions(nstr, "", nn, "NormalStrength")
    MEL.connect_material_property(nn, "", unreal.MaterialProperty.MP_NORMAL)

    rough = scalar_param(mat, "TerrainRoughness", 0.9, -500, 1200)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


ROAD_HLSL = """
float3 wp = WP.xyz / 100.0;
float2 uv = wp.xy;
float mm = max(RoadMacroMul, 1.5);
float mb = saturate(RoadMacroBlend);
float3 c = lerp(Texture2DSample(TRoadAlb, TRoadAlbSampler, uv / max(RoadTileM, 0.01)).rgb,
                Texture2DSample(TRoadAlb, TRoadAlbSampler, uv / (max(RoadTileM, 0.01) * mm)).rgb, mb);
return c * RoadColor.rgb;
"""


def build_road_material(tex):
    mat = fresh_material("M_Road")
    # Ribbon winding unproven both ways (gen_assets.py) - keep two-sided.
    mat.set_editor_property("two_sided", True)
    wp = MEL.create_material_expression(
        mat, unreal.MaterialExpressionWorldPosition, -1500, 0)
    col = vector_param(mat, "RoadColor", (1.0, 1.0, 1.0), -1500, 150)
    talb = texture_param(mat, "TRoadAlb", tex["T_Road_Alb"], -1500, 300)
    tile = scalar_param(mat, "RoadTileM", 3.5, -1500, 450)
    mmul = scalar_param(mat, "RoadMacroMul", 11.0, -1500, 600)
    mbl = scalar_param(mat, "RoadMacroBlend", 0.35, -1500, 750)
    cn = custom_node(mat, ROAD_HLSL,
                     ["WP", "TRoadAlb", "RoadTileM", "RoadMacroMul",
                      "RoadMacroBlend", "RoadColor"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    MEL.connect_material_expressions(wp, "", cn, "WP")
    MEL.connect_material_expressions(talb, "", cn, "TRoadAlb")
    MEL.connect_material_expressions(tile, "", cn, "RoadTileM")
    MEL.connect_material_expressions(mmul, "", cn, "RoadMacroMul")
    MEL.connect_material_expressions(mbl, "", cn, "RoadMacroBlend")
    MEL.connect_material_expressions(col, "", cn, "RoadColor")
    MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(mat, "RoadRoughness", 0.9, -1500, 900)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def main():
    with open(os.path.join(HERE, "asset_manifest.json"), "r", encoding="utf-8") as f:
        man = json.load(f)
    cache = cache_root()
    log("cache: " + cache)
    only = os.environ.get("SHITFIRE_IMPORT_ONLY")
    if only:
        # Scoped re-run: just the named models; textures, M_VegBase and the
        # terrain/road materials are left as they are on disk.
        names = set(n.strip() for n in only.split(",") if n.strip())
        missing = names - set(m["name"] for m in man["models"])
        if missing:
            raise RuntimeError("SHITFIRE_IMPORT_ONLY names unknown models: %s" % sorted(missing))
        man = dict(man, models=[m for m in man["models"] if m["name"] in names])
        veg_base = EAL.load_asset("/Game/Vegetation/M_VegBase") or ensure_veg_base()
        import_models(man, cache, veg_base)
        log("done (scoped to %s)" % sorted(names))
        return
    tex = import_textures(man, cache)
    veg_base = ensure_veg_base()
    import_models(man, cache, veg_base)
    build_terrain_material(tex)
    build_road_material(tex)
    log("done")


main()
