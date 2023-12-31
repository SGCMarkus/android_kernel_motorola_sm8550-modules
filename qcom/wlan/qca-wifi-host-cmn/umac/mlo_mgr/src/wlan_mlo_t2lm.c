/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: contains T2LM APIs
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_mlo_t2lm.h>
#include <wlan_mlo_mgr_cmn.h>
#include <qdf_util.h>
#include <wlan_cm_api.h>

/**
 * wlan_mlo_parse_t2lm_info() - Parse T2LM IE fields
 * @ie: Pointer to T2LM IE
 * @t2lm: Pointer to T2LM structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_parse_t2lm_info(uint8_t *ie,
					   struct wlan_t2lm_info *t2lm)
{
	struct wlan_ie_tid_to_link_mapping *t2lm_ie;
	enum wlan_t2lm_direction dir;
	uint8_t *t2lm_control_field;
	uint16_t t2lm_control;
	uint8_t link_mapping_presence_ind;
	uint8_t *link_mapping_of_tids;
	uint8_t tid_num;
	uint8_t *ie_ptr = NULL;

	t2lm_ie = (struct wlan_ie_tid_to_link_mapping *)ie;

	t2lm_control_field = t2lm_ie->data;
	if (!t2lm_control_field) {
		t2lm_err("t2lm_control_field is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_control = qdf_le16_to_cpu(*(uint16_t *)t2lm_control_field);

	dir = QDF_GET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DIRECTION_IDX,
			   WLAN_T2LM_CONTROL_DIRECTION_BITS);
	if (dir > WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Invalid direction");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm->direction = dir;
	t2lm->default_link_mapping =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_IDX,
			     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_BITS);

	t2lm->mapping_switch_time_present =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX,
			     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_BITS);

	t2lm->expected_duration_present =
		QDF_GET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_IDX,
			     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_BITS);

	t2lm_debug("direction:%d default_link_mapping:%d mapping_switch_time_present:%d expected_duration_present:%d",
		   t2lm->direction, t2lm->default_link_mapping,
		    t2lm->mapping_switch_time_present,
		    t2lm->expected_duration_present);

	if (t2lm->default_link_mapping) {
		ie_ptr = t2lm_control_field + sizeof(uint8_t);
	} else {
		link_mapping_presence_ind =
			QDF_GET_BITS(t2lm_control,
				     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX,
				     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_BITS);
		ie_ptr = t2lm_control_field + sizeof(t2lm_control);
	}

	if (t2lm->mapping_switch_time_present) {
		t2lm->mapping_switch_time =
			qdf_le16_to_cpu(*(uint16_t *)ie_ptr);
		ie_ptr += sizeof(uint16_t);
	}

	if (t2lm->expected_duration_present) {
		qdf_mem_copy(&t2lm->expected_duration, ie_ptr,
			     WLAN_T2LM_EXPECTED_DURATION_SIZE *
			     (sizeof(uint8_t)));
		ie_ptr += WLAN_T2LM_EXPECTED_DURATION_SIZE * (sizeof(uint8_t));
	}

	t2lm_debug("direction:%d default_link_mapping:%d mapping_switch_time:%d expected_duration:%d",
		   t2lm->direction, t2lm->default_link_mapping,
		   t2lm->mapping_switch_time, t2lm->expected_duration);

	if (t2lm->default_link_mapping)
		return QDF_STATUS_SUCCESS;

	link_mapping_of_tids = ie_ptr;

	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		if (!(link_mapping_presence_ind & BIT(tid_num)))
			continue;

		t2lm->ieee_link_map_tid[tid_num] =
		    qdf_le16_to_cpu(*(uint16_t *)link_mapping_of_tids);

		t2lm_debug("link mapping of TID%d is %x", tid_num,
			   t2lm->ieee_link_map_tid[tid_num]);

		link_mapping_of_tids += sizeof(uint16_t);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_parse_bcn_prbresp_t2lm_ie(
		struct wlan_t2lm_context *t2lm_ctx, uint8_t *ie)
{
	struct extn_ie_header *ext_ie_hdr;
	QDF_STATUS retval;
	int i = 0;

	for (i = 0; i < t2lm_ctx->num_of_t2lm_ie; i++) {
		qdf_mem_zero(&t2lm_ctx->t2lm_ie[i].t2lm,
			     sizeof(struct wlan_t2lm_info));
		t2lm_ctx->t2lm_ie[i].t2lm.direction =
			WLAN_T2LM_INVALID_DIRECTION;
	}

	t2lm_ctx->num_of_t2lm_ie = 0;

	for (i = 0; i < WLAN_MAX_T2LM_IE; i++) {
		if (!ie) {
			t2lm_err("ie is null");
			return QDF_STATUS_E_NULL_VALUE;
		}

		ext_ie_hdr = (struct extn_ie_header *)ie;

		if (ext_ie_hdr->ie_id == WLAN_ELEMID_EXTN_ELEM &&
		    ext_ie_hdr->ie_extn_id == WLAN_EXTN_ELEMID_T2LM) {
			retval = wlan_mlo_parse_t2lm_info(
					ie, &t2lm_ctx->t2lm_ie[i].t2lm);
			if (retval) {
				t2lm_err("Failed to parse the T2LM IE");
				return retval;
			}

			t2lm_ctx->num_of_t2lm_ie++;

			ie += ext_ie_hdr->ie_len + sizeof(struct ie_header);
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_parse_t2lm_ie(
		struct wlan_t2lm_onging_negotiation_info *t2lm, uint8_t *ie)
{
	struct extn_ie_header *ext_ie_hdr = NULL;
	QDF_STATUS retval;
	enum wlan_t2lm_direction dir;
	struct wlan_t2lm_info t2lm_info;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++)
		t2lm->t2lm_info[dir].direction = WLAN_T2LM_INVALID_DIRECTION;

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (!ie) {
			t2lm_err("ie is null");
			return QDF_STATUS_E_NULL_VALUE;
		}

		ext_ie_hdr = (struct extn_ie_header *)ie;

		if (ext_ie_hdr->ie_id == WLAN_ELEMID_EXTN_ELEM &&
		    ext_ie_hdr->ie_extn_id == WLAN_EXTN_ELEMID_T2LM) {
			qdf_mem_zero(&t2lm_info, sizeof(t2lm_info));
			retval = wlan_mlo_parse_t2lm_info(ie, &t2lm_info);
			if (!retval &&
			    t2lm_info.direction < WLAN_T2LM_MAX_DIRECTION) {
				qdf_mem_copy(&t2lm->t2lm_info[t2lm_info.direction],
					     &t2lm_info,
					     sizeof(struct wlan_t2lm_info));
			} else {
				t2lm_err("Failed to parse the T2LM IE");
				return retval;
			}
			ie += ext_ie_hdr->ie_len + sizeof(struct ie_header);
		}
	}

	if ((t2lm->t2lm_info[WLAN_T2LM_DL_DIRECTION].direction ==
			WLAN_T2LM_DL_DIRECTION ||
	     t2lm->t2lm_info[WLAN_T2LM_UL_DIRECTION].direction ==
			WLAN_T2LM_UL_DIRECTION) &&
	    t2lm->t2lm_info[WLAN_T2LM_BIDI_DIRECTION].direction ==
			WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Both DL/UL and BIDI T2LM IEs should not be present at the same time");

		qdf_mem_zero(t2lm, sizeof(*t2lm));
		for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
			t2lm->t2lm_info[dir].direction =
			    WLAN_T2LM_INVALID_DIRECTION;
		}

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlo_add_t2lm_info_ie() - Add T2LM IE for UL/DL/Bidirection
 * @frm: Pointer to buffer
 * @t2lm: Pointer to t2lm mapping structure
 *
 * Return: Updated frame pointer
 */
