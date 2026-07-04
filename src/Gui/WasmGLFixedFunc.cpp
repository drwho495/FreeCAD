/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * WebAssembly build only. A compact fixed-function OpenGL 1.x emulation over
 * WebGL2/GLES3, so Coin3D (which renders FreeCAD geometry with legacy GL) can
 * draw into the WebGL2 context Qt hands us. Emscripten's LEGACY_GL_EMULATION
 * is WebGL1-only and conflicts with Qt's forced WebGL2, so we implement the
 * subset Coin actually exercises ourselves.
 *
 * Design: the legacy entry points are C functions here; the state and drawing
 * live in JS (EM_JS) talking straight to emscripten's current context
 * (`Module.ctx`/`GLctx`). We maintain modelview/projection matrix stacks, the
 * fixed-function material/light state, and the client vertex-array bindings,
 * then translate glDrawArrays/glDrawElements/glBegin..glEnd into a shader draw
 * with an uploaded MVP + a single directional light.
 *
 * This is the Stage-4 backend; it replaces the earlier no-op shim. Coverage is
 * the FreeCAD viewport subset (lit/flat triangles, lines, points), not all of
 * GL 1.x. See research/R3-graphics-coin-webgl.md.
 */
#include <stddef.h>
#include <emscripten.h>

typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef double GLdouble;
typedef unsigned char GLubyte;
typedef unsigned char GLboolean;
typedef ptrdiff_t GLintptr;

/* One-time JS state + helpers. Attached to globalThis.__ff. */
EM_JS(void, ff_init, (void), {
  if (globalThis.__ff) return;
  // Resolve the WebGL2 context robustly: emscripten's current-context global
  // (GLctx), the GL module's current context, or Module.ctx. Qt makes its
  // context current via emscripten's GL module, so GL.currentContext is the
  // reliable source during a paint.
  const gl = () => {
    if (typeof GLctx !== 'undefined' && GLctx) return GLctx;
    if (typeof GL !== 'undefined' && GL.currentContext && GL.currentContext.GLctx) return GL.currentContext.GLctx;
    if (Module['ctx']) return Module['ctx'];
    return null;
  };
  const ident = () => [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1];
  const mul = (a,b) => { // column-major 4x4, result = a*b
    const o = new Array(16);
    for (let c=0;c<4;c++) for (let r=0;r<4;r++) {
      let s=0; for (let k=0;k<4;k++) s += a[k*4+r]*b[c*4+k];
      o[c*4+r]=s;
    }
    return o;
  };
  globalThis.__ff = {
    gl,
    mode: 0x1700, // GL_MODELVIEW
    mv: [ident()], pr: [ident()],
    cur() { return this.mode===0x1701 ? this.pr : this.mv; }, // GL_PROJECTION=0x1701
    color: [1,1,1,1],
    normal: [0,0,1],
    lighting: false,
    lightDir: [0.3,0.3,1.0],
    arrays: { // enabled + pointer specs
      vertex:{on:false,size:3,type:0x1406,stride:0,ptr:0,vbo:0},
      normal:{on:false,size:3,type:0x1406,stride:0,ptr:0,vbo:0},
      color: {on:false,size:4,type:0x1406,stride:0,ptr:0,vbo:0},
    },
    arrayBuffer: 0,
    prog: null, loc: null,
    posVBO: null, nrmVBO: null, colVBO: null, idxVBO: null,
    imm: null, // immediate-mode accumulation
    lists: {}, curList: null, curListMode: 0, _listCtr: 0, // GL display lists
    mul, ident,
    // Draw one recorded immediate-mode primitive using the CURRENT matrices, so a
    // replayed display list follows camera changes (correct orbit/pan/zoom).
    emitImm(rec) {
      const g=this.gl(); if(!g||!rec||rec.verts.length===0) return; if(!this.program()) return;
      g.useProgram(this.prog);
      const mvp=this.mul(this.pr[this.pr.length-1], this.mv[this.mv.length-1]);
      g.uniformMatrix4fv(this.loc.mvp,false,new Float32Array(mvp));
      g.uniformMatrix3fv(this.loc.nm,false,new Float32Array(this.normalMat3(this.mv[this.mv.length-1])));
      g.uniform1i(this.loc.lit, rec.lighting?1:0);
      g.uniform3fv(this.loc.ldir,new Float32Array(rec.lightDir)); g.uniform1f(this.loc.psize,3.0);
      g.uniform4fv(this.loc.color,new Float32Array(rec.color));
      g.bindBuffer(g.ARRAY_BUFFER,this.posVBO); g.bufferData(g.ARRAY_BUFFER,new Float32Array(rec.verts),g.STREAM_DRAW);
      g.enableVertexAttribArray(0); g.vertexAttribPointer(0,3,g.FLOAT,false,0,0);
      if(rec.nrms.length>0){ g.bindBuffer(g.ARRAY_BUFFER,this.nrmVBO); g.bufferData(g.ARRAY_BUFFER,new Float32Array(rec.nrms),g.STREAM_DRAW);
        g.enableVertexAttribArray(1); g.vertexAttribPointer(1,3,g.FLOAT,false,0,0); }
      else { g.disableVertexAttribArray(1); g.vertexAttrib3f(1,0,0,1); }
      const useCol = rec.cols.length>0;
      g.uniform1i(this.loc.useCol, useCol?1:0);
      if(useCol){ g.bindBuffer(g.ARRAY_BUFFER,this.colVBO); g.bufferData(g.ARRAY_BUFFER,new Float32Array(rec.cols),g.STREAM_DRAW);
        g.enableVertexAttribArray(2); g.vertexAttribPointer(2,4,g.FLOAT,false,0,0); }
      else g.disableVertexAttribArray(2);
      g.disable(g.CULL_FACE);
      const nv=rec.verts.length/3;
      if(rec.mode===7){ const nq=(nv/4)|0; const tri=new Uint32Array(nq*6);
        for(let q=0;q<nq;q++){const b=q*4; tri[q*6]=b;tri[q*6+1]=b+1;tri[q*6+2]=b+2;tri[q*6+3]=b;tri[q*6+4]=b+2;tri[q*6+5]=b+3;}
        g.bindBuffer(g.ELEMENT_ARRAY_BUFFER,this.idxVBO); g.bufferData(g.ELEMENT_ARRAY_BUFFER,tri,g.STREAM_DRAW);
        g.drawElements(g.TRIANGLES,nq*6,g.UNSIGNED_INT,0);
      } else { const dm=rec.mode===8?g.TRIANGLE_STRIP:rec.mode===9?g.TRIANGLE_FAN:rec.mode; g.drawArrays(dm,0,nv); }
    },
    program() {
      if (this.prog) return this.prog;
      const g = this.gl(); if (!g) return null;
      const vs = `attribute vec3 aPos; attribute vec3 aNormal; attribute vec4 aColor;
        uniform mat4 uMVP; uniform mat3 uNormalMat; uniform vec4 uColor;
        uniform bool uUseColorArray; uniform bool uLighting; uniform vec3 uLightDir;
        uniform float uPointSize;
        varying vec4 vColor;
        void main(){
          gl_Position = uMVP * vec4(aPos,1.0);
          gl_PointSize = uPointSize;
          vec4 base = uUseColorArray ? aColor : uColor;
          if(uLighting){
            vec3 n = normalize(uNormalMat * aNormal);
            vec3 L = normalize(uLightDir);
            float d = abs(dot(n, L));        // two-sided
            vColor = vec4(base.rgb*(0.35+0.65*d), base.a);
          } else { vColor = base; }
        }`;
      const fs = `precision mediump float; varying vec4 vColor;
        void main(){ gl_FragColor = vColor; }`;
      const sh=(t,s)=>{ const o=g.createShader(t); g.shaderSource(o,s); g.compileShader(o);
        if(!g.getShaderParameter(o,g.COMPILE_STATUS)) console.error('ff shader',g.getShaderInfoLog(o)); return o; };
      const p=g.createProgram();
      g.attachShader(p,sh(g.VERTEX_SHADER,vs)); g.attachShader(p,sh(g.FRAGMENT_SHADER,fs));
      g.bindAttribLocation(p,0,'aPos'); g.bindAttribLocation(p,1,'aNormal'); g.bindAttribLocation(p,2,'aColor');
      g.linkProgram(p);
      if(!g.getProgramParameter(p,g.LINK_STATUS)) console.error('ff link',g.getProgramInfoLog(p));
      this.prog=p;
      this.loc={ mvp:g.getUniformLocation(p,'uMVP'), nm:g.getUniformLocation(p,'uNormalMat'),
        color:g.getUniformLocation(p,'uColor'), useCol:g.getUniformLocation(p,'uUseColorArray'),
        lit:g.getUniformLocation(p,'uLighting'), ldir:g.getUniformLocation(p,'uLightDir'),
        psize:g.getUniformLocation(p,'uPointSize') };
      this.posVBO=g.createBuffer(); this.nrmVBO=g.createBuffer(); this.colVBO=g.createBuffer(); this.idxVBO=g.createBuffer();
      return p;
    },
    normalMat3(m) { // upper-left 3x3 of modelview (good enough for rigid/uniform)
      return [m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10]];
    },
  };
});

