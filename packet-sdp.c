/* packet-sdp.c
 * Routines for SDP packet disassembly (RFC 2327)
 *
 * Jason Lango <jal@netapp.com>
 * Liberally copied from packet-http.c, by Guy Harris <guy@alum.mit.edu>
 *
 * $Id: packet-sdp.c,v 1.21 2001/12/10 00:25:34 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "config.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h>
#include <ctype.h>

#include <glib.h>
#include "packet.h"
#include "strutil.h"

static int proto_sdp = -1;

static int ett_sdp = -1;

static void
dissect_sdp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_tree	*sdp_tree;
	proto_item	*ti;
	gint		offset = 0;
	const u_char	*line;
	gint		next_offset;
	int		linelen;
	u_char		section;
	u_char		type;
	const u_char	*value;
	int		valuelen;
	const char	*typename;
	int		datalen;

	/*
	 * As RFC 2327 says, "SDP is purely a format for session
	 * description - it does not incorporate a transport protocol,
	 * and is intended to use different transport protocols as
	 * appropriate including the Session Announcement Protocol,
	 * Session Initiation Protocol, Real-Time Streaming Protocol,
	 * electronic mail using the MIME extensions, and the
	 * Hypertext Transport Protocol."
	 *
	 * We therefore don't set the protocol or info columns;
	 * instead, we append to them, so that we don't erase
	 * what the protocol inside which the SDP stuff resides
	 * put there.
	 */
	if (check_col(pinfo->cinfo, COL_PROTOCOL))
		col_append_str(pinfo->cinfo, COL_PROTOCOL, "/SDP");

	if (check_col(pinfo->cinfo, COL_INFO)) {
		/* XXX: Needs description. */
		col_append_str(pinfo->cinfo, COL_INFO, ", with session description");
	}

	if (!tree)
		return;

	ti = proto_tree_add_item(tree, proto_sdp, tvb, offset,
	    tvb_length_remaining(tvb, offset), FALSE);
	sdp_tree = proto_item_add_subtree(ti, ett_sdp);

	/*
	 * Show the SDP message a line at a time.
	 */
	section = 0;
	while (tvb_offset_exists(tvb, offset)) {
		/*
		 * Find the end of the line.
		 */
		linelen = tvb_find_line_end_unquoted(tvb, offset, -1,
		    &next_offset);

		/*
		 * Line must contain at least e.g. "v=".
		 */
		if (linelen < 2)
			break;

		line = tvb_get_ptr(tvb, offset, next_offset - offset);
		type = line[0];
		if (line[1] != '=') {
			proto_tree_add_text(sdp_tree, tvb, offset,
			    next_offset - offset,
			    "Invalid line: %s",
			    tvb_format_text(tvb, offset, next_offset - offset));
                        offset = next_offset;
			continue;
		}
		value = line + 2;
		valuelen = linelen - 2;

		/*
		 * Attributes.
		 */
		switch (type) {
		case 'v':
			section = 'v';
			typename = "Session Description, version";
			break;
		case 'o':
			typename = "Owner/Creator, Session Id";
			break;
		case 's':
			typename = "Session Name";
			break;
		case 'i':
			if (section == 'v')
				typename = "Session Information";
			else if (section == 'm')
				typename = "Media Title";
			else
				typename = "Misplaced";
			break;
		case 'u':
			typename = "URI of Description";
			break;
		case 'e':
			typename = "E-mail Address";
			break;
		case 'p':
			typename = "Phone Number";
			break;
		case 'c':
			typename = "Connection Information";
			break;
		case 'b':
			typename = "Bandwidth Information";
			break;
		case 't':
			section = 't';
			typename = "Time Description, active time";
			break;
		case 'r':
			typename = "Repeat Time";
			break;
		case 'm':
			section = 'm';
			typename = "Media Description, name and address";
			break;
		case 'k':
			typename = "Encryption Key";
			break;
		case 'a':
			if (section == 'v')
				typename = "Session Attribute";
			else if (section == 'm')
				typename = "Media Attribute";
			else
				typename = "Misplaced";
			break;
		case 'z':
			typename = "Time Zone Adjustment";
			break;
		default:
			typename = "Unknown";
			break;
		}

		proto_tree_add_text(sdp_tree, tvb, offset,
		    next_offset - offset,
		    "%s (%c): %s", typename, type,
		    format_text(value, valuelen));
		offset = next_offset;
	}

	datalen = tvb_length_remaining(tvb, offset);
	if (datalen > 0) {
		proto_tree_add_text(sdp_tree, tvb, offset, datalen,
		    "Data (%d bytes)", datalen);
	}
}

void
proto_register_sdp(void)
{
/*        static hf_register_info hf[] = {
                { &variable,
                { "Name",           "sdp.abbreviation", TYPE, VALS_POINTER }},
        };*/
	static gint *ett[] = {
		&ett_sdp,
	};

        proto_sdp = proto_register_protocol("Session Description Protocol",
	    "SDP", "sdp");
 /*       proto_register_field_array(proto_sdp, hf, array_length(hf));*/
	proto_register_subtree_array(ett, array_length(ett));

	/*
	 * Register the dissector by name, so other dissectors can
	 * grab it by name rather than just referring to it directly
	 * (you can't refer to it directly from a plugin dissector
	 * on Windows without stuffing it into the Big Transfer Vector).
	 */
	register_dissector("sdp", dissect_sdp, proto_sdp);
}
