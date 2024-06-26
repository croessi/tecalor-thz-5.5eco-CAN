/*
 *  Copyright (C) 2023 Bastian Stahmer
 * 
 *  This file is part of ha-stiebel-control.
 *  ha-stiebel-control is free software: : you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(heatingpump_H)
#define heatingpump_H
#include "ElsterTable.h"
#include "KElsterTable.h"

typedef struct
{
  const char * Name;
  uint32_t CanId;
  uint8_t ReadId[2];
  uint8_t WriteId[2];
  uint8_t ConfirmationId[2];
} CanMember;

/*
#################################################################
#Stiebel Eltron WPL 13 E (2013)
#WPM3i software version 391-08
#FEK software version 416 - 02
#################################################################
#WPL 13 E: CAN ID 180: read - 3100, write - 3000 # Pump
#WPL 13 E: CAN ID 500: read - A100, write - A000 # Heating Module
#WPL 13 E: CAN ID 480: read - 9100, write - 9000 # Manager

#OLD: CAN ID 301: read - 0c01, FEK-device (no active can request, only listening)
#
#OLD: other addresses
#OLD:   180 read: 3100  write: 3000
#OLD:  	301	read: 6101  write: 6001
#OLD: 	480	read: 9100  write: 9000    WMPme Wärmepumpenmanager
#OLD: 	601	read: C101  write: C001
#OLD: 	680	confirmation: D200
#################################################################
*/

static const CanMember CanMembers[] =
{
//  Name              CanId     ReadId          WriteId         ConfirmationID
  { "ESPCLIENT"     , 0x6A2,    {0x00, 0x00},   {0x00, 0x00},   {0xE2, 0x00}}, //The ESP Home Client, thus no valid read/write IDs
  { "KESSEL"        , 0x180,    {0x31, 0x00},   {0x30, 0x00},   {0x00, 0x00}},
  { "MANAGER"       , 0x301,    {0x91, 0x00},   {0x90, 0x00},   {0x00, 0x00}},
  { "HEIZMODUL"     , 0x500,    {0xA1, 0x00},   {0xA0, 0x00},   {0x00, 0x00}}
};


typedef enum
{
  // Die Reihenfolge muss mit CanMembers übereinstimmen!
  cm_espclient = 0,
  cm_kessel,
  cm_manager,       
  cm_heizmodul      
} CanMemberType;

