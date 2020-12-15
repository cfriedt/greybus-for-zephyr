#ifndef SUBSYS_GREYBUS_PLATFORM_CERTIFICATE_H_
#define SUBSYS_GREYBUS_PLATFORM_CERTIFICATE_H_

#include <net/tls_credentials.h>

enum greybus_tls_certificate_type {
	GB_TLS_CA_CERT_TAG,
	GB_TLS_SERVER_CERT_TAG,
	GB_TLS_SERVER_PRIVKEY_TAG,
	GB_TLS_AUTH_KEY_TAG,
};

int greybus_tls_init(void);

#endif /* SUBSYS_GREYBUS_PLATFORM_CERTIFICATE_H_ */