/* Read `n` floats from a client array element into dst (handles type). */
EM_JS(void, ff_setup_and_draw, (GLenum prim, GLsizei count, GLenum idxType, GLintptr idxPtr, GLint first, int isElements), {
  const F = globalThis.__ff; const g = F.gl(); if (!g) return;
  if (!F.program()) return;
  g.useProgram(F.prog);

  const mvp = F.mul(F.pr[F.pr.length-1], F.mv[F.mv.length-1]);
  g.uniformMatrix4fv(F.loc.mvp, false, new Float32Array(mvp));
  g.uniformMatrix3fv(F.loc.nm, false, new Float32Array(F.normalMat3(F.mv[F.mv.length-1])));
  g.uniform4fv(F.loc.color, new Float32Array(F.color));
  g.uniform1i(F.loc.useCol, F.arrays.color.on ? 1 : 0);
  g.uniform1i(F.loc.lit, F.lighting ? 1 : 0);
  g.uniform3fv(F.loc.ldir, new Float32Array(F.lightDir));
  g.uniform1f(F.loc.psize, 3.0);

  const typeSize = (t)=>({0x1406:4,0x1400:1,0x1401:1,0x1402:2,0x1403:2,0x1404:4,0x1405:4}[t]||4);
  const heapFor = (t)=>({0x1406:HEAPF32,0x1401:HEAPU8,0x1400:HEAP8,0x1405:HEAPU32,0x1403:HEAPU16}[t]||HEAPF32);

  // vertex count to read for client arrays
  const nVerts = isElements ? 0 : (first+count);
  const bind = (spec, attrib, defaultN) => {
    if (!spec.on) { g.disableVertexAttribArray(attrib); return false; }
    g.enableVertexAttribArray(attrib);
    const sz = spec.size;
    if (spec.glbuf) {
      g.bindBuffer(g.ARRAY_BUFFER, spec.glbuf);
      g.vertexAttribPointer(attrib, sz, spec.type, spec.type===0x1401, spec.stride, spec.ptr);
    } else {
      // client memory: gather into a scratch VBO
      const stride = spec.stride || sz*typeSize(spec.type);
      const heap = heapFor(spec.type);
      const elem = spec.type===0x1406 ? 4 : typeSize(spec.type);
      const base = spec.ptr;
      const maxV = isElements ? F._maxIndex+1 : nVerts;
      const out = new Float32Array(maxV*sz);
      const norm = spec.type===0x1401; // ubyte color -> /255
      for (let v=0; v<maxV; v++) {
        const o = (base + v*stride);
        for (let k=0;k<sz;k++) {
          let val = heap[(o + k*elem) / (heap.BYTES_PER_ELEMENT)];
          out[v*sz+k] = norm ? val/255 : val;
        }
      }
      const vbo = attrib===0?F.posVBO:attrib===1?F.nrmVBO:F.colVBO;
      g.bindBuffer(g.ARRAY_BUFFER, vbo);
      g.bufferData(g.ARRAY_BUFFER, out, g.STREAM_DRAW);
      g.vertexAttribPointer(attrib, sz, g.FLOAT, false, 0, 0);
    }
    return true;
  };

  // Is the index data in a bound ELEMENT_ARRAY_BUFFER (VBO path) or client mem?
  const elemBuf = isElements ? g.getParameter(g.ELEMENT_ARRAY_BUFFER_BINDING) : null;
  const vertClient = F.arrays.vertex.on && !F.arrays.vertex.glbuf;

  // For client-array element draws, find max index to size the gathers.
  if (isElements && vertClient) {
    const iheap = idxType===0x1405?HEAPU32:idxType===0x1403?HEAPU16:HEAPU8;
    F._maxIndex = 0;
    const div = iheap.BYTES_PER_ELEMENT;
    if (elemBuf) {
      // indices are in a VBO — we cannot read them from a client heap; assume
      // they cover the whole client array (Coin uploads both together).
      F._maxIndex = F.arrays.vertex._count ? F.arrays.vertex._count-1 : count-1;
    } else {
      for (let i=0;i<count;i++){ const x=iheap[(idxPtr/div)+i]; if(x>F._maxIndex)F._maxIndex=x; }
    }
  }

  bind(F.arrays.vertex, 0, 3);
  if (!bind(F.arrays.normal, 1, 3)) g.vertexAttrib3f(1, F.normal[0],F.normal[1],F.normal[2]);
  if (!bind(F.arrays.color, 2, 4)) g.disableVertexAttribArray(2);

  g.disable(g.CULL_FACE);
  if ((F._drawCount=(F._drawCount||0)+1) <= 4)
    console.log('FF_DRAW #'+F._drawCount+' prim='+prim+' count='+count+' isElem='+isElements+' lit='+F.lighting);

  // WebGL2/GLES3 has no GL_QUADS(7)/GL_QUAD_STRIP(8)/GL_POLYGON(9). QUAD_STRIP and
  // POLYGON share vertex order with TRIANGLE_STRIP/TRIANGLE_FAN respectively, so
  // they remap directly. GL_QUADS must be expanded to two triangles per quad.
  const QUADS=7, QUAD_STRIP=8, POLYGON=9;
  let drawPrim = prim;
  if (prim===QUAD_STRIP) drawPrim = g.TRIANGLE_STRIP;
  else if (prim===POLYGON) drawPrim = g.TRIANGLE_FAN;

  const readIdx = () => {
    // return a JS array of the `count` source indices, from client mem or the
    // bound element buffer (WebGL2 getBufferSubData).
    if (isElements && !elemBuf) {
      const iheap = idxType===0x1405?HEAPU32:idxType===0x1403?HEAPU16:HEAPU8;
      const div = iheap.BYTES_PER_ELEMENT; const a=new Array(count);
      for (let i=0;i<count;i++) a[i]=iheap[(idxPtr/div)+i];
      return a;
    }
    if (isElements && elemBuf) {
      const bpe = idxType===0x1405?4:idxType===0x1403?2:1;
      const view = idxType===0x1405?new Uint32Array(count):idxType===0x1403?new Uint16Array(count):new Uint8Array(count);
      g.bindBuffer(g.ELEMENT_ARRAY_BUFFER, elemBuf);
      g.getBufferSubData(g.ELEMENT_ARRAY_BUFFER, idxPtr, view);
      return Array.from(view);
    }
    return null; // drawArrays: sequential first..first+count
  };

  if (prim===QUADS) {
    const nq = (count/4)|0;
    const tri = new Uint32Array(nq*6);
    const src = readIdx();
    for (let q=0;q<nq;q++){
      const b = src ? null : (first+q*4);
      const i0 = src ? src[q*4] : b, i1 = src ? src[q*4+1] : b+1,
            i2 = src ? src[q*4+2] : b+2, i3 = src ? src[q*4+3] : b+3;
      tri[q*6]=i0; tri[q*6+1]=i1; tri[q*6+2]=i2;
      tri[q*6+3]=i0; tri[q*6+4]=i2; tri[q*6+5]=i3;
    }
    g.bindBuffer(g.ELEMENT_ARRAY_BUFFER, F.idxVBO);
    g.bufferData(g.ELEMENT_ARRAY_BUFFER, tri, g.STREAM_DRAW);
    g.drawElements(g.TRIANGLES, nq*6, g.UNSIGNED_INT, 0);
    return;
  }

  if (isElements) {
    if (elemBuf) {
      g.drawElements(drawPrim, count, idxType, idxPtr);
    } else {
      const iheap = idxType===0x1405?HEAPU32:idxType===0x1403?HEAPU16:HEAPU8;
      const div = iheap.BYTES_PER_ELEMENT;
      const arr = idxType===0x1405 ? new Uint32Array(count) : new Uint16Array(count);
      for (let i=0;i<count;i++) arr[i]=iheap[(idxPtr/div)+i];
      g.bindBuffer(g.ELEMENT_ARRAY_BUFFER, F.idxVBO);
      g.bufferData(g.ELEMENT_ARRAY_BUFFER, arr, g.STREAM_DRAW);
      g.drawElements(drawPrim, count, idxType===0x1405?g.UNSIGNED_INT:g.UNSIGNED_SHORT, 0);
    }
  } else {
    g.drawArrays(drawPrim, first, count);
  }
});