uint8_t *wlan_mlo_add_t2lm_info_ie(uint8_t *frm, struct wlan_t2lm_info *t2lm)
{
	struct wlan_ie_tid_to_link_mapping *t2lm_ie;
	uint16_t t2lm_control = 0;
	uint8_t *t2lm_control_field;
	uint8_t *link_mapping_of_tids;
	uint8_t tid_num;
	uint8_t num_tids = 0;
	uint8_t link_mapping_presence_indicator = 0;

	t2lm_ie = (struct wlan_ie_tid_to_link_mapping *)frm;
	t2lm_ie->elem_id = WLAN_ELEMID_EXTN_ELEM;
	t2lm_ie->elem_id_extn = WLAN_EXTN_ELEMID_T2LM;

	t2lm_ie->elem_len = sizeof(*t2lm_ie) - sizeof(struct ie_header);

	t2lm_control_field = t2lm_ie->data;

	QDF_SET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DIRECTION_IDX,
		     WLAN_T2LM_CONTROL_DIRECTION_BITS, t2lm->direction);

	QDF_SET_BITS(t2lm_control, WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_IDX,
		     WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_BITS,
		     t2lm->default_link_mapping);

	QDF_SET_BITS(t2lm_control,
		     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX,
		     WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_BITS,
		     t2lm->mapping_switch_time_present);

	QDF_SET_BITS(t2lm_control,
		     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_IDX,
		     WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_BITS,
		     t2lm->expected_duration_present);

	if (t2lm->default_link_mapping) {
		/* Link mapping of TIDs are not present when default mapping is
		 * set. Hence, the size of TID-To-Link mapping control is one
		 * octet.
		 */
		*t2lm_control_field = (uint8_t)t2lm_control;

		t2lm_ie->elem_len += sizeof(uint8_t);

		t2lm_rl_debug("T2LM IE added, default_link_mapping: %d dir:%d",
			      t2lm->default_link_mapping, t2lm->direction);

		frm += sizeof(*t2lm_ie) + sizeof(uint8_t);
	} else {
		for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++)
			if (t2lm->hw_link_map_tid[tid_num])
				link_mapping_presence_indicator |= BIT(tid_num);

		QDF_SET_BITS(t2lm_control,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX,
			     WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_BITS,
			     link_mapping_presence_indicator);
		t2lm_rl_debug("T2LM IE added, direction:%d link_mapping_presence_indicator:%x",
			      t2lm->direction, link_mapping_presence_indicator);

		/* The size of TID-To-Link mapping control is two octets when
		 * default link mapping is not set.
		 */
		*(uint16_t *)t2lm_control_field = htole16(t2lm_control);
		frm += sizeof(*t2lm_ie) + sizeof(uint16_t);
		t2lm_ie->elem_len += sizeof(uint16_t);
	}

	if (t2lm->mapping_switch_time_present) {
		*frm = htole16(t2lm->mapping_switch_time);
		frm += sizeof(uint16_t);
		t2lm_ie->elem_len += sizeof(uint16_t);
	}

	if (t2lm->expected_duration_present) {
		qdf_mem_copy(frm, &t2lm->expected_duration,
			     WLAN_T2LM_EXPECTED_DURATION_SIZE *
			     sizeof(uint8_t));
		frm += WLAN_T2LM_EXPECTED_DURATION_SIZE;
		t2lm_ie->elem_len +=
			WLAN_T2LM_EXPECTED_DURATION_SIZE * sizeof(uint8_t);
	}

	t2lm_rl_debug("mapping_switch_time:%d expected_duration:%u",
		      t2lm->mapping_switch_time, t2lm->expected_duration);

	if (t2lm->default_link_mapping)
		return frm;

	link_mapping_of_tids = frm;

	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		if (!t2lm->ieee_link_map_tid[tid_num])
			continue;

		*(uint16_t *)link_mapping_of_tids =
			htole16(t2lm->ieee_link_map_tid[tid_num]);
		t2lm_rl_debug("link mapping of TID%d is %x", tid_num,
			      htole16(t2lm->ieee_link_map_tid[tid_num]));
		link_mapping_of_tids += sizeof(uint16_t);
		num_tids++;
	}

	frm += num_tids * sizeof(uint16_t);
	t2lm_ie->elem_len += (num_tids * sizeof(uint16_t));

	return frm;
}

