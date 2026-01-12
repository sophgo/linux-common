
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_edid.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>

#include "ms912x.h"
#include "usb_hal_interface.h"

#define EDID_BLOCK_SIZE 128

static int ms912x_read_edid(void *data, u8 *buf, unsigned int block, size_t len)
{
	struct ms912x_device *ms912x = data;
	int ret;
	ret = usb_hal_get_edid(ms912x->hal, block, buf, len);
	if (ret)
		return ret;

	return 0;
}

/**
 * ms912x_get_edid - read EDID from MS912x device and assemble into struct edid
 * @ms912x: pointer to the device
 *
 * Returns a pointer to a newly allocated struct edid on success, NULL on failure.
 * Caller must kfree() the returned pointer.
 */
static struct edid *ms912x_get_edid(struct ms912x_device *ms912x)
{
	struct edid *edid = NULL;
	u8 buf[EDID_BLOCK_SIZE];
	int i, blocks;
	int ret;

	if (!ms912x || !ms912x->is_enable)
		return NULL;

	/* Read block 0 (base EDID) */
	ret = ms912x_read_edid(ms912x, buf, 0, EDID_BLOCK_SIZE);
	if (ret)
		return NULL;

	/* Determine number of blocks from byte 0x7e of base block */
	blocks = buf[0x7e] + 1; /* 1 base + n extension blocks */

	/* Allocate enough memory for all blocks */
	edid = kzalloc(blocks * EDID_BLOCK_SIZE, GFP_KERNEL);
	if (!edid)
		return NULL;

	/* Copy base block */
	memcpy(edid, buf, EDID_BLOCK_SIZE);

	/* Read extension blocks if any */
	for (i = 1; i < blocks; i++) {
		ret = ms912x_read_edid(ms912x, buf, i, EDID_BLOCK_SIZE);
		if (ret) {
			kfree(edid);
			return NULL;
		}
		memcpy((u8 *)edid + i * EDID_BLOCK_SIZE, buf, EDID_BLOCK_SIZE);
	}

	return edid;
}

static int ms912x_connector_get_modes(struct drm_connector *connector)
{
	int ret;
	struct ms912x_device *ms912x = to_ms912x(connector->dev);
	struct edid *edid;
	edid = ms912x_get_edid(ms912x);
	if (!edid)
		return 0;
	drm_connector_update_edid_property(connector, edid);
	ret = drm_add_edid_modes(connector, edid);
	kfree(edid);
	return ret;
}

static enum drm_connector_status ms912x_detect(struct drm_connector *connector,
					       bool force)
{
	struct ms912x_device *ms912x = to_ms912x(connector->dev);
	u32 status;
	int ret;

	ret = usb_hal_get_hpd_status(ms912x->hal, &status);
	if (ret)
		return connector_status_unknown;

	return status == 1 ? connector_status_connected :
			     connector_status_disconnected;
}

static void ms9132_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_helper_funcs ms912x_connector_helper_funcs = {
	.get_modes = ms912x_connector_get_modes,
};

static const struct drm_connector_funcs ms912x_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = ms9132_connector_destroy,
	.detect = ms912x_detect,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

int ms912x_connector_init(struct ms912x_device *ms912x)
{
	int ret;
	drm_connector_helper_add(&ms912x->connector,
				 &ms912x_connector_helper_funcs);
	ret = drm_connector_init(&ms912x->drm, &ms912x->connector,
				 &ms912x_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	ms912x->connector.polled = DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;
	return ret;
}