/* Make emscripten's *current context* global (GLctx, used by emscripten's real
 * GL functions and by Coin's glGetString) point at the context Qt made current.
 * On Qt-wasm QOpenGLWidget, GL.currentContext can be set without the emscripten
 * GLctx global being synced, so Coin's cc_glglue_instance sees "no current
 * context". Call this right before Coin renders. */
EM_JS(void, ffSyncContext, (void), {
  if (typeof GL === 'undefined' || !GL.currentContext) return;
  try {
    if (typeof GLctx === 'undefined' || !GLctx || GLctx !== GL.currentContext.GLctx) {
      GL.makeContextCurrent(GL.currentContext.handle);
    }
  } catch (e) {}
  // Reset per-frame draw-diagnostic counters so each rendered frame logs its own
  // first few draws (fcWasmSyncGLContext runs once per actualRedraw).
  globalThis.__ffie = 0;
  if (globalThis.__ff) globalThis.__ff._drawCount = 0;
  const fn=(globalThis.__ffFrame=(globalThis.__ffFrame||0)+1);
  if (fn<=12) console.log('FRAME #'+fn+' newlists='+(globalThis.__ffNL||0)+' calllists='+(globalThis.__ffCL||0));
})

EM_JS(void, ffDbgRedraw, (void), {
  if((globalThis.__ffrd=(globalThis.__ffrd||0)+1) <= 8)
    console.log('QUARTER_REDRAW #'+globalThis.__ffrd+' hasGL='+(typeof GL!=='undefined'&&!!GL.currentContext));
})

/* Qt-for-wasm gives a child QOpenGLWidget an OFFSCREEN WebGL canvas and composites
 * the window through a 2D canvas context (QWasmBackingStore is pure raster), so the
 * GL content never reaches the screen. Work around it: after Coin renders into the
 * widget's FBO, glReadPixels the result and putImageData it onto the window's
 * visible 2D canvas at the widget's device-pixel position. Qt only repaints the 2D
 * canvas for dirty raster regions and treats the GL-widget rect as externally
 * managed, so the pixels persist. (dx,dy)=widget top-left in device px. */