uint8_t *wlan_mlo_add_t2lm_ie(uint8_t *frm,
			      struct wlan_t2lm_onging_negotiation_info *t2lm)
{
	uint8_t dir;

	if (!frm) {
		t2lm_err("frm is null");
		return NULL;
	}

	if (!t2lm) {
		t2lm_err("t2lm is null");
		return NULL;
	}

	/* As per spec, the frame should include one or two T2LM IEs. When it is
	 * two, then direction should DL and UL.
	 */
	if ((t2lm->t2lm_info[WLAN_T2LM_DL_DIRECTION].direction ==
			WLAN_T2LM_DL_DIRECTION ||
	     t2lm->t2lm_info[WLAN_T2LM_UL_DIRECTION].direction ==
			WLAN_T2LM_UL_DIRECTION) &&
	    t2lm->t2lm_info[WLAN_T2LM_BIDI_DIRECTION].direction ==
			WLAN_T2LM_BIDI_DIRECTION) {
		t2lm_err("Both DL/UL and BIDI T2LM IEs should not be present at the same time");
		return NULL;
	}

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (t2lm->t2lm_info[dir].direction !=
			WLAN_T2LM_INVALID_DIRECTION)
			frm = wlan_mlo_add_t2lm_info_ie(frm,
							&t2lm->t2lm_info[dir]);
	}

	return frm;
}

