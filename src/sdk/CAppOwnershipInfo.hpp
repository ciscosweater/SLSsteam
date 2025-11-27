#pragma once

#include <cstdint>

class CAppOwnershipInfo
{
public:
    int32_t subId; //-1 for none
    int releaseState;
    uint32_t ownerSteamId;
    int field3_0xc;
    uint32_t field4_0x10;
    int field5_0x14;
    int field6_0x18;
    uint32_t field7_0x1c;
    uint32_t field8_0x20;
    bool purchased; //Enables play button
    char field10_0x25;
    bool permanent;
    char field12_0x27;
    char field13_0x28;
    bool field14_0x29;
    bool fromFreeWeekend; //Guesstimate (from debug string)
    char field16_0x2b;
    char field17_0x2c;
    char field18_0x2d;
    char field19_0x2e;
    char field20_0x2f;
    char field21_0x30;
    bool field22_0x31;
    char field23_0x32;
    char field24_0x33;
    char field25_0x34;
    bool familyShared;
    bool field27_0x36;
    char field28_0x37;
    //char field29_0x38;
}; //0x38
