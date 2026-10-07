/** Frees the GPU resources of a material, including every texture it holds (map, normalMap, shader uniforms, ...). */
export function disposeMaterial(m) {
  for (const v of Object.values(m)) if (v?.isTexture) v.dispose();
  if (m.uniforms) for (const u of Object.values(m.uniforms)) if (u?.value?.isTexture) u.value.dispose();
  m.dispose();
}

/** Frees geometries, instanced buffers and materials of an object tree. */
export function disposeTree(root) {
  root.traverse((o) => {
    o.geometry?.dispose?.(); if (o.isInstancedMesh) o.dispose();
    if (o.material) (Array.isArray(o.material) ? o.material : [o.material]).forEach(disposeMaterial);
  });
}
