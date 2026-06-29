import { useContext, useEffect, useId } from 'react';
import { ARSceneContext, type ARObjectDescriptor } from './ARSceneContext';

type ARObjectProps = Omit<ARObjectDescriptor, 'id'>;

export function ARObject(props: ARObjectProps) {
  const id = useId();
  const scene = useContext(ARSceneContext);

  useEffect(() => {
    if (!scene) return;
    const desc: ARObjectDescriptor = { id, ...props };
    scene.registerObject(desc);
    return () => scene.unregisterObject(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!scene) return;
    scene.updateObject({ id, ...props });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [
    scene,
    id,
    props.anchorId,
    props.model,
    props.texture,
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
