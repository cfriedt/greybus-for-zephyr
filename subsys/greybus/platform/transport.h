#ifndef GREYBUS_TRANSPORT_H_
#define GREYBUS_TRANSPORT_H_

#include <greybus/greybus.h>

struct gb_transport_backend *gb_transport_backend_init(unsigned int *cports, size_t num_cports);
const struct gb_transport_backend *gb_transport_get_backend(void);

#endif /* GREYBUS_TRANSPORT_H_ */
