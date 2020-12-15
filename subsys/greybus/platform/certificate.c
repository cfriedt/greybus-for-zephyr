#include <zephyr.h>
#include "certificate.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(greybus_tls, CONFIG_GREYBUS_LOG_LEVEL);

#ifdef CONFIG_GREYBUS_TLS_BUILTIN
static const unsigned char greybus_tls_builtin_ca_cert[] = {
#include "greybus_tls_builtin_ca_cert.inc"
};

static const unsigned char greybus_tls_builtin_server_cert[] = {
#include "greybus_tls_builtin_server_cert.inc"
};

static const unsigned char greybus_tls_builtin_server_privkey[] = {
#include "greybus_tls_builtin_server_privkey.inc"
};

static const unsigned char greybus_tls_builtin_authorized_keys[] = {
#include "greybus_tls_builtin_authorized_keys.inc"
};
#endif /* GREYBUS_TLS_BUILTIN */

int greybus_tls_init(void)
{
	if (IS_ENABLED(CONFIG_GREYBUS_TLS_BUILTIN)) {
		int r;

		LOG_INF("Initializing built-in certificates");

		r = tls_credential_add(GB_TLS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				greybus_tls_builtin_ca_cert, sizeof(greybus_tls_builtin_ca_cert));
		if (r < 0) {
			LOG_ERR("Failed to add CA Certificate: %d", r);
			return r;
		}

		r = tls_credential_add(GB_TLS_SERVER_CERT_TAG, TLS_CREDENTIAL_SERVER_CERTIFICATE,
				greybus_tls_builtin_server_cert, sizeof(greybus_tls_builtin_server_cert));
		if (r < 0) {
			LOG_ERR("Failed to add Server Certificate: %d", r);
			return r;
		}

		r = tls_credential_add(GB_TLS_SERVER_PRIVKEY_TAG, TLS_CREDENTIAL_PRIVATE_KEY,
				greybus_tls_builtin_server_privkey, sizeof(greybus_tls_builtin_server_privkey));
		if (r < 0) {
			LOG_ERR("Failed to add Server Certificate: %d", r);
			return r;
		}

		r = tls_credential_add(GB_TLS_AUTH_KEY_TAG, TLS_CREDENTIAL_NONE,
				greybus_tls_builtin_authorized_keys, sizeof(greybus_tls_builtin_authorized_keys));
		if (r < 0) {
			LOG_ERR("Failed to add Authorized Keys: %d", r);
			return r;
		}
	}

	return 0;
}
