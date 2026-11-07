"""Bake original, tileable material maps through Blender's image API.
Low-frequency staining, aggregate, pores, coating wear and micro-normal detail are
separate scales. No reference image pixels or third-party texture data are used.
"""
import numpy as np
N=2048
rng=np.random.default_rng(270719)
tex_dir=OUT/'Textures';tex_dir.mkdir(parents=True,exist_ok=True)
y,x=np.mgrid[0:N,0:N].astype(np.float32)/N

def field(cells):
    small=rng.random((cells,cells),dtype=np.float32)
    px=x*cells;py=y*cells
    ix=px.astype(np.int32);iy=py.astype(np.int32)
    fx=px-ix;fy=py-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy)
    return ((small[iy%cells,ix%cells]*(1-fx)+small[iy%cells,(ix+1)%cells]*fx)*(1-fy)+
        (small[(iy+1)%cells,ix%cells]*(1-fx)+small[(iy+1)%cells,(ix+1)%cells]*fx)*fy)

def write_image(name,rgb,normal=False):
    rgba=np.ones((N,N,4),dtype=np.float32);rgba[:,:,:3]=np.clip(rgb,0,1)
    image=bpy.data.images.new(name,width=N,height=N,alpha=False,float_buffer=False)
    image.colorspace_settings.name='Non-Color'
    image.pixels.foreach_set(rgba.ravel());image.update()
    image.file_format='PNG';image.filepath_raw=str(tex_dir/(name+'.png'));image.save()
    bpy.data.images.remove(image)

for family in ('Concrete','Floor','Metal','Paint','Rubber'):
    broad=field(5);mottle=field(31);grain=field(310);fine=field(970)
    height=np.zeros((N,N),dtype=np.float32)
    rough=np.ones_like(height)*.62
    metal=np.ones_like(height)
    ao=np.ones_like(height)
    value=.92+(broad-.5)*.08+(mottle-.5)*.04
    if family in ('Concrete','Floor'):
        aggregate=np.clip((grain-.55)*2,0,1)
        pores=np.clip((fine-.73)*7,0,1)**2
        height=(grain-.5)*.025-pores*.045
        value-=aggregate*.025+pores*.15
        # Sparse, directional seepage, not a uniformly dirty overlay.
        drips=np.clip((field(19)-.67)*4,0,1)*(np.sin(y*math.tau)*.5+.5)
        value-=drips*.08
        rough=.75+(mottle-.5)*.16+pores*.12
        ao=1-pores*.24;metal[:]=0
        if family=='Floor':
            polished=np.clip((broad-.54)*3,0,1)
            value-=polished*.035;rough-=polished*.12
            # Hairline construction crazing kept below a millimetre in the normal map.
            line=np.exp(-((np.sin(x*math.tau*3+(mottle-.5)*.7))/.012)**2)
            height-=line*.012;value-=line*.045
    elif family=='Metal':
        brushing=np.sin(y*N*1.2)*.35+np.sin(y*N*.61)*.2
        height=(fine-.5)*.0015+brushing*.001
        oxidation=np.clip((broad-.62)*3,0,1)*np.clip((mottle-.45)*3,0,1)
        value-=oxidation*.13
        rough=.48+(grain-.5)*.12+oxidation*.25
        metal=1-oxidation*.65
    elif family=='Paint':
        coating=np.clip((fine-.87)*11,0,1)*np.clip((mottle-.6)*4,0,1)
        # Sparse scuffs with a coherent direction; most of the coating is maintained.
        scratch=np.clip((np.sin(y*N*.27+x*37)-.996)*120,0,1)*np.clip((broad-.65)*3,0,1)
        chip=np.maximum(coating,scratch*.65)
        height=(fine-.5)*.0015-chip*.006
        value-=chip*.08;rough=.62+(mottle-.5)*.15
        metal=chip
    else:
        height=(grain-.5)*.004
        rough=.83+(grain-.5)*.08;metal[:]=0
    rgb=np.stack((value,value*.995,value*.98),axis=2)
    if family=='Metal':
        rgb[:,:,0]+=oxidation*.035;rgb[:,:,2]-=oxidation*.055
    tile_cm=200 if family in ('Concrete','Floor') else 100
    dx=(np.roll(height,-1,1)-np.roll(height,1,1))*N/(2*tile_cm)
    dy=(np.roll(height,-1,0)-np.roll(height,1,0))*N/(2*tile_cm)
    normals=np.stack((-dx,-dy,np.ones_like(dx)),axis=2)
    normals/=np.linalg.norm(normals,axis=2,keepdims=True)
    write_image('T_R_'+family+'_BC',rgb)
    write_image('T_R_'+family+'_N',normals*.5+.5,True)
    write_image('T_R_'+family+'_ORM',np.stack((ao,rough,metal),axis=2))
    print('TEXTURE_BAKE',family,N,N)

REFINERY_FAMILIES={name:('Concrete' if name in ('Concrete','Ivory') else 'Floor' if name=='Floor' else 'Rubber' if name in ('Rubber','Stair') else 'Paint' if name in ('Yellow','Teal','RedOxide') else 'Metal') for name in PALETTE}