/**
 * wlan_mlo_parse_t2lm_request_action_frame() - API to parse T2LM request action
 * frame.
 * @t2lm: Pointer to T2LM structure
 * @action_frm: Pointer to action frame
 * @category: T2LM action frame category
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_parse_t2lm_request_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		enum wlan_t2lm_category category)
{
	uint8_t *t2lm_action_frm;

	t2lm->category = category;

	/*
	 * T2LM request action frame
	 *
	 *   1-byte     1-byte     1-byte   variable
	 *-------------------------------------------
	 * |         |           |        |         |
	 * | Category| Protected | Dialog | T2LM IE |
	 * |         |    EHT    | token  |         |
	 * |         |  Action   |        |         |
	 *-------------------------------------------
	 */

	t2lm_action_frm = (uint8_t *)action_frm + sizeof(*action_frm);

	t2lm->dialog_token = *t2lm_action_frm;

	return wlan_mlo_parse_t2lm_ie(t2lm,
				      t2lm_action_frm + sizeof(uint8_t));
}

/**
 * wlan_mlo_parse_t2lm_response_action_frame() - API to parse T2LM response
 * action frame.
 * @t2lm: Pointer to T2LM structure
 * @action_frm: Pointer to action frame
 * @category: T2LM action frame category
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_mlo_parse_t2lm_response_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		enum wlan_t2lm_category category)
{
	uint8_t *t2lm_action_frm;
	QDF_STATUS ret_val = QDF_STATUS_SUCCESS;

	t2lm->category = WLAN_T2LM_CATEGORY_RESPONSE;
	/*
	 * T2LM response action frame
	 *
	 *   1-byte     1-byte     1-byte   1-byte   variable
	 *----------------------------------------------------
	 * |         |           |        |        |         |
	 * | Category| Protected | Dialog | Status | T2LM IE |
	 * |         |    EHT    | token  |  code  |         |
	 * |         |  Action   |        |        |         |
	 *----------------------------------------------------
	 */

	t2lm_action_frm = (uint8_t *)action_frm + sizeof(*action_frm);

	t2lm->dialog_token = *t2lm_action_frm;
	t2lm->t2lm_resp_type = *(t2lm_action_frm + sizeof(uint8_t));

	if (t2lm->t2lm_resp_type ==
			WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING) {
		t2lm_action_frm += sizeof(uint8_t) + sizeof(uint8_t);
		ret_val = wlan_mlo_parse_t2lm_ie(t2lm, t2lm_action_frm);
	}

	return ret_val;
}

int wlan_mlo_parse_t2lm_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		enum wlan_t2lm_category category)
{
	QDF_STATUS ret_val = QDF_STATUS_SUCCESS;

	switch (category) {
	case WLAN_T2LM_CATEGORY_REQUEST:
		{
			ret_val = wlan_mlo_parse_t2lm_request_action_frame(
					t2lm, action_frm, category);
			return qdf_status_to_os_return(ret_val);
		}
	case WLAN_T2LM_CATEGORY_RESPONSE:
		{
			ret_val = wlan_mlo_parse_t2lm_response_action_frame(
					t2lm, action_frm, category);

			return qdf_status_to_os_return(ret_val);
		}
	case WLAN_T2LM_CATEGORY_TEARDOWN:
			/* Nothing to parse from T2LM teardown frame, just reset
			 * the mapping to default mapping.
			 *
			 * T2LM teardown action frame
			 *
			 *   1-byte     1-byte
			 *------------------------
			 * |         |           |
			 * | Category| Protected |
			 * |         |    EHT    |
			 * |         |  Action   |
			 *------------------------
			 */
			break;
	default:
			t2lm_err("Invalid category:%d", category);
	}

	return ret_val;
}

static uint8_t *wlan_mlo_add_t2lm_request_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;

	t2lm_info("T2LM request frame: category:%d action:%d dialog_token:%d",
		  args->category, args->action, args->arg1);
	return wlan_mlo_add_t2lm_ie(frm, (void *)buf);
}

static uint8_t *wlan_mlo_add_t2lm_response_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;
	/* Status code */
	*frm++ = args->arg2;

	t2lm_info("T2LM response frame: category:%d action:%d dialog_token:%d status_code:%d",
		  args->category, args->action, args->arg1, args->arg2);

	if (args->arg2 == WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING)
		frm = wlan_mlo_add_t2lm_ie(frm, (void *)buf);

	return frm;
}

uint8_t *wlan_mlo_add_t2lm_action_frame(
		uint8_t *frm,
		struct wlan_action_frame_args *args, uint8_t *buf,
		enum wlan_t2lm_category category)
{

