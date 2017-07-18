#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
//#include <linux/if.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <openssl/evp.h>
#include <sys/select.h>
#include <libintl.h>
#include <locale.h>

#include "rawsocket_LLDP.h"
#include "receiver.h"
#include "openssl_sign.h"
#include "define.h"
#include "openssl_sign.h"
#define _GNU_SOURCE

#define SEND_SOCKET 0
#define REC_SOCKET 1
//#define SET_SELECT_FDS 
//	FD_ZERO(&readfds); 
//	for (int x = 0; x < numInterfaces; x++) 
//		{FD_SET(sockfd[x], &readfds);}

void sendRawSocket (unsigned char *destination_mac, void *payload, int payloadLen, 
					unsigned short etherType, struct open_ssl_keys *lanbeacon_keys, char *interface_to_send_on) {
puts("Begin of sendRawSocket");	
	if (etherType == CHALLENGE_ETHTYPE)
		* (unsigned long *) payload = htonl(*(unsigned long *) payload);
	int sockfd[20];
	struct ifreq if_idx[20];
	struct ifreq if_mac[20];
	int frameLength = 0;
	int numInterfaces = 0;
	char lldpEthernetFrame[LLDP_BUF_SIZ];
	struct ether_header *eh = (struct ether_header *) lldpEthernetFrame;
	struct sockaddr_ll socket_address;

	unsigned long *receivedChallenge = NULL;

	if (etherType == LLDP_ETHER_TYPE) {
		receivedChallenge = calloc(sizeof(unsigned long), 1);
		if(!receivedChallenge) 
			puts(_("calloc error of \"receivedChallenge\" in sendRawSocket"));
	}

	// Construct the Ethernet header
	memset(lldpEthernetFrame, 0, LLDP_BUF_SIZ);
	memcpy(eh->ether_dhost, destination_mac, 6);
// printf("%02X ****** %02X ****** %02X ****** %02X ****** %02X ****** %02X\n", (unsigned) eh->ether_dhost[0], (unsigned) eh->ether_dhost[1], (unsigned) eh->ether_dhost[2], (unsigned) eh->ether_dhost[3], (unsigned) eh->ether_dhost[4], (unsigned) eh->ether_dhost[5]);
	// Ethertype field
	eh->ether_type = htons(etherType);
	frameLength += sizeof(struct ether_header);

	// Packet data
	memcpy(&lldpEthernetFrame[frameLength], payload, payloadLen);
	frameLength += payloadLen;
	
	// End of LLDPDU TLV
	if (etherType == LLDP_ETHER_TYPE) {
		lldpEthernetFrame[frameLength++] = 0x00;
		lldpEthernetFrame[frameLength++] = 0x00;
	}

	// Address length and destination MAC
	socket_address.sll_halen = ETH_ALEN;
	memcpy(socket_address.sll_addr, destination_mac, 6);

	// Get interfaces
	getInterfaces (sockfd, &numInterfaces, etherType, SEND_SOCKET, if_idx, if_mac, NULL, NULL, interface_to_send_on);
	
	// Get interfaces for challenge		TODO
	int challengeSockfd[20];
	int challengeSockopt[20];
	int challengeNumInterfaces = 0;
	int challengeMaxSockFd = 0;
	getInterfaces (challengeSockfd, &challengeNumInterfaces, CHALLENGE_ETHTYPE, REC_SOCKET, 
		NULL, NULL, challengeSockopt, &challengeMaxSockFd, interface_to_send_on);

	
	int signed_responses_sent = 0; 
	
	// send challenge 1 time
	// in case of LLDP-packet counter is not incremented, therefore infinite loop
	for (int i = 0; i<1; ) {

		
		// send packets on all interfaces
		for(int j = 0; j < numInterfaces; j++) {

			// Ethernet header, destination MAC address
			memcpy(eh->ether_shost, ((uint8_t *)&if_mac[j].ifr_hwaddr.sa_data), 6);
			
			if (etherType == LLDP_ETHER_TYPE) {
				// Port and chassis subtype TLVs filled
				memcpy(&lldpEthernetFrame[17], ((uint8_t *)&if_mac[j].ifr_hwaddr.sa_data), 6);
				memcpy(&lldpEthernetFrame[26], ((uint8_t *)&if_mac[j].ifr_hwaddr.sa_data), 6);
			}
		
			// Index of the network device
			socket_address.sll_ifindex = if_idx[j].ifr_ifindex;

			// Send packet
			if (sendto(sockfd[j], lldpEthernetFrame, frameLength, 0, 
				(struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
					printf(_("Send failed on interface number %i\n"), j);
			else
				printf(_("Successfully sent on interface number %i\n"), j);
		}

		if (etherType == CHALLENGE_ETHTYPE) i++; //{
			
//			usleep(CHALLENGE_SEND_FREQUENCY);
//		}

		if (etherType == LLDP_ETHER_TYPE) {
			
			if ((*receivedChallenge != 0) && (signed_responses_sent++ == 1)) {
			
				*receivedChallenge = 0;
			
				memcpy(eh->ether_dhost, destination_mac, 6);
				memcpy(socket_address.sll_addr, destination_mac, 6);
			
				frameLength -= 270;
				lldpEthernetFrame[frameLength-2] = 0x00;
				lldpEthernetFrame[frameLength-1] = 0x00;
				flush_all_interfaces (challengeSockfd, challengeMaxSockFd, challengeNumInterfaces);
			}
puts("########################################################################");
			
			if (*receivedChallenge == 0) {
				
				// receive challenge
				char challenge_dest_mac[6];
				*receivedChallenge = receiveChallenge(	challengeSockfd, 
														challengeNumInterfaces, 
														challengeMaxSockFd,
														challenge_dest_mac);
				*receivedChallenge = htonl(*receivedChallenge);
				
				if (*receivedChallenge != 0) {
					memcpy(eh->ether_dhost, challenge_dest_mac, 6);
					memcpy(socket_address.sll_addr, challenge_dest_mac, 6);
				
					// generate timestamp
					time_t timeStamp = time(NULL);
					timeStamp = htonl(timeStamp);
				
					// add signature TLV
	//				long challenge = 0;

	//				challenge = htonl(challenge);
	//				memcpy(&mylanbeacon[currentByte], &challenge, 4);
				
					lldpEthernetFrame[frameLength-2+0] = 0xff;
					lldpEthernetFrame[frameLength-2+1] = 0x0c;
					lldpEthernetFrame[frameLength-2+2] = 0xcc;
					lldpEthernetFrame[frameLength-2+3] = 0x4d;
					lldpEthernetFrame[frameLength-2+4] = 0x55;
					lldpEthernetFrame[frameLength-2+5] = 0xd8;
				
		//			unsigned char signature_header_buffer[6] = {0xff, 0x0c, 0xcc, 0x4d, 0x55, 0xd8};
		//			memcpy(&lldpEthernetFrame[frameLength-2], signature_header_buffer, 6);
				
					memcpy(&lldpEthernetFrame[frameLength-2+6], receivedChallenge, 4);
				
	//				memcpy(&mylanbeacon[currentByte+4], &timeStamp, 4);
					memcpy(&lldpEthernetFrame[frameLength-2+6+4], &timeStamp, 4);
				
				
					unsigned char* sig = NULL;
					size_t slen = 0;
					signlanbeacon(&sig, &slen, (const unsigned char *) &lldpEthernetFrame[14], 
						(size_t) payloadLen + 6 + 6 + 4, lanbeacon_keys);
					memcpy(&lldpEthernetFrame[frameLength-2+6+4+4], sig, slen);
					free(sig);
				
				
					lldpEthernetFrame[frameLength+272] = 0x00;
					lldpEthernetFrame[frameLength+273] = 0x00;
				
					frameLength += 270;
					
					signed_responses_sent = 0; 
					
					// add TLV-header using function, this overrides lanbeacon signature part with itself
	//				transferToCombinedBeacon (SUBTYPE_SIGNATURE, &mylanbeacon[currentByte], 
	//					mylanbeacon, &currentByte, 256 + 8);

	//				unsigned char* sig = NULL;
	//				size_t slen = 0;
	//				signlanbeacon(&sig, &slen, (const unsigned char *) mylanbeacon, 
	//					(size_t) currentByte - 256, lanbeacon_keys);
	//				memcpy(&mylanbeacon[currentByte-256], sig, slen);

	//				free(sig);
				
				
					//## add signature ##//
	//				time_t timeStamp = time(NULL);
	//				timeStamp = htonl(timeStamp);
				}
				
			}
			else
				sleep(LLDP_SEND_FREQUENCY);

		}
//printf("%i*********************** %ld *************** %02X ****** %02X ****** %02X ****** %02X\n", frameLength,   * (unsigned long *) payload, (unsigned) lldpEthernetFrame[14], (unsigned)lldpEthernetFrame[15], (unsigned)lldpEthernetFrame[16], (unsigned)(unsigned)lldpEthernetFrame[17]);

	}

	//free(receivedChallenge);

	return;
}


// parts of code based on https://gist.github.com/austinmarton/1922600
int sendLLDPrawSock (struct sender_information *my_sender_information)
{
//	unsigned char lldp_mac[6] = {LLDP_DEST_MAC};
	sendRawSocket ((unsigned char[6]){LLDP_DEST_MAC}, my_sender_information->lanBeacon_PDU, 
		my_sender_information->lldpdu_len, LLDP_ETHER_TYPE, &my_sender_information->lanbeacon_keys, my_sender_information->interface_to_send_on);
	return EXIT_SUCCESS;
}


// parts of code based on https://gist.github.com/austinmarton/2862515
struct received_lldp_packet *recLLDPrawSock(struct receiver_information *my_receiver_information) {

	struct received_lldp_packet *my_received_lldp_packet = 
		malloc(sizeof(struct received_lldp_packet));
	if(!my_received_lldp_packet) 
		puts(_("malloc error of \"my_received_lldp_packet\" in recLLDPrawSock"));
	struct ether_header *eh = 
		(struct ether_header *) my_received_lldp_packet->lldpReceivedPayload;

	int sockfd[20];
	int sockopt[20];

	// parameters for select()
	int maxSockFd = 0;
	struct timeval tv = {1, 0};
	fd_set readfds;
	int numInterfaces = 0;

	getInterfaces (sockfd, &numInterfaces, LLDP_ETHER_TYPE, REC_SOCKET, NULL, NULL, sockopt, &maxSockFd, NULL);

	int challengeSentBool = 0;
	int rv;

	// receive one lanbeacon frame, send a challenge to the source MAC and
	// then wait for the frame containing the challenge
	while (1) {
		//SET_SELECT_FDS
		FD_ZERO(&readfds); 
		for (int x = 0; x < numInterfaces; x++) FD_SET(sockfd[x], &readfds);
		
		rv = select(maxSockFd, &readfds, NULL, NULL, NULL);

		if (rv == -1) 
			perror("select");
		else if (rv == 0) 
			printf("Timeout occurred! No data after %i seconds.\n", LLDP_SEND_FREQUENCY);  // TODO receive for how long?
		else {
			for (int i = 0; i < numInterfaces; i++) {
				if (FD_ISSET(sockfd[i], &readfds)) {

					my_received_lldp_packet->payloadSize = 
						recvfrom(sockfd[i], my_received_lldp_packet->lldpReceivedPayload, 
							LLDP_BUF_SIZ, 0, NULL, NULL);
//printf("neu %02X ****** %02X ****** %02X ****** %02X ****** %02X ****** %02X\n", (unsigned) my_received_lldp_packet->lldpReceivedPayload[6], (unsigned) my_received_lldp_packet->lldpReceivedPayload[7], (unsigned) my_received_lldp_packet->lldpReceivedPayload[8], (unsigned) my_received_lldp_packet->lldpReceivedPayload[9], (unsigned) my_received_lldp_packet->lldpReceivedPayload[10], (unsigned) my_received_lldp_packet->lldpReceivedPayload[11]);

					// Check if the packet was sent to the LLDP multicast MAC address
//printf("%02X ****** %02X ****** %02X ****** %02X ****** %02X ****** %02X\n", (unsigned) eh->ether_dhost[0], (unsigned) eh->ether_dhost[1], (unsigned) eh->ether_dhost[2], (unsigned) eh->ether_dhost[3], (unsigned) eh->ether_dhost[4], (unsigned) eh->ether_dhost[5]);
					
					// if no challenge has been sent yet,
					// only break if message was sent to multicast address
					
					if (0 == challengeSentBool) {
						if(memcmp((unsigned char[6]){LLDP_DEST_MAC}, eh->ether_dhost, 6))
							break;
					}
					
					// if challenge has been sent yet,
					// only break if message was NOT sent to multicast address
					else {
						if(!memcmp((unsigned char[6]){LLDP_DEST_MAC}, eh->ether_dhost, 6))
							break;
					}

//					break;
				}
			}
		}
		
		// Verify signature
		// position: end of LLDPDU - 2 Ethernet header - 14
		if (0 != verifylanbeacon(
			&my_received_lldp_packet->lldpReceivedPayload[14],
			my_received_lldp_packet->payloadSize - 2 - 14, &my_receiver_information->lanbeacon_keys)) {
				puts("problem with verification");
		}
		
		memcpy(my_received_lldp_packet->current_destination_mac, eh->ether_shost, 6);

		if ((my_receiver_information->authenticated == 1) && (0 == challengeSentBool++)) {
			// delete received packet, send challenge and flush buffer
			memset (my_received_lldp_packet->lldpReceivedPayload, 0, LLDP_BUF_SIZ);

			srand(time(NULL));
//my_received_lldp_packet->challenge = 65321;
			my_received_lldp_packet->challenge = 1+ (rand() % 4294967294);
//printf("dadada%02X ****** %02X ****** %02X ****** %02X ****** %02X ****** %02X\n", (unsigned) eh->ether_shost[0], (unsigned) eh->ether_shost[1], (unsigned) eh->ether_shost[2], (unsigned) eh->ether_shost[3], (unsigned) eh->ether_shost[4], (unsigned) eh->ether_shost[5]);
//FILE *combined = fopen("macDebug","w");	fwrite(my_received_lldp_packet->current_destination_mac, 20, 1, combined); fclose(combined); exit(0);
			sendRawSocket (my_received_lldp_packet->current_destination_mac, &my_received_lldp_packet->challenge, 
				4, CHALLENGE_ETHTYPE, NULL, NULL);
			tv.tv_sec = 0;
			
			flush_all_interfaces (sockfd, maxSockFd, numInterfaces);
/*			
			while (1) {
				//SET_SELECT_FDS
				FD_ZERO(&readfds); 
				for (int x = 0; x < numInterfaces; x++) FD_SET(sockfd[x], &readfds);
				
				rv = select(maxSockFd, &readfds, NULL, NULL, &tv);
puts("neuneuneu");//sleep(1);
				if (rv == -1) perror("select"); // error occurred in select()
				else if (rv == 0) {
					printf("Timeout occurred! All data flushed.\n");
					break;
				}
				else {
					for (int j = 0; j < numInterfaces; j++) {
						if (FD_ISSET(sockfd[j], &readfds)) {
puts("Deleted something");
							recvfrom(sockfd[j], NULL, 1500, 0, NULL, NULL);
						}
					}
				}
			}
//*/
		}
		else break;
	}

	for (int i = 0; i < numInterfaces; i++)
		close(sockfd[i]);

	return my_received_lldp_packet;
}


// parts of code based on https://gist.github.com/austinmarton/2862515
unsigned long receiveChallenge(int *sockfd, int numInterfaces, int maxSockFd, char *challenge_dest_mac) {

	unsigned char *receiveBuf = calloc(300, 1);
	if(!receiveBuf) puts(_("calloc error of \"receiveBuf\" in receiveChallenge"));
	int receivedSize;
	unsigned long *receivedChallenge = calloc(sizeof(unsigned long), 1);
	if(!receivedChallenge) puts(_("calloc error of \"receivedChallenge\" in receiveChallenge"));

	struct ether_header *eh = (struct ether_header *) receiveBuf;

	// parameters for select()
	struct timeval tv = {1, 0};
	tv.tv_sec = LLDP_SEND_FREQUENCY;
	fd_set readfds;

	//SET_SELECT_FDS
	FD_ZERO(&readfds); 
	for (int x = 0; x < numInterfaces; x++) FD_SET(sockfd[x], &readfds);	

	int rv = select(maxSockFd, &readfds, NULL, NULL, &tv);

	if (rv == -1) 
		perror("select"); // error occurred in select()
	else if (rv == 0) 
		printf("Timeout occurred! No data after %i seconds.\n", LLDP_SEND_FREQUENCY);
	else {
		for (int i = 0; i < numInterfaces; i++) {
			if (FD_ISSET(sockfd[i], &readfds)) {

				receivedSize = recvfrom(sockfd[i], receiveBuf, 300, 0, NULL, NULL);

				memcpy(receivedChallenge, &receiveBuf[14], 4);
				*receivedChallenge = ntohl(*receivedChallenge);
				printf(_("Received challenge: %lu\n"), *receivedChallenge);
				memcpy(challenge_dest_mac, &receiveBuf[6], 6);

				break;
			}
		}
	}

//	for (int i = 0; i < numInterfaces; i++)
//		close(sockfd[i]);

	free(receiveBuf);

	return *receivedChallenge;
}


void getInterfaces (int *sockfd, int *numInterfaces, unsigned short etherType, 
					unsigned short sendOrReceive, struct ifreq *if_idx, 
					struct ifreq *if_mac, int *sockopt, int *maxSockFd, char *interface_to_send_on) {

	struct ifaddrs *interfaces;
	if (getifaddrs(&interfaces) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	for (; interfaces != NULL; interfaces = interfaces->ifa_next) {

		
		if (interface_to_send_on) {
			if (strcmp(interface_to_send_on, interfaces->ifa_name) ) {
				continue;
			}
		}
		printf("if-name: %s\nparameter: %s\n", interfaces->ifa_name, interface_to_send_on);

		if (interfaces->ifa_addr == NULL) continue;
		if (!(interfaces->ifa_addr->sa_family == AF_PACKET)) continue;

		// Open RAW socket to send on
		if ((sockfd[*numInterfaces] = socket(PF_PACKET, SOCK_RAW, htons(etherType))) == -1) {
			perror("socket");
		}

		if (sendOrReceive == SEND_SOCKET) {

			// Get the index of the interface to send on
			memset(&if_idx[*numInterfaces], 0, sizeof(struct ifreq));
			memcpy(if_idx[*numInterfaces].ifr_name, interfaces->ifa_name, IFNAMSIZ-1);
			if (ioctl(sockfd[*numInterfaces], SIOCGIFINDEX, &if_idx[*numInterfaces]) < 0)
				perror("SIOCGIFINDEX");
			// Get the MAC address of the interface to send on
			memset(&if_mac[*numInterfaces], 0, sizeof(struct ifreq));
			memcpy(if_mac[*numInterfaces].ifr_name, interfaces->ifa_name, IFNAMSIZ-1);
			if (ioctl(sockfd[*numInterfaces], SIOCGIFHWADDR, &if_mac[*numInterfaces]) < 0)
				perror("SIOCGIFHWADDR");
		}

		if (sendOrReceive == REC_SOCKET) {

			// Set interface to promiscuous mode
			struct ifreq ifopts;
			memcpy(ifopts.ifr_name, interfaces->ifa_name, IFNAMSIZ-1);
			ioctl(sockfd[*numInterfaces], SIOCGIFFLAGS, &ifopts);
			ifopts.ifr_flags |= IFF_PROMISC;
			ioctl(sockfd[*numInterfaces], SIOCSIFFLAGS, &ifopts);

			// Allow the socket to be reused - incase connection is closed prematurely
			if (setsockopt(sockfd[*numInterfaces], SOL_SOCKET, SO_REUSEADDR, 
				&sockopt[*numInterfaces], sizeof sockopt) == -1) {
					perror("setsockopt");
					close(sockfd[*numInterfaces]);
					exit(EXIT_FAILURE);
			}

			// Bind to device
			if (setsockopt(sockfd[*numInterfaces], SOL_SOCKET, SO_BINDTODEVICE, 
				interfaces->ifa_name, IFNAMSIZ-1) == -1)	{
					perror("SO_BINDTODEVICE");
					close(sockfd[*numInterfaces]);
					exit(EXIT_FAILURE);
			}

			if (sockfd[*numInterfaces] > *maxSockFd)
				*maxSockFd = sockfd[*numInterfaces];
		}

		printf(_("Number %i is interface %s\n"), *numInterfaces, interfaces->ifa_name);
		*numInterfaces += 1;
	}

	return;
}

void flush_all_interfaces (int *sockfd, int maxSockFd, int numInterfaces) {
	
	struct timeval tv = {0, 0};
	
	fd_set readfds;
	int rv;
	
	while (1) {
		//SET_SELECT_FDS
		FD_ZERO(&readfds); 
		for (int x = 0; x < numInterfaces; x++) FD_SET(sockfd[x], &readfds);
		
		rv = select(maxSockFd, &readfds, NULL, NULL, &tv);
puts("neuneuneu");//sleep(1);
		if (rv == -1) perror("select"); // error occurred in select()
		else if (rv == 0) {
			printf("Timeout occurred! All data flushed.\n");
			break;
		}
		else {
			for (int j = 0; j < numInterfaces; j++) {
				if (FD_ISSET(sockfd[j], &readfds)) {
puts("Deleted something");
					recvfrom(sockfd[j], NULL, 1500, 0, NULL, NULL);
				}
			}
		}
	}
	return;
}


