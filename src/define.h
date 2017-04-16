#ifndef DEFINE_H
#define DEFINE_H


#define _(STRING) gettext(STRING)


#define PARSED_TLVS_MAX_NUMBER	15
#define PARSED_TLVS_MAX_LENGTH	510

#define LLDP_BUF_SIZ		4000


#define DESCRIPTOR_WIDTH 10		//TODO: TLVcustomcopy

// Subtype numbers LANbeacon:
#define SUBTYPE_VLAN_ID 200
#define SUBTYPE_NAME 201
#define SUBTYPE_CUSTOM 202
#define SUBTYPE_IPV4 203
#define SUBTYPE_IPV6 204
#define SUBTYPE_EMAIL 205
#define SUBTYPE_DHCP 206
#define SUBTYPE_ROUTER 207
#define SUBTYPE_SIGNATURE 216
#define SUBTYPE_COMBINED_STRING 217

// Descriptors LANbeacon:
#define DESCRIPTOR_VLAN_ID gettext("VLAN-ID:")
#define DESCRIPTOR_NAME gettext("VLAN-Name:")
#define DESCRIPTOR_CUSTOM gettext("Freitext:")
#define DESCRIPTOR_IPV4 gettext("IPv4:")
#define DESCRIPTOR_IPV6 gettext("IPv6:")
#define DESCRIPTOR_EMAIL gettext("Email:")
#define DESCRIPTOR_DHCP gettext("DHCP:")
#define DESCRIPTOR_ROUTER gettext("Router:")
#define DESCRIPTOR_SIGNATURE gettext("Authentication:")
#define DESCRIPTOR_COMBINED_STRING gettext("Combined String:")

#endif