	switch (category) {
	case WLAN_T2LM_CATEGORY_REQUEST:
		return wlan_mlo_add_t2lm_request_action_frame(frm, args,
							      buf, category);
	case WLAN_T2LM_CATEGORY_RESPONSE:
		return wlan_mlo_add_t2lm_response_action_frame(frm, args,
							      buf, category);
	case WLAN_T2LM_CATEGORY_TEARDOWN:
		*frm++ = args->category;
		*frm++ = args->action;
		return frm;
	default:
		t2lm_err("Invalid category:%d", category);
	}

	return frm;
}

/**
 * wlan_mlo_t2lm_handle_mapping_switch_time_expiry() - API to handle the mapping
 * switch timer expiry.
 * @t2lm_ctx: Pointer to T2LM context
 * @vdev: Pointer to vdev structure
 *
 * Return: None
 */
static void wlan_mlo_t2lm_handle_mapping_switch_time_expiry(
		struct wlan_t2lm_context *t2lm_ctx,
		struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	int i = 0;

	for (i = 0; i < t2lm_ctx->num_of_t2lm_ie; i++) {
		if (!t2lm_ctx->t2lm_ie[i].t2lm.mapping_switch_time_present)
			continue;

		/* Mapping switch time will always present at index 1. Hence,
		 * skip the index 0.
		 */
		if (i == 0)
			continue;

		qdf_mem_copy(&t2lm_ctx->t2lm_ie[0], &t2lm_ctx->t2lm_ie[1],
			     sizeof(struct wlan_mlo_t2lm_ie));
		t2lm_ctx->t2lm_ie[0].t2lm.mapping_switch_time_present = false;
		t2lm_ctx->t2lm_ie[0].t2lm.mapping_switch_time = 0;
		t2lm_debug("vdev_id:%d mark the advertised T2LM as established",
			   vdev_id);
	}

	/* Notify the registered caller about the link update*/
	wlan_mlo_dev_t2lm_notify_link_update(vdev->mlo_dev_ctx);
}

/**
 * wlan_mlo_t2lm_handle_expected_duration_expiry() - API to handle the expected
 * duration timer expiry.
 * @t2lm_ctx: Pointer to T2LM context
 * @vdev: Pointer to vdev structure
 *
 * Return: none
 */
static void wlan_mlo_t2lm_handle_expected_duration_expiry(
		struct wlan_t2lm_context *t2lm_ctx,
		struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	int i = 0;

	for (i = 0; i < t2lm_ctx->num_of_t2lm_ie; i++) {
		if (!t2lm_ctx->t2lm_ie[i].t2lm.expected_duration_present)
			continue;

		/* If two T2LM IEs are present, and expected duration of first
		 * T2LM IE is expired, copy the T2LM IE from index 1 to index 0.
		 * Mark mapping switch time present as false and clear the
		 * mapping switch time value.
		 * If one T2LM IE is present, and the expected duration is
		 * expired, configure the T2LM IE with the default values.
		 */
		if (!i && t2lm_ctx->num_of_t2lm_ie == WLAN_MAX_T2LM_IE) {
			qdf_mem_copy(&t2lm_ctx->t2lm_ie[0],
				     &t2lm_ctx->t2lm_ie[1],
				     sizeof(struct wlan_mlo_t2lm_ie));
			t2lm_ctx->t2lm_ie[0].t2lm.mapping_switch_time_present =
				false;
			t2lm_ctx->t2lm_ie[0].t2lm.mapping_switch_time = 0;
			t2lm_debug("vdev_id:%d mark the advertised T2LM as established",
				   vdev_id);
		} else {
			qdf_mem_zero(&t2lm_ctx->t2lm_ie[i],
				     sizeof(struct wlan_mlo_t2lm_ie));
			t2lm_ctx->t2lm_ie[i].t2lm.direction =
				WLAN_T2LM_BIDI_DIRECTION;
			t2lm_ctx->t2lm_ie[i].t2lm.default_link_mapping = 1;
			t2lm_debug("vdev_id:%d Expected duration is expired",
				   vdev_id);
		}
	}

	/* Notify the registered caller about the link update*/
	wlan_mlo_dev_t2lm_notify_link_update(vdev->mlo_dev_ctx);
}

