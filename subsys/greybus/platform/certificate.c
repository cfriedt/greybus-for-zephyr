#include <zephyr.h>
#include "certificate.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_tls, CONFIG_GREYBUS_LOG_LEVEL);

#ifdef CONFIG_GREYBUS_TLS_BUILTIN
#if defined(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL) \
		|| defined(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)
static const unsigned char greybus_tls_builtin_ca_cert[] = {
#include "greybus_tls_builtin_ca_cert.inc"
};
#else /* CONFIG_GREYBUS_TLS_TLS_CLIENT_VERIFY.. */
#define greybus_tls_builtin_ca_cert NULL
#endif /* CONFIG_GREYBUS_TLS_TLS_CLIENT_VERIFY.. */

static const unsigned char greybus_tls_builtin_server_cert[] = {
#include "greybus_tls_builtin_server_cert.inc"
};

static const unsigned char greybus_tls_builtin_server_privkey[] = {
#include "greybus_tls_builtin_server_privkey.inc"
};
#else
#define greybus_tls_builtin_ca_cert NULL
#define greybus_tls_builtin_server_cert NULL
#define greybus_tls_builtin_server_privkey NULL
#endif /* GREYBUS_TLS_BUILTIN */

int greybus_tls_init(void)
{
	if (IS_ENABLED(CONFIG_GREYBUS_TLS_BUILTIN)) {
		int r;

		LOG_INF("Initializing built-in certificates");

		if (IS_ENABLED(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL)
			|| IS_ENABLED(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)) {
			LOG_DBG("Adding CA Certificate (%zu bytes)", sizeof(greybus_tls_builtin_ca_cert));
			r = tls_credential_add(GB_TLS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
					greybus_tls_builtin_ca_cert, sizeof(greybus_tls_builtin_ca_cert));
			if (r < 0) {
				LOG_ERR("Failed to add CA Certificate: %d", r);
				return r;
			}
		}

		LOG_DBG("Adding Server Certificate (Public Key) (%zu bytes)", sizeof(greybus_tls_builtin_server_cert));
		r = tls_credential_add(GB_TLS_SERVER_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE,
				greybus_tls_builtin_server_cert, sizeof(greybus_tls_builtin_server_cert));
		if (r < 0) {
			LOG_ERR("Failed to add Server Certificate (Public Key): %d", r);
			return r;
		}

		LOG_DBG("Adding Server Certificate (Private Key) (%zu bytes)", sizeof(greybus_tls_builtin_server_privkey));
		r = tls_credential_add(GB_TLS_SERVER_CERT_TAG, TLS_CREDENTIAL_PRIVATE_KEY,
				greybus_tls_builtin_server_privkey, sizeof(greybus_tls_builtin_server_privkey));
		if (r < 0) {
			LOG_ERR("Failed to add Server Certificate (Private Key): %d", r);
			return r;
		}
	}

	return 0;
}
