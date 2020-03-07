#include "lifx.h"



LifxPacket::LifxPacket(uint8_t* rawData, int dataLen) {
  if (dataLen >= sizeof(LifxPacketHeader)) {
    LifxPacketHeader header;
    memcpy((void*)&mHeader, rawData, sizeof(LifxPacketHeader));
    mPayloadSize = (dataLen-sizeof(LifxPacketHeader));
    memcpy(mPayload, rawData+sizeof(LifxPacketHeader), mPayloadSize);
    print();
  }
}


LifxPacket::LifxPacket(byte type, byte* data, int dataLen, uint32_t source, uint8_t sequence){
    mHeader.size = sizeof(mHeader) + dataLen;
    mHeader.protocol = LifxProtocol_AllBulbsResponse;
    mHeader.addressable = 0;
    mHeader.tagged = 0;
    mHeader.origin = 1;   // WHAT SHOULD THIS BE
    mHeader.source = source;   // COULD NEED TO CHANGE THIS BASED ON PREVIOUS PACKET
    mHeader.res_required = 0;
    mHeader.ack_required = 0;
    mHeader.sequence = sequence;  // COULD NEED TO CHANGE THIS BASED ON PREVIOUS PACKET
    mHeader.type = type;
    
    for (int i=0; i < 8; ++i) {
        mHeader.target[i] = 0;
    }
    
    for (int i=0; i < dataLen; ++i) {
        mPayload[i] = data[i];
    }
    mPayloadSize = dataLen;
}


void LifxPacket::print() {
    
    // commented post-debugging
    //   Serial1.printf("  header.size = %d\r\n", mHeader.size);
    //   Serial1.printf("  header.protocol = %d\r\n", mHeader.protocol);
    //   Serial1.printf("  header.addressable = %d\r\n", mHeader.addressable);
    //   Serial1.printf("  header.tagged = %d\r\n", mHeader.tagged);
    //   Serial1.printf("  header.origin = %d\r\n", mHeader.origin);
    //   Serial1.printf("  header.source = %d\r\n", mHeader.source);
    //   Serial1.printf("  header.target = ");
    //   for(int i = 0; i < 8; i++) {
    //     Serial1.printf(" %.2X", mHeader.target[i]);
    //   }
    //   Serial1.printf("\r\n");
    //   Serial1.printf("  header.res_required = %d\r\n", mHeader.res_required);
    //   Serial1.printf("  header.ack_required = %d\r\n", mHeader.ack_required);
    //   Serial1.printf("  header.sequence = %d\r\n", mHeader.sequence);
    //   Serial1.printf("  header.type = %X\r\n", mHeader.type);
    //   Serial1.printf("  payload\(%.2d\) = ", mPayloadSize);
    //   for(int i = 0; i < mPayloadSize; i++) {
    //     if (!(i % 8)) { Serial1.printf("\r\n                 "); }  
    //     Serial1.printf(" %.2X", mPayload[i]);
    //   }
    //   Serial1.printf("\r\n");
    //   Serial1.printf("\r\n");
    
}

void LifxPacket::setTarget(byte* mac){
    for(int i=0;i<6;i++) {
        mHeader.target[i] = mac[i];
    }
    
}