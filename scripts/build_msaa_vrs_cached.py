"""Cache final MSAA2 VRS maps; verify parent build and restore Mesa sources."""
import argparse, fcntl, hashlib, json, re, shutil, subprocess
from pathlib import Path

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--mesa',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    repo=Path(__file__).resolve().parents[1];mesa=a.mesa.resolve();out=a.out.resolve()
    assert not out.exists() and not out.is_relative_to(mesa)
    baseline=repo/'dependencies/native-build/quest3-turnip/optimization-mk2-msaa-vrs/build.json'
    report=json.loads(baseline.read_text());assert sha(baseline)=='d908ae03484ed9c39856e042847d6a7180036dc84a3edb43677589d1ea486778'
    report['parentCandidateSha256']=report['candidateSha256'];report['restored']=False;report['gpuValidated']=False
    patch='tests/turnip_vrs/msaa2-final-map-cache.patch'
    report['patches'].append(dict(path=patch,sha256=sha(repo/patch)))
    report['inputs'].append(dict(path='scripts/build_msaa_vrs_cached.py',sha256=sha(Path(__file__))))
    for item in report['patches']+report['inputs']:assert sha(repo/item['path'])==item['sha256'],item['path']
    out.mkdir(parents=True)
    lock=(mesa/'build-android/alyx-fdm-vrs.lock').open('w');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    lib=mesa/'build-android/src/freedreno/vulkan/libvulkan_freedreno.so'
    assert sha(lib)==report['baselineSha256']
    paths=set()
    for item in report['patches']:
        for name in re.findall(r'^\+\+\+ b/(.+)$',(repo/item['path']).read_text(),re.M):
            path=(mesa/name).resolve();assert path.is_relative_to(mesa);paths.add(path)
    saved={x:x.read_bytes() if x.exists() else None for x in paths}
    for path,data in saved.items():
        if data is not None:
            backup=out/'source-backup'/path.relative_to(mesa);backup.parent.mkdir(parents=True,exist_ok=True);backup.write_bytes(data)
    def build(name):
        with (out/(name+'.log')).open('w') as log:subprocess.run(['ninja','-C','build-android','-j8','src/freedreno/vulkan/libvulkan_freedreno.so'],cwd=mesa,stdout=log,stderr=subprocess.STDOUT,check=True)
    try:
        for item in report['patches'][:-1]:subprocess.run(['git','apply',str(repo/item['path'])],cwd=mesa,check=True)
        build('parent');assert sha(lib)==report['parentCandidateSha256'], 'Parent driver not reproduced'
        subprocess.run(['git','apply',str(repo/patch)],cwd=mesa,check=True)
        build('candidate');shutil.copyfile(lib,out/'vulkan.ad07xx.so');report['candidateSha256']=sha(out/'vulkan.ad07xx.so')
    finally:
        for path,data in saved.items():
            if data is None:path.unlink(missing_ok=True)
            else:path.write_bytes(data)
        try:
            build('restored');report['restoredSha256']=sha(lib);report['restored']=report['restoredSha256']==report['baselineSha256']
        finally:(out/'build.json').write_text(json.dumps(report,indent=2)+'\n')
    assert report['restored'];print(report['candidateSha256'],flush=True)

if __name__=='__main__':main()
