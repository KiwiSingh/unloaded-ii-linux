import pathlib,tempfile,subprocess,os,signal,json,hashlib,shutil,time,tarfile,base64
import argparse
parser=argparse.ArgumentParser(description='Bounded native MVGL test using an isolated Proton copy and a verified staged prefix snapshot')
parser.add_argument('--stage', required=True, help='Directory containing test-hooks.exe, prefix snapshot and compat-metadata')
parser.add_argument('--runtime', default='/home/steamos/.local/share/Steam/steamapps/common/Proton 11.0 (ARM64)')
args=parser.parse_args()
stage=pathlib.Path(args.stage).resolve()
runtime=args.runtime
with tempfile.TemporaryDirectory(prefix='mvgl-native-synthetic-',dir='/home/steamos/.local/share') as directory:
 work=pathlib.Path(directory)
 for name in ['compatdata','home','steam-client','tmp']: (work/name).mkdir()
 shutil.copy2(stage/'test-hooks.exe',work/'test-hooks.exe')
 archive=stage/'prefix-AppID-1984270.tar.gz'
 assert hashlib.sha256(archive.read_bytes()).hexdigest()=='b546692f45452c972e30b013a4a7399fed1e92c0ef04fdfce8611b2673837b88'
 with tarfile.open(archive,'r:gz') as tar:
  manifest=json.load(tar.extractfile('manifest.json'));entries=manifest['entries'];root=work/'compatdata'/'pfx';root.mkdir()
  assert len(entries)<10000
  total=0
  for path,e in sorted(entries.items(),key=lambda x:(x[0].count('/'),x[0])):
   rel=pathlib.PurePosixPath(path);assert not rel.is_absolute() and '..' not in rel.parts
   target=root/path
   if not path:continue
   if e['kind']=='directory':target.mkdir()
   elif e['kind']=='file':
    member=tar.getmember('snapshot/'+path);assert member.isfile() and member.size<100000000;total+=member.size;assert total<500000000
    with tar.extractfile(member) as src,target.open('xb') as dst:shutil.copyfileobj(src,dst)
    assert hashlib.sha256(target.read_bytes()).hexdigest()==e['sha256']
  for path,e in entries.items():
   target=root/path
   if e['kind']=='symlink':os.symlink(e['target'],target)
   else:
    os.chmod(target,e['mode'])
    for key,value in e.get('xattrs',{}).items():os.setxattr(target,key,base64.b64decode(value))
 for file in (stage/'compat-metadata').iterdir():shutil.copy2(file,work/'compatdata'/file.name)
 clone=work/'runtime'
 subprocess.run(['cp','-a','--reflink=auto',runtime,str(clone)],check=True)
 wrapper=clone/'proton';source=wrapper.read_text();needle='g_session.init_session(sys.argv[1] != "runinprefix")';assert source.count(needle)==1
 wrapper.write_text(source.replace(needle,'g_session.init_session(True)'))
 base=['bwrap' ,'--unshare-all','--die-with-parent','--clearenv','--ro-bind','/usr','/usr','--symlink','usr/lib','/lib','--symlink','usr/bin','/bin','--symlink','usr/bin','/sbin','--ro-bind','/etc','/etc','--ro-bind','/sys/devices/system/cpu','/sys/devices/system/cpu','--proc','/proc','--dev','/dev','--bind',str(work/'tmp'),'/tmp','--bind',directory,'/work','--bind',str(clone),runtime,'--chdir','/work','--setenv','PATH','/usr/bin:/bin','--setenv','HOME','/work/home','--setenv','TMPDIR','/work/tmp','--setenv','XDG_CACHE_HOME','/work/home/.cache','--setenv','STEAM_COMPAT_DATA_PATH','/work/compatdata','--setenv','STEAM_COMPAT_CLIENT_INSTALL_PATH','/work/steam-client','--setenv','SteamGameId','0','--setenv','SteamAppId','0','--setenv','PROTON_LOG','1','--setenv','PROTON_NO_FSYNC','1','--setenv','WINEDEBUG','err+all','--setenv','LIBGL_ALWAYS_SOFTWARE','1','--','/usr/bin/xvfb-run','-a',runtime+'/proton','runinprefix','Z:/work/test-hooks.exe']
 start=time.monotonic()
 with (work/'run.log').open('wb') as log:
  proc=subprocess.Popen(base,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  timed=False
  try: code=proc.wait(timeout=90)
  except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);code=proc.wait();timed=True
 result={'exitCode':code,'timedOut':timed,'seconds':round(time.monotonic()-start,2),'livePrefixMounted':False,'networkIsolated':True,'freshPrefix':False,'restoredVerifiedSyntheticPrefix':True,'runtimeCopyModified':'runinprefix session-init only; original untouched','exeSha256':hashlib.sha256((work/'test-hooks.exe').read_bytes()).hexdigest(),'output':(work/'run.log').read_text(errors='replace')[-20000:]}
 for log in (work/'home').glob('steam-*.log'): shutil.copy2(log,stage/'native-proton.log')
 marker='MVGL native x64 inline hooks: snapshot stability, size/read replacement, vanilla fallback and removal passed'
 proton_log=(stage/'native-proton.log').read_text(errors='replace') if (stage/'native-proton.log').exists() else ''
 result['successMarkerPresent']=marker in proton_log
 result['passed']=code==0 and not timed and marker in proton_log and 'FAIL line' not in proton_log
 result['protonOutput']=proton_log[-10000:]
 (stage/'native-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

if not result["passed"]: raise SystemExit(1)
