"""Build one independent strategy against the verified control objects."""
import argparse,json
import build_texture_alias_trial as build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('strategy',choices=('staging','intervalos','texturas'))
    a=p.parse_args()
    build.OUT=build.ROOT / ('work/strategy-'+a.strategy+'-20261105')
    build.PATCH=build.ROOT / ('tools/gpu_patches/sotc-strategy-'+a.strategy+'.patch.json')
    build.main()
    path=build.OUT/'trial-build.json';data=json.loads(path.read_text())
    headers=['vegetation_parameter_scale.h']
    if a.strategy=='staging':headers.append('staging_size_classes.h')
    data['adapter_headers']={h:build.digest(build.ROOT/'runtime/src'/h) for h in headers}
    path.write_text(json.dumps(data,indent=2)+'\n')
if __name__=='__main__':main()
