import { useContext, useEffect, useId } from 'react';
import { ARFaceSceneContext } from './ARFaceSceneContext';
import type { ARFaceFilterDescriptor } from './ARView.nitro';

type ARFaceFilterProps = Omit<ARFaceFilterDescriptor, 'id'>;

export function ARFaceFilter(props: ARFaceFilterProps) {
  const id = useId();
  const scene = useContext(ARFaceSceneContext);

  useEffect(() => {
    if (!scene) return;
    const desc: ARFaceFilterDescriptor = { id, ...props };
    scene.registerFilter(desc);
    return () => scene.unregisterFilter(id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!scene) return;
    scene.updateFilter({ id, ...props });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [
    scene,
    id,
    props.attachmentPoint,
    props.model,
    props.scale?.x,
    props.scale?.y,
    props.scale?.z,
    props.offset?.x,
    props.offset?.y,
    props.offset?.z,
    props.rotation?.x,
    props.rotation?.y,
    props.rotation?.z,
    props.visible,
  ]);

  return null;
}
