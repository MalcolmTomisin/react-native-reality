import { Image } from 'react-native';

/**
 * An asset source accepted by the library's asset props, matching the idiomatic
 * React Native forms:
 *  - `require('./tex.png')` — a Metro-registered asset (an opaque number)
 *  - `{ uri: 'https://…' | 'file://…' | 'data:…' }` — a remote/local/data URI
 *  - a plain string — a bundled asset name or a literal URI
 */
export type ARAssetSource = number | { uri: string } | string;

/**
 * Normalizes an {@link ARAssetSource} to a plain string the native layer can
 * load — a URI (http/https/file/data) or a bundled asset name.
 *
 * `require()` numbers and `{ uri }` objects are resolved via
 * `Image.resolveAssetSource` (in dev this yields the Metro dev-server URL; in a
 * release build a `file://` path on iOS or a drawable resource name on Android).
 * Plain strings are passed through unchanged. Returns `undefined` for a missing
 * or unresolvable source.
 */
export function resolveAsset(source?: ARAssetSource): string | undefined {
  if (source == null) return undefined;
  if (typeof source === 'string') return source;
  const resolved = Image.resolveAssetSource(source as number);
  return resolved?.uri;
}
