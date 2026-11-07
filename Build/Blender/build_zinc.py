"""Original tileable zinc finish, 6.25 mm crystals; no imported pixels."""
import bpy, numpy as np
from pathlib import Path
N=2048
out=Path(__file__).resolve().parents[2]/'ArtSource/Photoreal/Textures';out.mkdir(parents=True,exist_ok=True)
rng=np.random.default_rng(71126);cells=160
y,x=np.mgrid[0:N,0:N].astype(np.float32)/N*cells
ix=x.astype(np.int32);iy=y.astype(np.int32)
sites=rng.random((cells,cells,2),dtype=np.float32);tones=rng.random((cells,cells),dtype=np.float32)
nearest=np.full((N,N),1e6,dtype=np.float32);grain=np.zeros_like(nearest)
for dy in (-1,0,1):
    for dx in (-1,0,1):
        site=sites[(iy+dy)%cells,(ix+dx)%cells]
        d=(x-(ix+dx+site[:,:,0]))**2+(y-(iy+dy+site[:,:,1]))**2
        mask=d<nearest;nearest=np.minimum(nearest,d)
        grain=np.where(mask,tones[(iy+dy)%cells,(ix+dx)%cells],grain)
v=.84+(grain-.5)*.07
rgb=np.stack((v*.985,v,v*1.015),axis=2)
theta=grain*np.pi*2
normal=np.stack((np.cos(theta)*.012,np.sin(theta)*.012,np.ones_like(v)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
orm=np.stack((np.ones_like(v),.48+(grain-.5)*.15,np.ones_like(v)),axis=2)
for suffix,values in [('BC',rgb),('N',normal*.5+.5),('ORM',orm)]:
    rgba=np.ones((N,N,4),dtype=np.float32);rgba[:,:,:3]=values
    image=bpy.data.images.new('T_P_Zinc_'+suffix,width=N,height=N,alpha=False)
    image.colorspace_settings.name='Non-Color';image.pixels.foreach_set(rgba.ravel());image.update()
    image.file_format='PNG';image.filepath_raw=str(out/('T_P_Zinc_'+suffix+'.png'));image.save()
    bpy.data.images.remove(image)
print('ZINC_MAPS_PASS',N,N)
