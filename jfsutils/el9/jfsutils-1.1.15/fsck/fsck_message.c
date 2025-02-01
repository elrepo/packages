#define _GNU_SOURCE	/* for basename() */
#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "fsck_message.h"
#include "fsckwsp.h"
#include "jfs_endian.h"
#include "xfsckint.h"

int msg_lvl = fsck_debug;
int dbg_output = 0;

extern struct fsck_agg_record *agg_recptr;

/*****************************************************************************
 * NAME: fsck_record_msg
 *
 * FUNCTION:
 *
 * PARAMETERS:
 *      ?                 - input -
 *      ?                 - returned -
 *
 * RETURNS:
 *	nothing
 */
void fsck_record_msg(char *msg_txt) {
	char log_entry[max_log_entry_length];
	int entry_length = sizeof(struct fscklog_entry_hdr);
	struct fscklog_entry_hdr *hdptr;
	char *msg;
	int32_t buffer_bytes_left;
	char *buf_entry_ptr;

	if (agg_recptr->fscklog_full || !agg_recptr->fscklog_buf_allocated ||
	    agg_recptr->fscklog_buf_alloc_err)
		return;

	/* logging is active */
	hdptr = (struct fscklog_entry_hdr *) log_entry;
	msg = &log_entry[entry_length];
	strncpy(msg, msg_txt, max_log_entry_length - entry_length);
	entry_length += strlen(msg_txt);
	/* add null terminator to string */
	log_entry[entry_length++] = '\0';

	/* pad to the next 4 byte boundary */
	entry_length = ((entry_length + 3) / 4) * 4;
	hdptr->entry_length = entry_length;
	buffer_bytes_left = agg_recptr->fscklog_buf_length -
			    agg_recptr->fscklog_buf_data_len;
	if (buffer_bytes_left < entry_length) {
		agg_recptr->fscklog_last_msghdr->entry_length +=
			buffer_bytes_left;
		fscklog_put_buffer();
		// clear the buffer
		memset((void *) (agg_recptr->fscklog_buf_ptr), 0,
		       agg_recptr->fscklog_buf_length);
	}

	if (!agg_recptr->fscklog_full) {
		buf_entry_ptr = (char *)((char *)agg_recptr->fscklog_buf_ptr +
					 agg_recptr->fscklog_buf_data_len);

		// swap if on big endian machine
		ujfs_swap_fscklog_entry_hdr(hdptr);

		memcpy((void *) buf_entry_ptr, (void *) hdptr, entry_length);

		agg_recptr->fscklog_last_msghdr =
			(struct fscklog_entry_hdr *) buf_entry_ptr;
		agg_recptr->fscklog_buf_data_len += entry_length;
	}
}


/*****************************************************************************
 * NAME: fsck_send_msg
 *
 * FUNCTION:
 *
 * PARAMETERS:
 *      ?                 - input -
 *      ?                 - returned -
 *
 * RETURNS:
 *	0 on success
 *      Non-zero on error
 */
int v_fsck_send_msg(int msg_num, const char *file_name, int line_number, ...) {
	struct fsck_message *message = &msg_defs[msg_num];

	char msg_string[max_log_entry_length - 4];
	char debug_detail[100];
	va_list args;

	va_start(args, line_number);
	vsnprintf(msg_string, sizeof(msg_string), message->msg_txt, args);
	va_end(args);

	sprintf(debug_detail, " [%s:%d]\n", basename(file_name), line_number);

	if (message->msg_level <= msg_lvl) {
		printf("%s", msg_string);
		if (dbg_output) {
			printf("%s", debug_detail);
		}
		else printf("\n");
	}

	// append file and line number information to string for logging
	strncat(msg_string, debug_detail,
		max_log_entry_length - 4 - strlen(msg_string));

	fsck_record_msg(msg_string);

	return 0;
}