const ElsterIndex *  processCanMessage(unsigned short can_id, std::string &signalValue, std::vector<unsigned char> msg)
{
    // Return if the message is too small
    if(msg.size() < 7) {
        return &ElsterTable[0];
    }

    const ElsterIndex* ei;
    unsigned char byte1;
    unsigned char byte2;
    char charValue[16];
    char charValue_double[16]; //buffer to try different types
    char charValue_little_endian[16]; //buffer to try different types
    char charValue_dec[16]; //buffer to try different types
    char charValue_standard[16]; //buffer to try different types
    char charValue_bool[16]; //buffer to try different types
    

    unsigned short receiver_id; // hex to decode receiver
    unsigned short  register_num; //hex to decode register

    // erster block maskiert mit F0 *8 ist die adresse + zweite stelel des zweiten blocks - limitiert auf drei stellen 
    receiver_id = (((msg[0] & 0xF0) *8)) + (msg[1] & 0x0F);

    if (int(msg[2]) == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        register_num = (unsigned short)((msg[4])+( (msg[3])<<8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        register_num = (unsigned short)(msg[2]);
    }

    ei = GetElsterIndex(register_num);

    //For Debug purposes decode value with different decoders
    SetDoubleType(charValue_double, et_double_val, double(byte2+(byte1<<8)));
    SetValueType(charValue_little_endian, ei->Type, int(byte2+(byte1<<8)));
    SetValueType(charValue_dec, et_dec_val, int(byte2+(byte1<<8)));
    SetValueType(charValue_standard, et_default, int(byte2+(byte1<<8)));
    SetValueType(charValue_bool, et_bool, int(byte2+(byte1<<8)));

    switch(ei->Type){
        case et_double_val:
            SetDoubleType(charValue, ei->Type, double(byte2+(byte1<<8)));
            break;
        case et_triple_val:
            SetDoubleType(charValue, ei->Type, double(byte2+(byte1<<8)));
            break;
        default:
            SetValueType(charValue, ei->Type, int(byte2+(byte1<<8)));
            break;
    }

    // sprintf(logString, "%d;%s;%s;%s", can_id, ei->Name, charValue, ElsterTypeStr[ei->Type]);
    // id(received_can_signal).publish_state(logString);
    
    if (msg[0] & 0x01)
    {
        ESP_LOGI("processCanMessage()", "0x%03x rd 0x%03x\t0x%04x-%s", can_id, receiver_id, register_num, ei->Name);
    }
    else if (msg[0] & 0x02)
        {
            ESP_LOGI("processCanMessage()", "0x%03x wt 0x%03x\t0x%04x-%s:\t%s\t(%s)\thex:0x%04x st:%s le:%s", can_id, receiver_id, register_num, ei->Name, charValue, ElsterTypeStr[ei->Type], (byte1 << 8) | byte2, charValue_standard, charValue_little_endian);
        }
    else
        {
            ESP_LOGI("processCanMessage()", "0x%03x %d 0x%03x\t0x%04x-%s:\t%s\t(%s)\thex:0x%04x st:%s le:%s", can_id, msg[0] & 0x0F, receiver_id, register_num, ei->Name, charValue, ElsterTypeStr[ei->Type], (byte1 << 8) | byte2, charValue_standard, charValue_little_endian);
        }

    signalValue = (std::string)charValue;
    return ei;
}

void update_COP_WW()
{
    id(COP_WW).publish_state((id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / id(EL_AUFNAHMELEISTUNG_WW_SUM).state);
    return;
}
void update_COP_HEIZ()
{
    id(COP_HEIZ).publish_state((id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state) / id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state);
    return;
}
void update_COP_GESAMT()
{
    id(COP_GESAMT).publish_state((id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state + id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / (id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state + id(EL_AUFNAHMELEISTUNG_WW_SUM).state));             
    return;
}

void readSignal(const CanMember * member, const ElsterIndex * ei) {
    bool use_extended_id = 0; //No use of extended ID
    uint8_t IndexByte1 = (uint8_t) (ei->Index >> 8);
    uint8_t IndexByte2 = (uint8_t) (ei->Index - ((ei->Index >> 8) << 8));
    std::vector< uint8_t > data;

    if(IndexByte1 == 0x00) {
        data.insert(data.end(), {
            member->ReadId[0],
            member->ReadId[1],
            IndexByte2,
            0x00,
            0x00,
            0x00,
            0x00
        });
    } else {
        data.insert(data.end(), {
            member->ReadId[0],
            member->ReadId[1],
            0xfa,
            IndexByte1,
            IndexByte2,
            0x00,
            0x00
        });
    }

    char logmsg[220];
    sprintf(logmsg, "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, member->Name, member->CanId, member->ReadId[0], member->ReadId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("readSignal()", "%s", logmsg);
    
    id(my_mcp2515).send_data(CanMembers[cm_espclient].CanId, use_extended_id, data);
    
    return;
}

void writeSignal(const CanMember * member, const ElsterIndex * ei, const char * & str) {
    bool use_extended_id = 0; //No use of extended ID
    int writeValue = TranslateString(str, ei->Type);
    uint8_t IndexByte1 = (uint8_t) (ei->Index >> 8);
    uint8_t IndexByte2 = (uint8_t) (ei->Index - ((ei->Index >> 8) << 8));
    std::vector< uint8_t > data;

    if(IndexByte1 == 0x00) {
        data.insert(data.end(), {
            member->WriteId[0],
            member->WriteId[1],
            IndexByte2,
            ((uint8_t)(writeValue>>8)),
            ((uint8_t)(writeValue-((writeValue>>8)<<8))),
            0x00,
            0x00
        });
    } else {
        data.insert(data.end(), {
            member->WriteId[0],
            member->WriteId[1],
            0xfa,
            IndexByte1,
            IndexByte2,
            ((uint8_t)(writeValue>>8)),
            ((uint8_t)(writeValue-((writeValue>>8)<<8)))
        });
    }

    char logmsg[220];
    sprintf(logmsg, "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, writeValue, member->Name, member->CanId, member->ReadId[0], member->ReadId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);
    
    id(my_mcp2515).send_data(CanMembers[cm_espclient].CanId, use_extended_id, data);
    
    return;
}

/*
void publishDate()
{
    int ijahr = (int)id(JAHR).state;
    std::string jahr;
    if(ijahr >= 0 & ijahr < 99)
    {
        if(ijahr < 10)
            jahr = "0" + to_string(ijahr);
        else
            jahr = to_string(ijahr);
    }
    else
    {
        jahr = "00";
    }

    int imonat = (int)id(MONAT).state;
    std::string monat;
    if(imonat > 0 & imonat <= 12)
    {
        if(imonat < 10)
            monat = "0" + to_string(imonat);
        else
            monat = to_string(imonat);
    }
    else
    {
        monat = "00";
    }

    int itag = (int)id(TAG).state;
    std::string tag;
    if(itag > 0 & itag <= 31)
    {
        if(itag < 10)
            tag = "0" + to_string(itag);
        else
            tag = to_string(itag);
    }
    else
    {
        tag = "00";
    }

    id(DATUM).publish_state("20" + jahr + "-" + monat + "-" + tag);

    return;

    //              id(DATUM).publish_state("20" + to_string((int)id(JAHR).state) + "-" + to_string((int)id(MONAT).state) + "-" + to_string((int)id(TAG).state));
}

void publishTime()
{
    int istunde = (int)id(STUNDE).state;
    std::string stunde;
    if(istunde >= 0 & istunde < 60)
    {
        if(istunde < 10)
            stunde = "0" + to_string(istunde);
        else
            stunde = to_string(istunde);
    }
    else
    {
        stunde = "00";
    }

    int iminute = (int)id(MINUTE).state;
    std::string minute;
    if(iminute >= 0 & iminute < 60)
    {
        if(iminute < 10)
            minute = "0" + to_string(iminute);
        else
            minute = to_string(iminute);
    }
    else 
    {
        minute = "00";
    }

    int isekunde = (int)id(SEKUNDE).state;
    std::string sekunde;
    if(isekunde >= 0 & isekunde < 60)
    {
        if(isekunde < 10)
            sekunde = "0" + to_string(isekunde);
        else
            sekunde = to_string(isekunde);
    }
    else
    {
        sekunde = "00";
    }

    id(ZEIT).publish_state(stunde + ":" + minute + ":" + sekunde);
    return;

    //              id(DATUM).publish_state("20" + to_string((int)id(JAHR).state) + "-" + to_string((int)id(MONAT).state) + "-" + to_string((int)id(TAG).state));
}
*/
#endif