import { useContext, useEffect, useId } from 'react';
import { ARSceneContext } from './ARSceneContext';
import type { ARObjectDescriptor } from './ARView.nitro';
import { resolveAsset, type ARAssetSource } from './resolveAsset';

// The public `texture` accepts idiomatic RN sources (require()/{uri}/string);
// it's normalized to a URI or bundled name before reaching native.
type ARObjectProps = Omit<ARObjectDescriptor, 'id' | 'texture'> & {
  texture?: ARAssetSource;
};

export function ARObject(props: ARObjectProps) {
  const id = useId();
  const scene = useContext(ARSceneContext);
  const texture = resolveAsset(props.texture);

  useEffect(() => {
    if (!scene) return;
    const desc: ARObjectDescriptor = { id, ...props, texture };
    scene.registerObject(desc);
    return () => scene.unregisterObject(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!scene) return;
    scene.updateObject({ id, ...props, texture });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [
    scene,
    id,
    props.anchorId,
    props.model,
    texture,
    props.scale?.x,
    props.scale?.y,
    props.scale?.z,
    props.rotation?.x,
    props.rotation?.y,
    props.rotation?.z,
    props.rotation?.w,
    props.color?.r,
    props.color?.g,
    props.color?.b,
    props.color?.a,
    props.visible,
  ]);

  return null;
}