EM_JS(void, ffBlitToCanvas, (int dx, int dy), {
  const F=globalThis.__ff; const g=F?F.gl():null; if(!g) return;
  const vp = g.getParameter(g.VIEWPORT); const w=vp[2], h=vp[3];
  if (w<=0||h<=0) return;
  const fbo = g.getParameter(g.FRAMEBUFFER_BINDING);
  let buf;
  try { buf=new Uint8Array(w*h*4); g.readPixels(0,0,w,h,g.RGBA,g.UNSIGNED_BYTE,buf); }
  catch(e){ if((globalThis.__ffbc=(globalThis.__ffbc||0)+1)<=3)console.log('BLIT readPixels threw '+e); return; }
  // GL is bottom-up; ImageData is top-down. Flip rows.
  const out=new Uint8ClampedArray(w*h*4);
  for(let row=0; row<h; row++){ const s=(h-1-row)*w*4; out.set(buf.subarray(s,s+w*4), row*w*4); }
  const img=new ImageData(out, w, h);
  const ci=(Math.floor(h/2)*w+Math.floor(w/2))*4; // center pixel
  // Qt-wasm renders inside a shadow DOM and registers the window canvas in
  // specialHTMLTargets as "!qtwindow...". Find it there (the raster window canvas
  // has a 2D context we can paint the 3D pixels into).
  let dbg='';
  let cv=globalThis.__ffwincanvas;
  if(!cv || !cv.getContext){
    cv=null;
    let tgt=null;
    if(typeof specialHTMLTargets!=='undefined') { tgt=specialHTMLTargets; dbg='global'; }
    else if(typeof Module!=='undefined'&&Module.specialHTMLTargets) { tgt=Module.specialHTMLTargets; dbg='module'; }
    else if(typeof globalThis.Module!=='undefined'&&globalThis.Module.specialHTMLTargets){ tgt=globalThis.Module.specialHTMLTargets; dbg='globalModule'; }
    else dbg='none';
    if(tgt){ const keys=Object.keys(tgt);
      for(const k of keys){ if(k[0]!=='!')continue; const c=tgt[k]; if(!c)continue;
        let kind='?'; try{ kind=(c.tagName||'obj')+' '+(c.width||0)+'x'+(c.height||0); }catch(e){kind='err';}
        // is this canvas' context 2d, webgl, or unset?
        let ctxk='none'; try{
          if(c.__ctxKind) ctxk=c.__ctxKind;
          else { const t=c.getContext; ctxk = t?'has-getContext':'no-getContext'; }
        }catch(e){ctxk='threw';}
        dbg+=' ['+k.slice(1,14)+' '+kind+' '+ctxk+']';
        // pick the biggest qtwindow that yields a 2d context
        if(k.indexOf('qtwindow')>=0 && !cv){ try{ const c2=c.getContext('2d'); if(c2){ cv=c; } }catch(e){} }
      } }
    globalThis.__ffwincanvas=cv;
  } else dbg='cached';
  let res='no-canvas';
  if(cv){ let ctx=null; try{ ctx=cv.getContext('2d'); }catch(e){ res='getContext2d threw '+e; }
    if(ctx){ try{
      // Put the readback into a scratch canvas, then draw it SCALED to fill the
      // whole target canvas (temporary: proves visibility; positioning TBD).
      let tc=globalThis.__fftmpc; if(!tc||tc.width!==w||tc.height!==h){ tc=(typeof OffscreenCanvas!=='undefined')?new OffscreenCanvas(w,h):document.createElement('canvas'); tc.width=w; tc.height=h; globalThis.__fftmpc=tc; }
      tc.getContext('2d').putImageData(img,0,0);
      ctx.drawImage(tc, 0,0,w,h, 0,0, cv.width, cv.height);
      res='ok cv='+cv.width+'x'+cv.height;
    }catch(e){ res='draw threw '+e; } } }
  if((globalThis.__ffbc=(globalThis.__ffbc||0)+1)<=3)
    console.log('BLIT vp='+w+'x'+h+' dxy='+dx+','+dy+' rgba='+buf[ci]+','+buf[ci+1]+','+buf[ci+2]+' tgt['+dbg+'] -> '+res);
})

