#pragma once

#include <cstdint>

enum class AuthProofCode : uint8_t {
    SUCCESS           = 0x00, // OK — клиент авторизован
    FAIL              = 0x01, // Неверный пароль / неизвестный пользователь
    ACCOUNT_DISABLED  = 0x02, // Учётка отключена (редко используется)
    ACCOUNT_LOCKED    = 0x03, // Учётка заблокирована
    BAD_VERSION       = 0x04, // Неправильная версия игры
    BANNED            = 0x05, // Забанен
    OLD_GAME_VERSION  = 0x06, // Слишком старая версия клиента
    CUSTOM_FAIL       = 0x0E  // Пользовательская ошибка PvPGN-style
};
