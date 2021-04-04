/* felipe@astroza.cl - 2021
 * under MIT license
*/

#ifndef __VEHICLE_H__
#define __VEHICLE_H__

#define CONTROL_PORT 4000
#define CONTROL_ADDR "127.0.0.1"

struct vehicle_state {
	signed char steering;
	signed char wheels;
};

#endif
