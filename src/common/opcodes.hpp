#pragma once

#include <cstdint>

enum Opcode : uint8_t {
    // === Core ===
    SID_NULL               = 0x00,
    SID_STOPADV            = 0x02,
    SID_CLIENTID           = 0x05,
    SID_STARTADVEX3        = 0x08,
    SID_GETADVLISTEX       = 0x09,
    SID_ENTERCHAT          = 0x0A, // (он же SID_JOINCHANNEL) Если пришло от клиента — значит, это Join Channel. Если шлёт сервер — это Enter Chat.
    SID_GETCHANNELLIST     = 0x0B,

    SID_CHATCOMMAND        = 0x0C,
    SID_CHATEVENT          = 0x0F,
    SID_LEAVECHAT          = 0x10,
    SID_LOCALEINFO         = 0x12,
    SID_FLOODDETECTED      = 0x13,

    // === Authentication ===
    SID_AUTH_INFO          = 0x50,
    SID_AUTH_CHECK         = 0x51,
    SID_AUTH_ACCOUNTCREATE = 0x52,
    SID_LOGON_CHALLENGE    = 0x53,
    SID_LOGON_PROOF        = 0x54,
    SID_LOGON_RESULT       = 0x55,
    SID_ACCOUNTLOGON       = 0x56,
    SID_ACCOUNTLOGONPROOF  = 0x57,
    SID_ACCOUNTCREATE      = 0x58,
    SID_ACCOUNTCREATEREPLY = 0x59,
    SID_ACCOUNTCHANGEPASS  = 0x5A,
    SID_ACCOUNTCHANGEPROOF = 0x5B,
    SID_ACCOUNTUPGRADE     = 0x5C,

    // === Ping / Keepalive ===
    SID_PING               = 0x25,
    SID_PONG               = 0x26,

    // === Friends ===
    SID_FRIENDSLIST        = 0x65,
    SID_FRIENDSADD         = 0x66,
    SID_FRIENDSREMOVE      = 0x67,
    SID_FRIENDSPOSITION    = 0x68,
    SID_FRIENDSUPDATE      = 0x69,
    SID_FRIENDSUPDATEEX    = 0x6A,

    // === Clan ===
    SID_CLANFINDCANDIDATES = 0x70,
    SID_CLANINVITEMEMBER   = 0x71,
    SID_CLANREMOVEMEMBER   = 0x72,
    SID_CLANINVITATION     = 0x73,
    SID_CLANRANKCHANGE     = 0x74,
    SID_CLANQUITNOTIFY     = 0x75,
    SID_CLANDISBAND        = 0x76,
    SID_CLANMAKECHIEFTAIN  = 0x77,
    SID_CLANINFO           = 0x78,
    SID_CLANMEMBERLIST     = 0x79,
    SID_CLANMEMBERREMOVED  = 0x7A,
    SID_CLANMEMBERSTATUSCHANGE = 0x7B,

    // === Warcraft III hosting ===
    W3GS_PING_FROM_HOST    = 0x19,
    W3GS_SLOTINFOJOIN      = 0x1E,
    W3GS_SLOTINFO          = 0x1B,
    W3GS_CREATEGAME        = 0x01,
    W3GS_REFRESHGAME       = 0x02,
    W3GS_SEARCHGAME        = 0x09,
    W3GS_PLAYERINFO        = 0x05,
    W3GS_REQJOIN           = 0x0E,
    W3GS_REJECTJOIN        = 0x0F,
    W3GS_STARTGAME         = 0x17,
    W3GS_INCOMING_ACTION   = 0x1C,
    W3GS_OUTGOING_ACTION   = 0x1D,

    SID_UNKNOWN            = 0xFE
};