extern "C" {

void fcWasmSyncGLContext(void){ ffSyncContext(); }
void fcWasmBlitToCanvas(int dx, int dy){ (void)dx; (void)dy; /* superseded by WasmGLWidget FBO readback + QPainter blit */ }

/* ---- lifecycle ---- */
static int g_ff_ready = 0;
static void ensure() { if (!g_ff_ready) { ff_init(); g_ff_ready = 1; } ffSyncContext(); }

/* ---- matrix stack ---- */
EM_JS(void, ffMatrixMode, (GLenum m), { globalThis.__ff.mode = m; })
EM_JS(void, ffLoadIdentity, (void), { const F=globalThis.__ff; const s=F.cur(); s[s.length-1]=F.ident(); })
EM_JS(void, ffLoadMatrixf, (const GLfloat* m), { const F=globalThis.__ff; const s=F.cur(); const a=new Array(16); for(let i=0;i<16;i++)a[i]=HEAPF32[m/4+i]; s[s.length-1]=a; })
EM_JS(void, ffLoadMatrixd, (const GLdouble* m), { const F=globalThis.__ff; const s=F.cur(); const a=new Array(16); for(let i=0;i<16;i++)a[i]=HEAPF64[m/8+i]; s[s.length-1]=a; })
EM_JS(void, ffMultMatrixf, (const GLfloat* m), { const F=globalThis.__ff; const s=F.cur(); const b=new Array(16); for(let i=0;i<16;i++)b[i]=HEAPF32[m/4+i]; s[s.length-1]=F.mul(s[s.length-1],b); })
EM_JS(void, ffMultMatrixd, (const GLdouble* m), { const F=globalThis.__ff; const s=F.cur(); const b=new Array(16); for(let i=0;i<16;i++)b[i]=HEAPF64[m/8+i]; s[s.length-1]=F.mul(s[s.length-1],b); })
EM_JS(void, ffPushMatrix, (void), { const F=globalThis.__ff; const s=F.cur(); s.push(s[s.length-1].slice()); })
EM_JS(void, ffPopMatrix, (void), { const F=globalThis.__ff; const s=F.cur(); if(s.length>1)s.pop(); })
EM_JS(void, ffTranslate, (GLdouble x, GLdouble y, GLdouble z), { const F=globalThis.__ff; const s=F.cur(); const t=F.ident(); t[12]=x;t[13]=y;t[14]=z; s[s.length-1]=F.mul(s[s.length-1],t); })
EM_JS(void, ffScale, (GLdouble x, GLdouble y, GLdouble z), { const F=globalThis.__ff; const s=F.cur(); const t=F.ident(); t[0]=x;t[5]=y;t[10]=z; s[s.length-1]=F.mul(s[s.length-1],t); })
EM_JS(void, ffRotate, (GLdouble a, GLdouble x, GLdouble y, GLdouble z), {
  const F=globalThis.__ff; const s=F.cur(); const r=a*Math.PI/180; const c=Math.cos(r),si=Math.sin(r);
  let l=Math.hypot(x,y,z); if(l>0){x/=l;y/=l;z/=l;} const m=F.ident();
  m[0]=c+x*x*(1-c); m[1]=y*x*(1-c)+z*si; m[2]=z*x*(1-c)-y*si;
  m[4]=x*y*(1-c)-z*si; m[5]=c+y*y*(1-c); m[6]=z*y*(1-c)+x*si;
  m[8]=x*z*(1-c)+y*si; m[9]=y*z*(1-c)-x*si; m[10]=c+z*z*(1-c);
  s[s.length-1]=F.mul(s[s.length-1],m);
})
EM_JS(void, ffOrtho, (GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f), {
  const F=globalThis.__ff; const s=F.cur(); const m=F.ident();
  m[0]=2/(r-l); m[5]=2/(t-b); m[10]=-2/(f-n); m[12]=-(r+l)/(r-l); m[13]=-(t+b)/(t-b); m[14]=-(f+n)/(f-n);
  s[s.length-1]=F.mul(s[s.length-1],m);
})
EM_JS(void, ffFrustum, (GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f), {
  const F=globalThis.__ff; const s=F.cur(); const m=[0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
  m[0]=2*n/(r-l); m[5]=2*n/(t-b); m[8]=(r+l)/(r-l); m[9]=(t+b)/(t-b); m[10]=-(f+n)/(f-n); m[11]=-1; m[14]=-2*f*n/(f-n);
  s[s.length-1]=F.mul(s[s.length-1],m);
})

void glMatrixMode(GLenum m){ ensure(); ffMatrixMode(m); }
void glLoadIdentity(void){ ensure(); ffLoadIdentity(); }
void glLoadMatrixf(const GLfloat* m){ ensure(); ffLoadMatrixf(m); }
void glLoadMatrixd(const GLdouble* m){ ensure(); ffLoadMatrixd(m); }
void glMultMatrixf(const GLfloat* m){ ensure(); ffMultMatrixf(m); }
void glMultMatrixd(const GLdouble* m){ ensure(); ffMultMatrixd(m); }
void glPushMatrix(void){ ensure(); ffPushMatrix(); }
void glPopMatrix(void){ ensure(); ffPopMatrix(); }
void glTranslatef(GLfloat x,GLfloat y,GLfloat z){ ensure(); ffTranslate(x,y,z); }
void glTranslated(GLdouble x,GLdouble y,GLdouble z){ ensure(); ffTranslate(x,y,z); }
void glScalef(GLfloat x,GLfloat y,GLfloat z){ ensure(); ffScale(x,y,z); }
void glScaled(GLdouble x,GLdouble y,GLdouble z){ ensure(); ffScale(x,y,z); }
void glRotatef(GLfloat a,GLfloat x,GLfloat y,GLfloat z){ ensure(); ffRotate(a,x,y,z); }
void glRotated(GLdouble a,GLdouble x,GLdouble y,GLdouble z){ ensure(); ffRotate(a,x,y,z); }
void glOrtho(GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f){ ensure(); ffOrtho(l,r,b,t,n,f); }
void glFrustum(GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f){ ensure(); ffFrustum(l,r,b,t,n,f); }

/* ---- current color / normal / lighting ---- */
EM_JS(void, ffColor4, (GLfloat r,GLfloat g,GLfloat b,GLfloat a), { globalThis.__ff.color=[r,g,b,a]; })
EM_JS(void, ffNormal3, (GLfloat x,GLfloat y,GLfloat z), { globalThis.__ff.normal=[x,y,z]; })
EM_JS(void, ffSetLighting, (int on), { globalThis.__ff.lighting=!!on; })
EM_JS(void, ffLightDir, (GLfloat x,GLfloat y,GLfloat z), { globalThis.__ff.lightDir=[x,y,z]; })

void glColor3f(GLfloat r,GLfloat g,GLfloat b){ ensure(); ffColor4(r,g,b,1); }
void glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a){ ensure(); ffColor4(r,g,b,a); }
void glColor3fv(const GLfloat* v){ ensure(); ffColor4(v[0],v[1],v[2],1); }
void glColor4fv(const GLfloat* v){ ensure(); ffColor4(v[0],v[1],v[2],v[3]); }
void glColor3ub(GLubyte r,GLubyte g,GLubyte b){ ensure(); ffColor4(r/255.f,g/255.f,b/255.f,1); }
void glColor4ub(GLubyte r,GLubyte g,GLubyte b,GLubyte a){ ensure(); ffColor4(r/255.f,g/255.f,b/255.f,a/255.f); }
void glColor3ubv(const GLubyte* v){ ensure(); ffColor4(v[0]/255.f,v[1]/255.f,v[2]/255.f,1); }
void glColor4ubv(const GLubyte* v){ ensure(); ffColor4(v[0]/255.f,v[1]/255.f,v[2]/255.f,v[3]/255.f); }
void glNormal3f(GLfloat x,GLfloat y,GLfloat z){ ensure(); ffNormal3(x,y,z); }
void glNormal3fv(const GLfloat* v){ ensure(); ffNormal3(v[0],v[1],v[2]); }

/* Materials: use the diffuse component as the surface color. */
void glMaterialfv(GLenum face, GLenum pname, const GLfloat* params){
    ensure();
    if (pname == 0x1201 /*GL_DIFFUSE*/ || pname == 0x1602 /*GL_AMBIENT_AND_DIFFUSE*/) {
        ffColor4(params[0], params[1], params[2], params[3]);
    }
}
void glMaterialf(GLenum, GLenum, GLfloat){ }
void glMateriali(GLenum, GLenum, GLint){ }
void glColorMaterial(GLenum, GLenum){ }
void glShadeModel(GLenum){ }
void glLightf(GLenum, GLenum, GLfloat){ }
void glLighti(GLenum, GLenum, GLint){ }
void glLightModelfv(GLenum, const GLfloat*){ }
void glLightModeli(GLenum, GLint){ }
void glLightfv(GLenum light, GLenum pname, const GLfloat* params){
    ensure();
    if (pname == 0x1203 /*GL_POSITION*/) {
        ffLightDir(params[0], params[1], params[2]);
    }
}

/* ---- glEnable/glDisable: intercept legacy caps, pass real ones to WebGL ---- */
EM_JS(int, ffCap, (GLenum cap, int enable), {
  // returns 1 if handled as legacy, 0 if the real GL should handle it
  const F=globalThis.__ff; const g=F.gl();
  switch(cap){
    case 0x0B50: F.lighting=!!enable; return 1;       // GL_LIGHTING
    case 0x0B57: return 1;                             // GL_COLOR_MATERIAL
    case 0x0BA1: return 1;                             // GL_NORMALIZE
    case 0x803A: return 1;                             // GL_RESCALE_NORMAL
    case 0x0BC0: return 1;                             // GL_ALPHA_TEST
    case 0x0B10: return 1;                             // GL_POINT_SMOOTH
    case 0x0B20: return 1;                             // GL_LINE_SMOOTH
    case 0x0B24: return 1;                             // GL_LINE_STIPPLE
    case 0x0B42: return 1;                             // GL_POLYGON_STIPPLE (0x0B42? actually GL_AUTO_NORMAL) - treat legacy
    case 0x0DE1: if(g){ enable?g.disable(g.TEXTURE_2D||0):0; } return 1; // GL_TEXTURE_2D: no fixed-func textures
    case 0x2A20: return 1;                             // GL_POLYGON_OFFSET_LINE
    case 0x2A02: return 1;                             // GL_POLYGON_OFFSET_POINT
  }
  if (cap>=0x4000 && cap<=0x4007) return 1;            // GL_LIGHT0..7
  if (cap>=0x3000 && cap<=0x3007) return 1;            // GL_CLIP_PLANE0..7 (ignored for now)
  return 0;
})
/* real glEnable/glDisable are provided by emscripten; we override to filter. */
EM_JS(void, ffRealEnable, (GLenum cap, int enable), {
  const g=globalThis.__ff.gl(); if(!g) return; try{ enable?g.enable(cap):g.disable(cap); }catch(e){}
})
void glEnable(GLenum cap){ ensure(); if(!ffCap(cap,1)) ffRealEnable(cap,1); }
void glDisable(GLenum cap){ ensure(); if(!ffCap(cap,0)) ffRealEnable(cap,0); }

/* ---- client vertex arrays ---- */
EM_JS(void, ffClientState, (GLenum arr, int on), {
  const A=globalThis.__ff.arrays;
  if(arr===0x8074)A.vertex.on=!!on; else if(arr===0x8075)A.normal.on=!!on;
  else if(arr===0x8076)A.color.on=!!on;
})
EM_JS(void, ffPointer, (int which, GLint size, GLenum type, GLsizei stride, GLintptr ptr), {
  const F=globalThis.__ff; const A=F.arrays; const g=F.gl();
  const spec = which===0?A.vertex:which===1?A.normal:A.color;
  spec.size=size; spec.type=type; spec.stride=stride; spec.ptr=ptr;
  // If a real ARRAY_BUFFER is bound, the pointer is an offset into it (VBO
  // path); otherwise it is client memory. Snapshot the bound WebGLBuffer.
  spec.glbuf = g ? g.getParameter(g.ARRAY_BUFFER_BINDING) : null;
})

void glEnableClientState(GLenum a){ ensure(); ffClientState(a,1); }
void glDisableClientState(GLenum a){ ensure(); ffClientState(a,0); }
void glVertexPointer(GLint s, GLenum t, GLsizei st, const void* p){ ensure(); ffPointer(0,s,t,st,(GLintptr)p); }
void glNormalPointer(GLenum t, GLsizei st, const void* p){ ensure(); ffPointer(1,3,t,st,(GLintptr)p); }
void glColorPointer(GLint s, GLenum t, GLsizei st, const void* p){ ensure(); ffPointer(2,s,t,st,(GLintptr)p); }
void glTexCoordPointer(GLint, GLenum, GLsizei, const void*){ ensure(); }
void glInterleavedArrays(GLenum, GLsizei, const void*){ ensure(); }
void glClientActiveTexture(GLenum){ }
/* Legacy multitexture coord setters (GLES1, gone from WebGL2). No-ops: we do
 * not carry per-texture coordinates through the fixed-function emulation. */
void glMultiTexCoord2f(GLenum, GLfloat, GLfloat){ }
void glMultiTexCoord2fv(GLenum, const GLfloat*){ }
void glMultiTexCoord3fv(GLenum, const GLfloat*){ }
void glMultiTexCoord4fv(GLenum, const GLfloat*){ }

/* We must observe buffer binding to know client-vs-VBO arrays. glBindBuffer is
 * provided by emscripten; wrap by also notifying JS via the real one. We can't
 * override it (needed for real WebGL buffers), so Coin's binds go to the real
 * function AND we shadow the last ARRAY/ELEMENT binding through a hook. Since
 * we can't intercept the real glBindBuffer without a symbol clash, we snapshot
 * the binding at draw time from the GL state instead. */

/* ---- draws ----
 * CRITICAL: Qt renders its own UI with glDrawArrays/glDrawElements using its
 * own GLES2 shaders (never glEnableClientState). We must only apply
 * fixed-function emulation when Coin has a fixed-function vertex array enabled;
 * otherwise pass the draw straight through to the real WebGL2 context so Qt's
 * rendering is untouched. */
EM_JS(int, ffFixedFuncActive, (void), {
  return (globalThis.__ff && globalThis.__ff.arrays.vertex.on) ? 1 : 0;
})
EM_JS(void, ffPassDrawArrays, (GLenum mode, GLint first, GLsizei count), {
  const g=globalThis.__ff.gl(); if(g) g.drawArrays(mode, first, count);
})
EM_JS(void, ffPassDrawElements, (GLenum mode, GLsizei count, GLenum type, GLintptr indices), {
  const g=globalThis.__ff.gl(); if(g) g.drawElements(mode, count, type, indices);
})
EM_JS(void, ffDbgDraw, (int isElem, GLenum mode, GLsizei count, int ffActive), {
  if((globalThis.__ffdc=(globalThis.__ffdc||0)+1) <= 12)
    console.log('GLDRAW '+(isElem?'Elem':'Arr')+' #'+globalThis.__ffdc+' mode='+mode+' count='+count+' ffActive='+ffActive);
})
void glDrawArrays(GLenum mode, GLint first, GLsizei count){
    ensure();
    int ff = ffFixedFuncActive();
    ffDbgDraw(0, mode, count, ff);
    if (ff) ff_setup_and_draw(mode, count, 0, 0, first, 0);
    else ffPassDrawArrays(mode, first, count);
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices){
    ensure();
    int ff = ffFixedFuncActive();
    ffDbgDraw(1, mode, count, ff);
    if (ff) ff_setup_and_draw(mode, count, type, (GLintptr)indices, 0, 1);
    else ffPassDrawElements(mode, count, type, (GLintptr)indices);
}

/* ---- immediate mode ---- */
EM_JS(void, ffBegin, (GLenum mode), { const F=globalThis.__ff; F.imm={mode, verts:[], nrms:[], cols:[]}; })
EM_JS(void, ffVertex, (GLfloat x,GLfloat y,GLfloat z), {
  const F=globalThis.__ff; if(!F.imm)return;
  F.imm.verts.push(x,y,z); F.imm.nrms.push(F.normal[0],F.normal[1],F.normal[2]);
  F.imm.cols.push(F.color[0],F.color[1],F.color[2],F.color[3]);
})
EM_JS(void, ffEnd, (void), {
  const F=globalThis.__ff; const g=F.gl(); const im=F.imm; F.imm=null;
  const isKey = im && (im.mode===4 || (im.mode===7 && im.verts.length>40));
  if((isKey && (globalThis.__ffKey=(globalThis.__ffKey||0)+1)<=40) ||
     ((globalThis.__ffFrame||0) <= 2 && (globalThis.__ffie=(globalThis.__ffie||0)+1) <= 40)){
    let diag='';
    if(im && im.verts.length>=3 && g){
      const mvp=F.mul(F.pr[F.pr.length-1], F.mv[F.mv.length-1]);
      // project ALL verts to NDC, compute AABB (min/max x,y,z) — tells us if the
      // geometry actually falls inside the [-1,1] clip cube.
      let mn=[1e9,1e9,1e9], mx=[-1e9,-1e9,-1e9];
      const nv=im.verts.length/3;
      for(let i=0;i<nv;i++){
        const x=im.verts[i*3],y=im.verts[i*3+1],z=im.verts[i*3+2];
        const c=[0,0,0,0]; for(let r=0;r<4;r++)c[r]=mvp[r]*x+mvp[4+r]*y+mvp[8+r]*z+mvp[12+r];
        const w=c[3]||1; for(let k=0;k<3;k++){const n=c[k]/w; if(n<mn[k])mn[k]=n; if(n>mx[k])mx[k]=n;}
      }
      const dt=g.getParameter(g.DEPTH_TEST), dm=g.getParameter(g.DEPTH_WRITEMASK),
            df=g.getParameter(g.DEPTH_FUNC), bl=g.getParameter(g.BLEND);
      diag=' ndcMin=['+mn.map(x=>x.toFixed(2))+'] ndcMax=['+mx.map(x=>x.toFixed(2))+']'
          +' col=['+F.color.map(x=>x.toFixed(2))+'] lit='+F.lighting
          +' depthTest='+dt+' depthMask='+dm+' depthFunc='+df+' blend='+bl;
    }
    console.log('GLDRAW f'+globalThis.__ffFrame+' #'+globalThis.__ffie+' mode='+(im?im.mode:-1)+' verts='+(im?im.verts.length/3:0)+diag);
  }
  if(!im || !g || im.verts.length===0) return; if(!F.program())return;
  // Snapshot object-space geometry + material. Matrices are applied at emit time
  // (now, or on each glCallList replay) so cached geometry tracks the camera.
  const rec={mode:im.mode, verts:im.verts, nrms:im.nrms, cols:im.cols,
             color:F.color.slice(), lighting:F.lighting, lightDir:F.lightDir.slice()};
  if(F.curList!==null){
    // Recording a GL display list (Coin's shape render cache). Store the draw;
    // for GL_COMPILE (0x1300) do NOT execute now — it will render via glCallList.
    if(F.lists[F.curList]) F.lists[F.curList].push(rec);
    if(F.curListMode===0x1300) return;
  }
  F.emitImm(rec);
})

void glBegin(GLenum mode){ ensure(); ffBegin(mode); }
void glEnd(void){ ensure(); ffEnd(); }
void glVertex2f(GLfloat x,GLfloat y){ ensure(); ffVertex(x,y,0); }
void glVertex2fv(const GLfloat* v){ ensure(); ffVertex(v[0],v[1],0); }
void glVertex2i(GLint x,GLint y){ ensure(); ffVertex((GLfloat)x,(GLfloat)y,0); }
void glVertex2s(short x,short y){ ensure(); ffVertex((GLfloat)x,(GLfloat)y,0); }
void glVertex3f(GLfloat x,GLfloat y,GLfloat z){ ensure(); ffVertex(x,y,z); }
void glVertex3fv(const GLfloat* v){ ensure(); ffVertex(v[0],v[1],v[2]); }
void glVertex4fv(const GLfloat* v){ ensure(); ffVertex(v[0],v[1],v[2]); }
void glTexCoord2f(GLfloat,GLfloat){ }
void glTexCoord2fv(const GLfloat*){ }
void glTexCoord3f(GLfloat,GLfloat,GLfloat){ }
void glTexCoord3fv(const GLfloat*){ }
void glTexCoord4f(GLfloat,GLfloat,GLfloat,GLfloat){ }
void glTexCoord4fv(const GLfloat*){ }

/* ---- remaining legacy no-ops (unsupported paths) ---- */
void glTexGeni(GLenum, GLenum, GLint){ }
void glTexGenf(GLenum, GLenum, GLfloat){ }
void glTexGend(GLenum, GLenum, GLdouble){ }
void glTexGenfv(GLenum, GLenum, const GLfloat*){ }
void glTexGendv(GLenum, GLenum, const GLdouble*){ }
void glAlphaFunc(GLenum, GLfloat){ }
void glFogf(GLenum, GLfloat){ }
void glFogfv(GLenum, const GLfloat*){ }
void glFogi(GLenum, GLint){ }
void glTexEnvi(GLenum, GLenum, GLint){ }
void glTexEnvf(GLenum, GLenum, GLfloat){ }
void glTexEnvfv(GLenum, GLenum, const GLfloat*){ }
void glGetTexGeniv(GLenum, GLenum, GLint* p){ if(p)p[0]=0; }
void glGetDoublev(GLenum, GLdouble* p){ if(p)p[0]=0; }
void glGetTexLevelParameteriv(GLenum, GLint, GLenum, GLint* p){ if(p)p[0]=0; }
void glAccum(GLenum, GLfloat){ }
void glClipPlane(GLenum, const GLdouble*){ }
void glIndexi(GLint){ }
void glPolygonMode(GLenum, GLenum){ }
void glPushAttrib(GLenum){ }
void glPopAttrib(void){ }
void glPushClientAttrib(GLenum){ }
void glPopClientAttrib(void){ }
void glLineStipple(GLint, unsigned short){ }
void glPolygonStipple(const GLubyte*){ }
void glRasterPos2f(GLfloat,GLfloat){ }
void glRasterPos3f(GLfloat,GLfloat,GLfloat){ }
void glRasterPos3fv(const GLfloat*){ }
void glBitmap(GLint,GLint,GLfloat,GLfloat,GLfloat,GLfloat,const GLubyte*){ }
void glDrawPixels(GLint,GLint,GLenum,GLenum,const void*){ }
void glClearIndex(GLfloat){ }
void glDrawBuffer(GLenum){ }
void glPixelMapfv(GLenum,GLint,const GLfloat*){ }
void glPixelMapuiv(GLenum,GLint,const unsigned int*){ }
void glPixelTransferf(GLenum,GLfloat){ }
void glPixelTransferi(GLenum,GLint){ }
void glPixelZoom(GLfloat,GLfloat){ }
void glPointSize(GLfloat){ }
void glLoadName(GLuint){ }
/* GL display lists: Coin caches shape geometry into a list on one frame and
 * replays it via glCallList on later frames. Real display lists don't exist in
 * WebGL2, so we record the immediate-mode primitives emitted during
 * glNewList..glEndList and re-emit them on glCallList (see ffEnd/emitImm).
 * Without this, cached geometry renders once then vanishes. */
EM_JS(GLuint, ffGenLists, (GLsizei n), {
  const F=globalThis.__ff; if(!F||n<=0) return 0;
  const base=(F._listCtr||0)+1; F._listCtr=base+n-1;
  for(let i=0;i<n;i++) F.lists[base+i]=[];
  return base;
})
EM_JS(void, ffNewList, (GLuint id, GLenum mode), {
  const F=globalThis.__ff; if(!F) return; F.curList=id; F.curListMode=mode; F.lists[id]=[];
  globalThis.__ffNL=(globalThis.__ffNL||0)+1;
})
EM_JS(void, ffEndList, (void), {
  const F=globalThis.__ff; if(!F) return;
  if((globalThis.__ffELdbg=(globalThis.__ffELdbg||0)+1)<=8)
    console.log('ENDLIST id='+F.curList+' recs='+(F.lists[F.curList]?F.lists[F.curList].length:-1));
  F.curList=null;
})
EM_JS(void, ffCallList, (GLuint id), {
  const F=globalThis.__ff; if(!F) return; globalThis.__ffCL=(globalThis.__ffCL||0)+1;
  const recs=F.lists[id];
  if((globalThis.__ffCLdbg=(globalThis.__ffCLdbg||0)<=8?(globalThis.__ffCLdbg||0)+1:9)<=8)
    console.log('CALLLIST id='+id+' recs='+(recs?recs.length:-1));
  if(!recs) return;
  for(let i=0;i<recs.length;i++) F.emitImm(recs[i]);
})
EM_JS(void, ffDeleteLists, (GLuint id, GLsizei n), {
  const F=globalThis.__ff; if(!F) return; for(let i=0;i<n;i++) delete F.lists[id+i];
})
GLuint glGenLists(GLsizei n){ ensure(); return ffGenLists(n); }
void glNewList(GLuint id, GLenum mode){ ensure(); ffNewList(id, mode); }
void glEndList(void){ ensure(); ffEndList(); }
void glCallList(GLuint id){ ensure(); ffCallList(id); }
void glCallLists(GLsizei, GLenum, const void*){ }
void glDeleteLists(GLuint id, GLsizei n){ ensure(); ffDeleteLists(id, n); }
void glListBase(GLuint){ }

/* Resolver so Coin's cc_glglue_getprocaddress can find the legacy GL entry
 * points we emulate (glActiveTexture is real WebGL2 and comes from emscripten,
 * but glClientActiveTexture, the glMultiTexCoord family, matrix and
 * immediate-mode calls are ours and are not in emscripten's GL name table).
 * Returning non-null for the whole
 * multitexture set keeps Coin from disabling — and then blindly calling — the
 * multitexture path. */
#include <cstring>
void* fcWasmResolveGL(const char* name){
    if(!name) return nullptr;
    struct E { const char* n; void* p; };
    static const E tbl[] = {
        {"glClientActiveTexture", (void*)glClientActiveTexture},
        {"glMultiTexCoord2f",  (void*)glMultiTexCoord2f},
        {"glMultiTexCoord2fv", (void*)glMultiTexCoord2fv},
        {"glMultiTexCoord3fv", (void*)glMultiTexCoord3fv},
        {"glMultiTexCoord4fv", (void*)glMultiTexCoord4fv},
        {"glMatrixMode",(void*)glMatrixMode},{"glLoadMatrixf",(void*)glLoadMatrixf},
        {"glMultMatrixf",(void*)glMultMatrixf},{"glPushMatrix",(void*)glPushMatrix},
        {"glPopMatrix",(void*)glPopMatrix},{"glVertexPointer",(void*)glVertexPointer},
        {"glNormalPointer",(void*)glNormalPointer},{"glColorPointer",(void*)glColorPointer},
        {"glEnableClientState",(void*)glEnableClientState},{"glDisableClientState",(void*)glDisableClientState},
        {"glTexCoordPointer",(void*)glTexCoordPointer},{"glInterleavedArrays",(void*)glInterleavedArrays},
        // The main render path: Coin resolves these via getprocaddress. Routing
        // them to our fixed-function wrappers is what makes geometry actually
        // draw (and keeps ff_setup_and_draw from being dead-code-eliminated).
        {"glDrawArrays",(void*)glDrawArrays},{"glDrawElements",(void*)glDrawElements},
        {"glColor3f",(void*)glColor3f},{"glColor4f",(void*)glColor4f},
        {"glColor3fv",(void*)glColor3fv},{"glColor4fv",(void*)glColor4fv},
        {"glNormal3f",(void*)glNormal3f},{"glNormal3fv",(void*)glNormal3fv},
        {"glLoadMatrixd",(void*)glLoadMatrixd},{"glMultMatrixd",(void*)glMultMatrixd},
        {"glLoadIdentity",(void*)glLoadIdentity},{"glTranslated",(void*)glTranslated},
        {"glMaterialfv",(void*)glMaterialfv},{"glLightfv",(void*)glLightfv},
        // Display lists: Coin probes these via getprocaddress to decide whether GL
        // render caching is available. Exposing them makes Coin cache shape
        // geometry through glNewList/glCallList, which we record & replay (see
        // ffEnd/emitImm) — required so geometry persists past the first frame.
        {"glGenLists",(void*)glGenLists},{"glNewList",(void*)glNewList},
        {"glEndList",(void*)glEndList},{"glCallList",(void*)glCallList},
        {"glCallLists",(void*)glCallLists},{"glDeleteLists",(void*)glDeleteLists},
        {"glListBase",(void*)glListBase},
    };
    for (const auto& e : tbl) if (std::strcmp(e.n, name)==0) return e.p;
    return nullptr;
}

/* EGL platform stubs (Qt owns the context) */
typedef void* EGLDisplay; typedef void* EGLSurface; typedef void* EGLConfig; typedef int EGLenum; typedef int EGLint; typedef unsigned int EGLBoolean;
EGLDisplay eglGetPlatformDisplay(EGLenum, void*, const EGLint*){ return (EGLDisplay)0; }
EGLSurface eglCreatePlatformWindowSurface(EGLDisplay, EGLConfig, void*, const EGLint*){ return (EGLSurface)0; }
EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay, EGLConfig, void*, const EGLint*){ return (EGLSurface)0; }
EGLBoolean eglBindTexImage(EGLDisplay, EGLSurface, EGLint){ return 0; }
EGLBoolean eglReleaseTexImage(EGLDisplay, EGLSurface, EGLint){ return 0; }

}  // extern "C"
