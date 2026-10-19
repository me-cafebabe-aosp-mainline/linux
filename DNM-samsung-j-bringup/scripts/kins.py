import sys,re
d,k,n,drv=sys.argv[1:]
p=d+'/Kconfig'; s=open(p).read()
if f'config {k}\n' not in s:
    new=f'config {k}\n\ttristate "{n}"\n\tdefault DRM_PANEL_MSM8916_GENERATED\n\n'
    # insert before first config entry (after the head one) with greater name
    idx=[m for m in re.finditer(r'^config (\S+)\n',s,re.M)][1:]
    pos=None
    for m in idx:
        if m.group(1)>k: pos=m.start(); break
    if pos is None: pos=s.rindex('endmenu')
    s=s[:pos]+new+s[pos:]
    open(p,'w').write(s)
p=d+'/Makefile'; lines=open(p).read().splitlines()
l=f'obj-$(CONFIG_{k}) += {drv}.o'
if l not in lines:
    pos=len(lines)
    for i,x in enumerate(lines):
        m=re.match(r'obj-\$\(CONFIG_(\S+)\)',x)
        if m and m.group(1)>k: pos=i;break
    lines.insert(pos,l); open(p,'w').write('\n'.join(lines)+'\n')
