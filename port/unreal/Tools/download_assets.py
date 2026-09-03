"""Fetches every asset in asset_manifest.json into the local asset cache.

Plain-stdlib Python (3.9+); runs with any system python OR UE's embedded
python. Raw archives NEVER enter the repo - this script re-creates the cache
that import_assets.py imports from, making the committed .uassets
regenerable end-to-end (UNREAL_RESEARCH.md 8 discipline).

Cache location: %SHITFIRE_ASSET_CACHE% if set, else
%LOCALAPPDATA%/shitfire-asset-cache. Layout:
  <cache>/textures/<Id>_2K-JPG.zip     (kept, for re-extraction)
  <cache>/textures/<Id>/...jpg         (extracted maps)
  <cache>/models/<Name>.glb

Existing files are skipped - delete a file to force a re-download.
poly.pizza's CDN sits behind Cloudflare and serves bot-UA requests an HTML
challenge page, so a browser User-Agent header is required; every download
is verified (zip magic / glTF magic) so a challenge page can never be
mistaken for an asset.

Usage:  python port/unreal/Tools/download_assets.py
"""
import json
import os
import sys
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36")


def cache_root():
    root = os.environ.get("SHITFIRE_ASSET_CACHE")
    if not root:
        base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        root = os.path.join(base, "shitfire-asset-cache")
    return root


def fetch(url, dest, referer=None):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        print("cached  " + dest)
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    headers = {"User-Agent": UA, "Accept": "*/*"}
    if referer:
        headers["Referer"] = referer
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=120) as r, open(dest, "wb") as f:
        while True:
            chunk = r.read(1 << 16)
            if not chunk:
                break
            f.write(chunk)
    print("fetched " + dest)


def verify_magic(path, magics):
    with open(path, "rb") as f:
        head = f.read(4)
    if not any(head.startswith(m) for m in magics):
        os.remove(path)
        raise RuntimeError(
            "%s is not the expected file type (got %r) - likely a bot-block "
            "page; re-run, or fetch the URL in a browser" % (path, head))


def main():
    with open(os.path.join(HERE, "asset_manifest.json"), "r", encoding="utf-8") as f:
        man = json.load(f)
    root = cache_root()
    print("asset cache: " + root)

    for t in man["textures"]:
        zpath = os.path.join(root, "textures", t["id"] + "_2K-JPG.zip")
        fetch(t["url"], zpath)
        verify_magic(zpath, [b"PK"])
        exdir = os.path.join(root, "textures", t["id"])
        need = [t["albedo"], t["normal_gl"]]
        if not all(os.path.exists(os.path.join(exdir, n)) for n in need):
            with zipfile.ZipFile(zpath) as z:
                z.extractall(exdir)
            print("extract " + exdir)

    for m in man["models"]:
        gpath = os.path.join(root, "models", m["name"] + ".glb")
        fetch(m["glb"], gpath, referer="https://poly.pizza/")
        verify_magic(gpath, [b"glTF"])

    print("download_assets: done (%d texture sets, %d models)"
          % (len(man["textures"]), len(man["models"])))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # noqa: BLE001
        print("download_assets: FAILED: %s" % e)
        sys.exit(1)
