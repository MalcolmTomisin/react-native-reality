#include "ARKitAvailability.h"
#import <ARKit/ARKit.h>

namespace arcore {
bool isWorldTrackingSupported() {
    return [ARWorldTrackingConfiguration isSupported];
}
}