QDF_STATUS wlan_mlo_vdev_tid_to_link_map_event(
		struct wlan_objmgr_psoc *psoc,
		struct mlo_vdev_host_tid_to_link_map_resp *event)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_t2lm_context *t2lm_ctx;
	int i = 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, event->vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		t2lm_err("null vdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!vdev->mlo_dev_ctx) {
		t2lm_err("null mlo_dev_ctx");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	t2lm_debug("status:%d", event->status);

	switch (event->status) {
	case WLAN_MAP_SWITCH_TIMER_TSF:
		for (i = 0; i < t2lm_ctx->num_of_t2lm_ie; i++) {
			if (!t2lm_ctx->t2lm_ie[i].t2lm.mapping_switch_time_present)
				continue;

			t2lm_ctx->t2lm_ie[i].t2lm.mapping_switch_time =
				event->mapping_switch_tsf;
			t2lm_debug("vdev_id:%d updated mapping switch time:%d",
				   event->vdev_id, event->mapping_switch_tsf);
		}
		break;
	case WLAN_MAP_SWITCH_TIMER_EXPIRED:
		wlan_mlo_t2lm_handle_mapping_switch_time_expiry(t2lm_ctx, vdev);
		break;
	case WLAN_EXPECTED_DUR_EXPIRED:
		wlan_mlo_t2lm_handle_expected_duration_expiry(t2lm_ctx, vdev);
		break;
	default:
		t2lm_err("Invalid status");
	}

	mlo_release_vdev_ref(vdev);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS wlan_send_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_t2lm_info *t2lm)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct wlan_objmgr_vdev *co_mld_vdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {NULL};
	uint16_t vdev_count = 0;
	int i = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		t2lm_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;
	if (!mlo_tx_ops) {
		t2lm_err("tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlo_tx_ops->send_tid_to_link_mapping) {
		t2lm_err("send_tid_to_link_mapping is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_get_ml_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	if (!vdev_count) {
		t2lm_err("Number of VDEVs under MLD is reported as 0");
		return QDF_STATUS_E_NULL_VALUE;
	}

	for (i = 0; i < vdev_count; i++) {
		co_mld_vdev = wlan_vdev_list[i];
		if (!co_mld_vdev) {
			t2lm_err("co_mld_vdev is null");
			mlo_release_vdev_ref(co_mld_vdev);
			continue;
		}

		status = mlo_tx_ops->send_tid_to_link_mapping(co_mld_vdev,
							      t2lm);
		if (QDF_IS_STATUS_ERROR(status))
			t2lm_err("Failed to send T2LM command to FW");
		mlo_release_vdev_ref(co_mld_vdev);
	}

	return status;
}

void wlan_mlo_t2lm_timer_expiry_handler(void *vdev)
{
	struct wlan_objmgr_vdev *vdev_ctx = (struct wlan_objmgr_vdev *)vdev;

	struct wlan_t2lm_timer *t2lm_timer;
	struct wlan_t2lm_context *t2lm_ctx;
	uint8_t t2lm_ie_idx;

	if (!vdev_ctx || !vdev_ctx->mlo_dev_ctx)
		return;

	t2lm_ctx = &vdev_ctx->mlo_dev_ctx->t2lm_ctx;
	t2lm_timer = &vdev_ctx->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	t2lm_ie_idx = t2lm_timer->t2lm_ie_index;
	if (t2lm_ie_idx >= WLAN_MAX_T2LM_IE)
		return;

	wlan_mlo_t2lm_timer_stop(vdev_ctx);

	if (t2lm_ctx->t2lm_ie[t2lm_ie_idx].t2lm.mapping_switch_time_present) {
		wlan_send_tid_to_link_mapping(
				vdev, &t2lm_ctx->t2lm_ie[t2lm_ie_idx].t2lm);
		wlan_mlo_t2lm_handle_mapping_switch_time_expiry(t2lm_ctx, vdev);
		wlan_handle_t2lm_timer(vdev_ctx, t2lm_timer->t2lm_ie_index);
	} else if (!t2lm_ie_idx) {
		wlan_mlo_t2lm_handle_expected_duration_expiry(t2lm_ctx, vdev);
		wlan_handle_t2lm_timer(vdev_ctx, t2lm_timer->t2lm_ie_index);
	}
}

QDF_STATUS
wlan_mlo_t2lm_timer_init(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer = NULL;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_FAILURE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_dev_lock_create(&vdev->mlo_dev_ctx->t2lm_ctx);
	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	qdf_timer_init(NULL, &t2lm_timer->t2lm_timer,
		       wlan_mlo_t2lm_timer_expiry_handler,
		       vdev, QDF_TIMER_TYPE_WAKE_APPS);

	t2lm_timer->timer_started = false;
	t2lm_timer->timer_interval = 0;
	t2lm_timer->t2lm_ie_index = 0;
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_t2lm_timer_start(struct wlan_objmgr_vdev *vdev,
			  uint32_t interval, uint8_t t2lm_ie_index)
{
	struct wlan_t2lm_timer *t2lm_timer;
	struct wlan_t2lm_context *t2lm_ctx;
	struct vdev_mlme_obj *vdev_mlme;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	if (interval == 0) {
		t2lm_debug("Timer interval is 0");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;
	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_debug("t2lm timer started with interval %d", interval);
	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	if (t2lm_ctx->t2lm_ie[t2lm_ie_index].t2lm.mapping_switch_time_present)
		t2lm_timer->timer_interval =
			t2lm_ctx->t2lm_ie[t2lm_ie_index].t2lm.mapping_switch_time;
	else
		t2lm_timer->timer_interval = interval *
			vdev_mlme->proto.generic.beacon_interval * 1000;

	t2lm_timer->t2lm_ie_index = t2lm_ie_index;
	t2lm_timer->timer_started = true;
	qdf_timer_start(&t2lm_timer->t2lm_timer, t2lm_timer->timer_interval);
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_t2lm_timer_stop(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	if (t2lm_timer->timer_started) {
		qdf_timer_stop(&t2lm_timer->t2lm_timer);
		t2lm_timer->timer_started = false;
		t2lm_timer->timer_interval = 0;
	}
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_handle_t2lm_timer(struct wlan_objmgr_vdev *vdev, uint8_t ie_idx)
{
	struct wlan_t2lm_context *t2lm_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;
	if (!t2lm_ctx->num_of_t2lm_ie) {
		t2lm_err("No T2LM IE present");
		return QDF_STATUS_SUCCESS;
	}

	if (ie_idx >= WLAN_MAX_T2LM_IE) {
		t2lm_err("Invalid T2LM IE index");
		return QDF_STATUS_E_FAILURE;
	}

	if (!t2lm_ctx->t2lm_ie[ie_idx].t2lm.mapping_switch_time_present &&
	    !t2lm_ctx->t2lm_ie[ie_idx].t2lm.expected_duration_present) {
		/* non-default to default mapping case */
		wlan_send_tid_to_link_mapping(vdev,
					      &t2lm_ctx->t2lm_ie[ie_idx].t2lm);
	} else if (t2lm_ctx->t2lm_ie[ie_idx].t2lm.mapping_switch_time_present) {
		/* Default to non-default mapping case */
		status = wlan_mlo_t2lm_timer_start(
				vdev,
				t2lm_ctx->t2lm_ie[ie_idx].t2lm.mapping_switch_time,
				ie_idx);
	} else if (t2lm_ctx->t2lm_ie[ie_idx].t2lm.expected_duration_present) {
		wlan_send_tid_to_link_mapping(
				vdev, &t2lm_ctx->t2lm_ie[ie_idx].t2lm);

		if (t2lm_ctx->t2lm_ie[ie_idx].t2lm.expected_duration !=
		    T2LM_EXPECTED_DURATION_MAX_VALUE)
			status = wlan_mlo_t2lm_timer_start(
					vdev,
					t2lm_ctx->t2lm_ie[ie_idx].t2lm.expected_duration,
					ie_idx);
	}

	return status;
}

/**
 * wlan_update_mapping_switch_time_expected_dur() - API to update the mapping
 * switch time and expected duration.
 * @vdev:Pointer to vdev
 * @rx_t2lm: Pointer to received T2LM IE
 * @tsf: TSF value of beacon/probe response
 *
 * Return: None
 */
static void wlan_update_mapping_switch_time_expected_dur(
		struct wlan_objmgr_vdev *vdev, struct wlan_t2lm_info *rx_t2lm,
		uint64_t tsf)
{
	struct wlan_t2lm_context *t2lm_ctx;
	uint16_t tsf_bit25_16, ms_time;
	bool match_found = false;
	int j;

	tsf_bit25_16 = (tsf & 0x3FF0000) >> 16;
	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	for (j = 0; j < t2lm_ctx->num_of_t2lm_ie; j++) {
		/* Match not found */
		if (qdf_mem_cmp(rx_t2lm->ieee_link_map_tid,
				t2lm_ctx->t2lm_ie[j].t2lm.ieee_link_map_tid,
				sizeof(uint16_t) * T2LM_MAX_NUM_TIDS))
			continue;

		if (t2lm_ctx->t2lm_ie[j].t2lm.mapping_switch_time_present) {
			ms_time = rx_t2lm->mapping_switch_time;

			if (ms_time > tsf_bit25_16) {
				t2lm_ctx->t2lm_ie[j].t2lm.mapping_switch_time =
					((ms_time - tsf_bit25_16) * 1024) / 1000;
			} else {
				t2lm_ctx->t2lm_ie[j].t2lm.mapping_switch_time =
					((0xFFFF - (tsf_bit25_16 - ms_time)) * 1024) / 1000;
			}
		}

		if (t2lm_ctx->t2lm_ie[j].t2lm.expected_duration_present) {
			t2lm_ctx->t2lm_ie[j].t2lm.expected_duration =
				rx_t2lm->expected_duration;
		}

		match_found = true;
		break;
	}

	if (!match_found &&
	    t2lm_ctx->num_of_t2lm_ie < WLAN_MAX_T2LM_IE) {
		qdf_mem_copy(&t2lm_ctx->t2lm_ie[t2lm_ctx->num_of_t2lm_ie].t2lm,
			     rx_t2lm, sizeof(struct wlan_t2lm_info));
		t2lm_ctx->num_of_t2lm_ie++;
	}
}

QDF_STATUS wlan_process_bcn_prbrsp_t2lm_ie(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_context *rx_t2lm_ie, uint64_t tsf)
{
	struct wlan_t2lm_context *t2lm_ctx;
	int i;

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;

	for (i = 0; i < rx_t2lm_ie->num_of_t2lm_ie; i++) {
		wlan_update_mapping_switch_time_expected_dur(
				vdev, &rx_t2lm_ie->t2lm_ie[i].t2lm, tsf);
	}

	if (!wlan_cm_is_vdev_connected(vdev))
		return QDF_STATUS_SUCCESS;

	/* Do not start the timer if STA is not in connected state */
	for (i = 0; i < t2lm_ctx->num_of_t2lm_ie; i++) {
		if (t2lm_ctx->t2lm_ie[i].t2lm.mapping_switch_time_present ||
		    t2lm_ctx->t2lm_ie[i].t2lm.expected_duration_present) {
			wlan_handle_t2lm_timer(vdev, i);
			break;
		}
	}

	return QDF_STATUS_SUCCESS;
}

int wlan_register_t2lm_link_update_notify_handler(
		wlan_mlo_t2lm_link_update_handler handler,
		struct wlan_mlo_dev_context *mldev)
{
	struct wlan_t2lm_context *t2lm_ctx = &mldev->t2lm_ctx;
	int i;

	for (i = 0; i < MAX_T2LM_HANDLERS; i++) {
		if (t2lm_ctx->is_valid_handler[i])
			continue;

		t2lm_ctx->link_update_handler[i] = handler;
		t2lm_ctx->is_valid_handler[i] = true;
		break;
	}

	if (i == MAX_T2LM_HANDLERS) {
		t2lm_err("Failed to register the link disablement callback");
		return -EINVAL;
	}

	return i;
}

void wlan_unregister_t2lm_link_update_notify_handler(
		struct wlan_mlo_dev_context *mldev,
		uint8_t index)
{
	if (index >= MAX_T2LM_HANDLERS)
		return;

	mldev->t2lm_ctx.link_update_handler[index] = NULL;
	mldev->t2lm_ctx.is_valid_handler[index] = false;
}

QDF_STATUS wlan_mlo_dev_t2lm_notify_link_update(
		struct wlan_mlo_dev_context *mldev)
{
	struct wlan_t2lm_context *t2lm_ctx = &mldev->t2lm_ctx;
	wlan_mlo_t2lm_link_update_handler handler;
	int i;

	for (i = 0; i < MAX_T2LM_HANDLERS; i++) {
		if (!t2lm_ctx->is_valid_handler[i])
			continue;

		handler = t2lm_ctx->link_update_handler[i];
		if (!(handler && t2lm_ctx->num_of_t2lm_ie))
			continue;

		handler(mldev, &t2lm_ctx->t2lm_ie[0].t2lm.ieee_link_map_tid[0]);
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_t2lm_timer_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_t2lm_timer *t2lm_timer = NULL;

	if (!vdev || !vdev->mlo_dev_ctx)
		return QDF_STATUS_E_FAILURE;

	t2lm_timer = &vdev->mlo_dev_ctx->t2lm_ctx.t2lm_timer;
	if (!t2lm_timer) {
		t2lm_err("t2lm timer ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_dev_lock_acquire(&vdev->mlo_dev_ctx->t2lm_ctx);
	t2lm_timer->timer_started = false;
	t2lm_timer->timer_interval = 0;
	t2lm_timer->t2lm_ie_index = 0;
	t2lm_dev_lock_release(&vdev->mlo_dev_ctx->t2lm_ctx);
	qdf_timer_free(&t2lm_timer->t2lm_timer);
	t2lm_dev_lock_destroy(&vdev->mlo_dev_ctx->t2lm_ctx);
	return QDF_STATUS_SUCCESS;
}
